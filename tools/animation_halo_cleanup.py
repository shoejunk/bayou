#!/usr/bin/env python3
"""Audit and repair white edge contamination in generated animation PNGs.

The cleanup is deliberately frame-local: colors never propagate across sprite
sheet cells, alpha and dimensions are preserved, and RGB is zeroed wherever
alpha is zero. The repair targets bright, low-saturation boundary pixels only;
opaque interior artwork and legitimate white details away from the silhouette
are left alone.
"""

from __future__ import annotations

import argparse
import concurrent.futures
import csv
import hashlib
import json
import shutil
from dataclasses import asdict, dataclass
from datetime import datetime
from pathlib import Path

import numpy as np
from PIL import Image, ImageDraw, ImageFont, PngImagePlugin


ALPHA_CUT = 10
FRAME_SIZES = (256, 128)
CLEANUP_VERSION = "bayou-animation-halo-v2"
ALPHA_CLEANUP_VERSION = "bayou-transparent-rgb-v1"
BACKGROUNDS = (
    (0, 0, 0, 255),
    (34, 16, 57, 255),
    (32, 220, 64, 255),
    (112, 112, 112, 255),
)


@dataclass
class Layout:
    frame_width: int
    frame_height: int
    columns: int
    rows: int

    @property
    def frame_count(self) -> int:
        return self.columns * self.rows


@dataclass
class Stats:
    file: str
    width: int = 0
    height: int = 0
    mode_before: str = ""
    cleanup_marker_before: str = ""
    cleanup_marker_after: str = ""
    frame_width: int = 0
    frame_height: int = 0
    columns: int = 0
    rows: int = 0
    frames: int = 0
    suspicious_before: int = 0
    suspicious_after: int = 0
    pixels_changed: int = 0
    repair_passes: int = 0
    zero_alpha_rgb_fixed: int = 0
    alpha_changed: int = 0
    opaque_interior_changed: int = 0
    dimensions_preserved: bool = False
    reopened: bool = False
    idempotent_delta: int = 0
    repaired: bool = False
    backup: str = ""
    skipped: bool = False
    skip_reason: str = ""


def parse_args() -> argparse.Namespace:
    parser = argparse.ArgumentParser(description=__doc__)
    parser.add_argument("--root", type=Path, default=Path(__file__).resolve().parents[1])
    parser.add_argument("--mode", choices=("scan", "repair", "validate"), default="scan")
    parser.add_argument("--include", action="append", default=[], help="Filename glob; may be repeated")
    parser.add_argument("--targets-file", type=Path, help="Text file containing filename globs")
    parser.add_argument("--report", type=Path)
    parser.add_argument("--preview-dir", type=Path)
    parser.add_argument("--preview-limit", type=int, default=16)
    parser.add_argument("--no-previews", action="store_true")
    parser.add_argument("--sample", action="store_true", help="Limit processing to eight matched files")
    parser.add_argument("--jobs", type=int, default=1, help="Number of files to process concurrently")
    parser.add_argument("--stamp-only", action="store_true", help=argparse.SUPPRESS)
    parser.add_argument("--basic", action="store_true", help="Validate structure without halo analysis")
    parser.add_argument("--transparent-only", action="store_true", help="Only zero RGB where alpha is zero")
    parser.add_argument("--aggressive-edge", action="store_true", help=argparse.SUPPRESS)
    return parser.parse_args()


def infer_layout(width: int, height: int) -> Layout | None:
    candidates: list[Layout] = []
    for size in FRAME_SIZES:
        if width % size == 0 and height % size == 0:
            columns = width // size
            rows = height // size
            if columns * rows >= 1:
                candidates.append(Layout(size, size, columns, rows))
    if not candidates:
        return None
    candidates.sort(
        key=lambda item: (
            item.frame_width != 256,
            item.rows > 1 and item.columns > 1,
            -item.frame_count,
        )
    )
    return candidates[0]


def frame_regions(layout: Layout):
    for row in range(layout.rows):
        for column in range(layout.columns):
            left = column * layout.frame_width
            top = row * layout.frame_height
            yield left, top, left + layout.frame_width, top + layout.frame_height


def zero_transparent_rgb(array: np.ndarray) -> tuple[np.ndarray, int]:
    out = array.copy()
    transparent = out[..., 3] == 0
    dirty = transparent & np.any(out[..., :3] != 0, axis=2)
    count = int(np.count_nonzero(dirty))
    out[transparent, :3] = 0
    return out, count


def eroded(mask: np.ndarray, radius: int) -> np.ndarray:
    kernel = radius * 2 + 1
    padded = np.pad(mask.astype(np.uint8), radius, mode="constant")
    integral = np.pad(padded, ((1, 0), (1, 0)), mode="constant").cumsum(0).cumsum(1)
    totals = (
        integral[kernel:, kernel:]
        - integral[:-kernel, kernel:]
        - integral[kernel:, :-kernel]
        + integral[:-kernel, :-kernel]
    )
    return totals == kernel * kernel


def window_sum(values: np.ndarray, radius: int) -> np.ndarray:
    """Return square-neighborhood sums for a 2D or channel-last array."""
    kernel = radius * 2 + 1
    if values.ndim == 2:
        padding = ((radius, radius), (radius, radius))
        leading = ((1, 0), (1, 0))
    else:
        padding = ((radius, radius), (radius, radius), (0, 0))
        leading = ((1, 0), (1, 0), (0, 0))
    padded = np.pad(values, padding, mode="constant")
    integral = np.pad(padded, leading, mode="constant").cumsum(0).cumsum(1)
    return (
        integral[kernel:, kernel:]
        - integral[:-kernel, kernel:]
        - integral[kernel:, :-kernel]
        + integral[:-kernel, :-kernel]
    )


def component_labels(mask: np.ndarray) -> tuple[np.ndarray, list[int]]:
    """Label 8-connected visible regions so effects cannot borrow body colors."""
    height, width = mask.shape
    labels = np.zeros((height, width), dtype=np.int32)
    parent = [0]

    def find(value: int) -> int:
        root = value
        while parent[root] != root:
            root = parent[root]
        while parent[value] != value:
            next_value = parent[value]
            parent[value] = root
            value = next_value
        return root

    for y, x in zip(*np.nonzero(mask)):
        neighbors: list[int] = []
        if x > 0 and labels[y, x - 1]:
            neighbors.append(int(labels[y, x - 1]))
        if y > 0:
            for next_x in range(max(0, int(x) - 1), min(width, int(x) + 2)):
                value = int(labels[y - 1, next_x])
                if value:
                    neighbors.append(value)
        if not neighbors:
            value = len(parent)
            parent.append(value)
            labels[y, x] = value
            continue
        roots = [find(value) for value in neighbors]
        root = min(roots)
        labels[y, x] = root
        for value in roots:
            parent[value] = root

    lookup = np.arange(len(parent), dtype=np.int32)
    for value in range(1, len(parent)):
        lookup[value] = find(value)
    labels = lookup[labels]
    unique = np.unique(labels)
    compact = np.zeros(int(unique.max()) + 1, dtype=np.int32)
    compact[unique] = np.arange(len(unique), dtype=np.int32)
    labels = compact[labels].astype(np.int16)
    sizes = np.bincount(labels.ravel()).tolist()
    return labels, sizes


def boundary_candidates(
    frame: np.ndarray,
    components: tuple[np.ndarray, list[int]] | None = None,
) -> tuple[np.ndarray, np.ndarray]:
    """Return conservative bright-neutral halo candidates and their source colors."""
    alpha = frame[..., 3]
    visible = alpha > ALPHA_CUT
    if not np.any(visible):
        return np.zeros_like(visible), np.zeros((*visible.shape, 3), dtype=np.float32)

    inner_four = eroded(visible, 4)
    inner_five = eroded(visible, 5)
    edge_band = visible & ~inner_four
    trusted = inner_five & (alpha > 100)
    if not np.any(trusted):
        trusted = eroded(visible, 2) & (alpha > 100)
    if not np.any(trusted):
        return np.zeros_like(visible), np.zeros((*visible.shape, 3), dtype=np.float32)

    rgb = frame[..., :3].astype(np.float64)
    brightness = np.mean(rgb, axis=2)
    spread = np.max(rgb, axis=2) - np.min(rgb, axis=2)
    gate = edge_band & (brightness > 62) & (spread < 92)
    references = np.zeros((*visible.shape, 3), dtype=np.float64)
    found = np.zeros_like(visible)
    labels, sizes = components or component_labels(visible)
    for label in range(1, len(sizes)):
        if sizes[label] < 220:
            continue
        component = labels == label
        component_gate = gate & component
        component_trusted = trusted & component
        if not np.any(component_gate) or not np.any(component_trusted):
            continue
        trusted_float = component_trusted.astype(np.float64)
        trusted_rgb = rgb * trusted_float[..., None]
        for radius in (5, 8, 12, 20, 32):
            counts = window_sum(trusted_float, radius)
            choose = component_gate & ~found & (counts > 0)
            if not np.any(choose):
                continue
            sums = window_sum(trusted_rgb, radius)
            references[choose] = sums[choose] / counts[choose][:, None]
            found[choose] = True

    reference_brightness = np.mean(references, axis=2)
    reference_spread = np.max(references, axis=2) - np.min(references, axis=2)
    pixel_whiteness = brightness - spread * 0.55
    reference_whiteness = reference_brightness - reference_spread * 0.55
    color_distance = np.linalg.norm(rgb - references, axis=2)
    candidates = (
        gate
        & found
        & (brightness > reference_brightness + 8)
        & (pixel_whiteness > reference_whiteness + 10)
        & (color_distance > 18)
        & ((alpha < 250) | (brightness > 88) | (spread < 62))
    )
    return candidates, references.astype(np.float32)


def residual_candidates(
    frame: np.ndarray,
    components: tuple[np.ndarray, list[int]] | None = None,
) -> tuple[np.ndarray, np.ndarray]:
    alpha = frame[..., 3]
    visible = alpha > ALPHA_CUT
    if not np.any(visible):
        return np.zeros_like(visible), np.zeros((*visible.shape, 3), dtype=np.float32)
    inner_four = eroded(visible, 4)
    inner_eight = eroded(visible, 8)
    band = inner_four & ~inner_eight
    rgb = frame[..., :3].astype(np.float64)
    brightness = np.mean(rgb, axis=2)
    spread = np.max(rgb, axis=2) - np.min(rgb, axis=2)
    # Very bright neutral pixels inside the silhouette are commonly eyes,
    # metal glints, flowers, or magic. Outer boundary contamination is handled
    # above; this deeper pass stays below that highlight range.
    gate = band & (brightness > 78) & (brightness < 185) & (spread < 62)
    references = np.zeros((*visible.shape, 3), dtype=np.float64)
    found = np.zeros_like(visible)
    labels, sizes = components or component_labels(visible)
    for label in range(1, len(sizes)):
        if sizes[label] < 220:
            continue
        component = labels == label
        component_gate = gate & component
        source = component & (alpha > 80) & ~gate
        if not np.any(component_gate) or not np.any(source):
            continue
        source_float = source.astype(np.float64)
        source_rgb = rgb * source_float[..., None]
        for radius in (1, 2, 3, 4):
            counts = window_sum(source_float, radius)
            sums = window_sum(source_rgb, radius)
            local_brightness = np.zeros_like(brightness)
            has_color = counts > 0
            local_brightness[has_color] = np.mean(sums[has_color] / counts[has_color][:, None], axis=1)
            choose = component_gate & ~found & (counts >= 4) & (local_brightness < brightness - 8)
            if np.any(choose):
                references[choose] = sums[choose] / counts[choose][:, None]
                found[choose] = True
    return found, references.astype(np.float32)


def suspicious_count(frame: np.ndarray) -> int:
    components = component_labels(frame[..., 3] > ALPHA_CUT)
    boundary, _replacements = boundary_candidates(frame, components)
    residual, _residual_replacements = residual_candidates(frame, components)
    return int(np.count_nonzero(boundary | residual))


def repair_frame(frame: np.ndarray) -> tuple[np.ndarray, int, int]:
    out, zero_fixed = zero_transparent_rgb(frame)
    components = component_labels(out[..., 3] > ALPHA_CUT)
    boundary, replacements = boundary_candidates(out, components)
    out[boundary, :3] = np.clip(replacements[boundary] * 0.82, 0, 255).astype(np.uint8)

    # The successful Widowroot pass also cleaned a small neutral band just
    # inside the outer silhouette. Restrict this to pixels with nearby darker
    # visible neighbors so white clothing, eyes, and effects remain intact.
    residual, residual_replacements = residual_candidates(out, components)
    out[residual, :3] = np.clip(residual_replacements[residual] * 0.78, 0, 255).astype(np.uint8)

    out, extra_zero_fixed = zero_transparent_rgb(out)
    changed = int(np.count_nonzero(np.any(out != frame, axis=2)))
    return out, changed, zero_fixed + extra_zero_fixed


def process_array(array: np.ndarray, layout: Layout, repair: bool) -> tuple[np.ndarray, int, int, int]:
    output = array.copy()
    suspicious = 0
    changed = 0
    zero_fixed = 0
    for left, top, right, bottom in frame_regions(layout):
        frame = array[top:bottom, left:right]
        suspicious += suspicious_count(frame)
        if repair:
            repaired, frame_changed, frame_zero_fixed = repair_frame(frame)
            output[top:bottom, left:right] = repaired
            changed += frame_changed
            zero_fixed += frame_zero_fixed
    return output, suspicious, changed, zero_fixed


def load_patterns(args: argparse.Namespace) -> list[str]:
    patterns = list(args.include)
    if args.targets_file:
        for line in args.targets_file.read_text(encoding="utf-8").splitlines():
            value = line.strip()
            if value and not value.startswith("#"):
                patterns.append(value)
    return patterns


def find_files(root: Path, patterns: list[str]) -> list[Path]:
    animation_root = root / "assets" / "animations"
    files = sorted(animation_root.rglob("*.png"), key=lambda item: item.name.lower())
    if not patterns:
        return files
    matched: set[Path] = set()
    for pattern in patterns:
        if not any(token in pattern for token in "*?["):
            pattern = pattern if pattern.lower().endswith(".png") else f"*{pattern}*.png"
        matched.update(animation_root.glob(pattern))
    return sorted((path for path in matched if path.is_file()), key=lambda item: item.name.lower())


def sha256(path: Path) -> str:
    digest = hashlib.sha256()
    with path.open("rb") as stream:
        for block in iter(lambda: stream.read(1024 * 1024), b""):
            digest.update(block)
    return digest.hexdigest()


def save_rgba(path: Path, array: np.ndarray, mark_halo: bool = True) -> None:
    metadata = PngImagePlugin.PngInfo()
    metadata.add_text("bayou_alpha_cleanup", ALPHA_CLEANUP_VERSION)
    if mark_halo:
        metadata.add_text("bayou_halo_cleanup", CLEANUP_VERSION)
    Image.fromarray(array.astype(np.uint8), "RGBA").save(
        path,
        format="PNG",
        optimize=False,
        pnginfo=metadata,
    )


def make_preview(path: Path, before: np.ndarray, after: np.ndarray, layout: Layout) -> None:
    path.parent.mkdir(parents=True, exist_ok=True)
    frame_index = min(layout.frame_count - 1, layout.frame_count // 2)
    region = list(frame_regions(layout))[frame_index]
    left, top, right, bottom = region
    source_frames = (before[top:bottom, left:right], after[top:bottom, left:right])
    tile_size = 192
    label_height = 24
    preview = Image.new("RGB", (tile_size * 4, (tile_size + label_height) * 2), (24, 24, 24))
    draw = ImageDraw.Draw(preview)
    font = ImageFont.load_default()
    for row, (label, frame) in enumerate(zip(("BEFORE", "AFTER"), source_frames)):
        sprite = Image.fromarray(frame, "RGBA").resize((tile_size, tile_size), Image.Resampling.NEAREST)
        y = row * (tile_size + label_height)
        draw.text((5, y + 5), f"{label}  frame {frame_index + 1}", fill=(255, 255, 255), font=font)
        for column, background in enumerate(BACKGROUNDS):
            canvas = Image.new("RGBA", (tile_size, tile_size), background)
            canvas.alpha_composite(sprite)
            preview.paste(canvas.convert("RGB"), (column * tile_size, y + label_height))
    preview.save(path)


def write_report(path: Path, stats: list[Stats]) -> None:
    path.parent.mkdir(parents=True, exist_ok=True)
    fields = list(asdict(stats[0]).keys()) if stats else list(Stats(file="").__dict__.keys())
    with path.open("w", newline="", encoding="utf-8") as stream:
        writer = csv.DictWriter(stream, fieldnames=fields)
        writer.writeheader()
        for item in stats:
            writer.writerow(asdict(item))


def process_file(
    path: Path,
    root: Path,
    mode: str,
    backup_root: Path,
    preview_dir: Path | None,
    make_diagnostic: bool,
    stamp_only: bool,
    basic: bool,
    transparent_only: bool,
) -> Stats:
    relative = path.relative_to(root).as_posix()
    stat = Stats(file=relative)
    try:
        with Image.open(path) as opened:
            stat.mode_before = opened.mode
            stat.cleanup_marker_before = str(opened.info.get("bayou_halo_cleanup", ""))
            rgba = opened.convert("RGBA")
            original_size = rgba.size
            original = np.asarray(rgba).copy()
    except Exception as error:  # pragma: no cover - report malformed assets
        stat.skipped = True
        stat.skip_reason = f"open failed: {error}"
        return stat

    stat.width, stat.height = original_size
    layout = infer_layout(*original_size)
    if layout is None:
        stat.skipped = True
        stat.skip_reason = "could not safely infer 128x128 or 256x256 frame regions"
        return stat
    stat.frame_width = layout.frame_width
    stat.frame_height = layout.frame_height
    stat.columns = layout.columns
    stat.rows = layout.rows
    stat.frames = layout.frame_count

    already_cleaned = stat.cleanup_marker_before == CLEANUP_VERSION
    should_repair = mode == "repair" and not already_cleaned and not stamp_only and not transparent_only
    needs_analysis = not basic and (
        mode == "scan" or should_repair or (mode == "validate" and not already_cleaned)
    )
    if transparent_only:
        output, stat.zero_alpha_rgb_fixed = zero_transparent_rgb(original)
    elif needs_analysis:
        output, stat.suspicious_before, _first_changed, first_zero_fixed = process_array(
            original, layout, should_repair
        )
        stat.zero_alpha_rgb_fixed = first_zero_fixed
    else:
        output = original.copy()
        _unused_clean, stat.zero_alpha_rgb_fixed = zero_transparent_rgb(original)
    if should_repair:
        stat.repair_passes = 1
        # The proven Widowroot cleanup converged after a second selective pass.
        # Running that pass here leaves only a negligible idempotence delta and
        # avoids users needing to invoke the same repair repeatedly.
        output, _second_before, _second_changed, second_zero_fixed = process_array(output, layout, True)
        stat.zero_alpha_rgb_fixed += second_zero_fixed
        stat.repair_passes = 2
    stat.pixels_changed = int(np.count_nonzero(np.any(output != original, axis=2)))
    if should_repair:
        _checked, stat.suspicious_after, _unused_changed, _unused_zero = process_array(output, layout, False)
    else:
        stat.suspicious_after = stat.suspicious_before

    alpha = original[..., 3]
    stat.alpha_changed = int(np.count_nonzero(output[..., 3] != alpha))
    if stat.pixels_changed > 0:
        interior = np.zeros(alpha.shape, dtype=bool)
        for left, top, right, bottom in frame_regions(layout):
            visible = alpha[top:bottom, left:right] > ALPHA_CUT
            interior[top:bottom, left:right] = eroded(visible, 8)
        opaque_interior = interior & (alpha == 255)
        stat.opaque_interior_changed = int(
            np.count_nonzero(np.any(output[..., :3] != original[..., :3], axis=2) & opaque_interior)
        )

    should_write = mode == "repair" and (
        (should_repair and stat.pixels_changed > 0)
        or (stamp_only and not already_cleaned)
        or (transparent_only and stat.pixels_changed > 0)
    )
    if should_write:
        backup = backup_root / relative
        backup.parent.mkdir(parents=True, exist_ok=True)
        shutil.copy2(path, backup)
        stat.backup = backup.relative_to(root).as_posix()
        save_rgba(path, output, mark_halo=not transparent_only)
        stat.repaired = True
        stat.cleanup_marker_after = CLEANUP_VERSION if not transparent_only else stat.cleanup_marker_before
    else:
        stat.cleanup_marker_after = stat.cleanup_marker_before

    if make_diagnostic and preview_dir is not None:
        make_preview(preview_dir / f"{path.stem}-diagnostic.png", original, output, layout)

    try:
        with Image.open(path if stat.repaired else path) as reopened:
            reopened.load()
            stat.reopened = reopened.mode == "RGBA"
            stat.dimensions_preserved = reopened.size == original_size
    except Exception:
        stat.reopened = False
        stat.dimensions_preserved = False

    # Measure idempotence after an actual repair. Validation mode performs the
    # same check against the current file without writing it.
    if mode == "validate" and not already_cleaned and not basic:
        second, _second_suspicious, _second_changed, _second_zero = process_array(output, layout, True)
        stat.idempotent_delta = int(np.count_nonzero(np.any(second != output, axis=2)))
    return stat


def process_file_job(payload: tuple[str, str, str, str, str | None, bool, bool, bool, bool]) -> Stats:
    path, root, mode, backup_root, preview_dir, make_diagnostic, stamp_only, basic, transparent_only = payload
    return process_file(
        Path(path),
        Path(root),
        mode,
        Path(backup_root),
        Path(preview_dir) if preview_dir else None,
        make_diagnostic,
        stamp_only,
        basic,
        transparent_only,
    )


def main() -> int:
    args = parse_args()
    root = args.root.resolve()
    timestamp = datetime.now().strftime("%Y%m%d-%H%M%S")
    report = args.report or root / "tools" / f"animation-halo-{args.mode}-{timestamp}.csv"
    preview_dir = None if args.no_previews else (args.preview_dir or root / "tools" / f"animation-halo-previews-{timestamp}")
    backup_root = root / "tools" / "animation-halo-backups" / timestamp
    patterns = load_patterns(args)
    if args.mode == "repair" and not patterns:
        default_targets = root / "tools" / "animation-halo-visual-targets.txt"
        if default_targets.exists():
            args.targets_file = default_targets
            patterns = load_patterns(args)
    files = find_files(root, patterns)
    if args.sample:
        files = files[:8]
    stats: list[Stats] = []

    payloads = [
        (
            str(path),
            str(root),
            args.mode,
            str(backup_root),
            str(preview_dir) if preview_dir else None,
            index < args.preview_limit,
            args.stamp_only,
            args.basic,
            args.transparent_only,
        )
        for index, path in enumerate(files)
    ]
    if args.jobs > 1:
        with concurrent.futures.ProcessPoolExecutor(max_workers=args.jobs) as executor:
            results = executor.map(process_file_job, payloads)
            for stat in results:
                stats.append(stat)
    else:
        for payload in payloads:
            stats.append(process_file_job(payload))

    for stat in stats:
        status = "SKIP" if stat.skipped else ("REPAIRED" if stat.repaired else "checked")
        print(
            f"{status:8} {stat.file}: suspicious {stat.suspicious_before}->{stat.suspicious_after}, "
            f"changed {stat.pixels_changed}, idempotent delta {stat.idempotent_delta}"
        )

    write_report(report, stats)
    manifest = {
        "created": datetime.now().isoformat(timespec="seconds"),
        "mode": args.mode,
        "report": str(report),
        "files": [
            {"file": item.file, "sha256": sha256(root / item.file), "repaired": item.repaired}
            for item in stats
            if not item.skipped
        ],
    }
    manifest_path = report.with_suffix(".json")
    manifest_path.write_text(json.dumps(manifest, indent=2), encoding="utf-8")

    failures = [
        item
        for item in stats
        if not item.skipped
        and (
            not item.dimensions_preserved
            or not item.reopened
            or item.alpha_changed != 0
            or item.opaque_interior_changed != 0
            or (args.mode == "validate" and item.zero_alpha_rgb_fixed != 0)
        )
    ]
    print(f"Report: {report}")
    if args.mode == "repair":
        print(f"Backups: {backup_root}")
    if preview_dir:
        print(f"Previews: {preview_dir}")
    print(f"Inspected: {len(stats)}; repaired: {sum(item.repaired for item in stats)}; skipped: {sum(item.skipped for item in stats)}")
    return 2 if failures else 0


if __name__ == "__main__":
    raise SystemExit(main())
