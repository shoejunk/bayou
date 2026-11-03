#!/usr/bin/env python3
"""Normalize character animation sheets to their resting token sprites.

Every animation frame is a square cell in a single horizontal PNG sheet. The
tool measures the first frame against the matching image in assets/characters,
then applies one scale and ground alignment to the entire sheet. Oversized and
undersized animations are both corrected. When aligned art would cross a cell
edge, only that frame's translation is clamped so the scale, authored motion,
and full silhouette are preserved. When scale-up cannot fit in the original
256-pixel cell, the sheet receives a larger transparent square cell while the
game continues treating 256 pixels as its logical rendering size.

The scale measurement uses alpha-weighted silhouette area rather than only the
outer bounding box. That keeps a raised weapon, wing, spell wisp, or fallen pose
from making the character body artificially small. Placement uses the
silhouette's horizontal median and lower alpha percentile as a stable ground
anchor.
"""

from __future__ import annotations

import argparse
import csv
import math
import re
from dataclasses import asdict, dataclass
from pathlib import Path

import numpy as np
from PIL import Image, ImageDraw, ImageFont, PngImagePlugin


ALPHA_THRESHOLD = 10
GROUND_QUANTILE = 0.995
NORMALIZATION_VERSION = "bayou-animation-size-v4"
LOGICAL_FRAME_SIZE = 256
LOGICAL_FRAME_KEY = "bayou.animation_logical_frame_size"
NORMALIZATION_MATCH_TOLERANCE = 0.01
SHEET_SUFFIX = re.compile(
    r"-(?:attack\d*|damaged|fidget|idle|killed|walk|aim|lower-?weapon)$",
    re.IGNORECASE,
)
REFERENCE_ALIASES = {
    "crystalunicorn": "crystallineUnicorn",
    "dark-fairy": "gloomFairy",
    "elliotgreentide": "GhostOfElliotGreentide",
    "rowan": "rowanLeafbound",
    "vaelorin": "vaelthorn",
}


@dataclass(frozen=True)
class Silhouette:
    left: int
    top: int
    right: int
    bottom: int
    mass_fraction: float
    anchor_x: float
    anchor_y: float

    @property
    def width(self) -> int:
        return self.right - self.left

    @property
    def height(self) -> int:
        return self.bottom - self.top


@dataclass
class SheetStats:
    sheet: str
    reference: str = ""
    frames: int = 0
    frame_size: int = 0
    logical_frame_size: int = LOGICAL_FRAME_SIZE
    output_frame_size: int = 0
    frame_margin: int = 0
    reference_mass: float = 0.0
    source_mass: float = 0.0
    requested_scale: float = 1.0
    fit_scale_limit: float = 1.0
    fit_limited: bool = False
    scale: float = 1.0
    offset_x: int = 0
    offset_y: int = 0
    reference_width: float = 0.0
    reference_height: float = 0.0
    source_width: float = 0.0
    source_height: float = 0.0
    transformed_width: float = 0.0
    transformed_height: float = 0.0
    max_overflow_x: float = 0.0
    max_overflow_y: float = 0.0
    changed: bool = False
    marker_before: str = ""
    marker_after: str = ""
    skipped: bool = False
    skip_reason: str = ""


def parse_args() -> argparse.Namespace:
    parser = argparse.ArgumentParser(description=__doc__)
    parser.add_argument("--root", type=Path, default=Path(__file__).resolve().parents[1])
    parser.add_argument("--mode", choices=("scan", "repair", "validate"), default="scan")
    parser.add_argument(
        "--include",
        action="append",
        default=[],
        help="Case-insensitive substring in a sheet filename; may be repeated",
    )
    parser.add_argument("--report", type=Path)
    parser.add_argument("--preview-dir", type=Path)
    parser.add_argument("--preview-limit", type=int, default=24)
    parser.add_argument("--sequence-preview-dir", type=Path)
    parser.add_argument("--sequence-preview-limit", type=int, default=20)
    parser.add_argument(
        "--tolerance",
        type=float,
        default=0.035,
        help="Allowed difference from resting-sprite silhouette mass in validate mode",
    )
    return parser.parse_args()


def weighted_quantile(weights: np.ndarray, axis: int, quantile: float) -> float:
    totals = weights.sum(axis=axis)
    cumulative = np.cumsum(totals)
    total = float(cumulative[-1])
    if total <= 0.0:
        return 0.0
    index = int(np.searchsorted(cumulative, total * quantile, side="left"))
    return float(min(index, len(totals) - 1))


def measure(
    image: Image.Image, mass_basis_size: int | None = None
) -> Silhouette | None:
    alpha = np.asarray(image.convert("RGBA"), dtype=np.uint8)[..., 3]
    mask = alpha >= ALPHA_THRESHOLD
    points = np.argwhere(mask)
    if points.size == 0:
        return None

    top, left = points.min(axis=0)
    bottom, right = points.max(axis=0) + 1
    weights = alpha.astype(np.float64) / 255.0
    basis_size = mass_basis_size or image.width
    mass_fraction = float(weights.sum() / (basis_size * basis_size))
    anchor_x = weighted_quantile(weights, axis=0, quantile=0.5)
    anchor_y = weighted_quantile(weights, axis=1, quantile=GROUND_QUANTILE)
    return Silhouette(
        int(left),
        int(top),
        int(right),
        int(bottom),
        mass_fraction,
        anchor_x,
        anchor_y,
    )


def character_key(sheet: Path) -> str:
    if sheet.stem.lower() == "gt_animation_gloomfairy_idle":
        return "gloomFairy"
    return SHEET_SUFFIX.sub("", sheet.stem)


def find_reference(sheet: Path, references: dict[str, Path]) -> Path | None:
    key = character_key(sheet)
    alias = REFERENCE_ALIASES.get(key.lower(), key)
    return references.get(alias.lower())


def frame_images(sheet: Image.Image) -> list[Image.Image]:
    frame_size = sheet.height
    if frame_size <= 0 or sheet.width % frame_size != 0:
        return []
    return [
        sheet.crop((left, 0, left + frame_size, frame_size)).convert("RGBA")
        for left in range(0, sheet.width, frame_size)
    ]


def resize_premultiplied(image: Image.Image, size: tuple[int, int]) -> Image.Image:
    """Resize RGBA without pulling hidden transparent RGB into the edge."""

    source = np.asarray(image.convert("RGBA"), dtype=np.float32)
    alpha = source[..., 3:4] / 255.0
    premultiplied = np.concatenate((source[..., :3] * alpha, source[..., 3:4]), axis=2)
    channels = [
        Image.fromarray(
            np.clip(premultiplied[..., channel], 0, 255).astype(np.uint8), "L"
        ).resize(size, Image.Resampling.LANCZOS)
        for channel in range(4)
    ]
    resized = np.stack(
        [np.asarray(channel, dtype=np.float32) for channel in channels], axis=2
    )
    out_alpha = resized[..., 3:4]
    safe_alpha = np.maximum(out_alpha / 255.0, 1.0 / 255.0)
    rgb = np.where(out_alpha > 0.0, resized[..., :3] / safe_alpha, 0.0)
    rgba = np.concatenate(
        (np.clip(rgb, 0, 255), np.clip(out_alpha, 0, 255)), axis=2
    )
    rgba[out_alpha[..., 0] <= 0.0] = 0.0
    return Image.fromarray(np.rint(rgba).astype(np.uint8), "RGBA")


def transform_frame(
    frame: Image.Image,
    scale: float,
    offset_x: int,
    offset_y: int,
    output_size: int | None = None,
) -> Image.Image:
    scaled_size = max(1, int(round(frame.width * scale)))
    resized = resize_premultiplied(frame, (scaled_size, scaled_size))
    result_size = output_size or frame.width
    result = Image.new("RGBA", (result_size, result_size), (0, 0, 0, 0))
    result.alpha_composite(resized, (offset_x, offset_y))
    return result


def transformed_overflow(
    silhouettes: list[Silhouette | None],
    scale: float,
    offset_x: int,
    offset_y: int,
    frame_size: int,
) -> tuple[float, float]:
    overflow_x = 0.0
    overflow_y = 0.0
    for silhouette in silhouettes:
        if silhouette is None:
            continue
        left = offset_x + silhouette.left * scale
        right = offset_x + silhouette.right * scale
        top = offset_y + silhouette.top * scale
        bottom = offset_y + silhouette.bottom * scale
        overflow_x = max(overflow_x, -left, right - frame_size)
        overflow_y = max(overflow_y, -top, bottom - frame_size)
    return max(0.0, overflow_x), max(0.0, overflow_y)


def scale_limit_for_silhouettes(
    silhouettes: list[Silhouette | None], frame_size: int
) -> float:
    """Largest uniform scale that leaves a one-pixel margin in every frame."""

    usable_size = max(1.0, float(frame_size - 2))
    limits = [
        min(usable_size / silhouette.width, usable_size / silhouette.height)
        for silhouette in silhouettes
        if silhouette is not None and silhouette.width > 0 and silhouette.height > 0
    ]
    return min(limits, default=4.0)


def expanded_frame_margin(
    silhouettes: list[Silhouette | None],
    first_silhouette: Silhouette,
    scale: float,
    target_anchor_x: float,
    target_anchor_y: float,
    logical_frame_size: int,
) -> int:
    """Symmetric padding needed to preserve every scaled frame silhouette."""

    base_offset_x = target_anchor_x - first_silhouette.anchor_x * scale
    base_offset_y = target_anchor_y - first_silhouette.anchor_y * scale
    bounds = [silhouette for silhouette in silhouettes if silhouette is not None]
    if not bounds:
        return 0
    left = min(base_offset_x + silhouette.left * scale for silhouette in bounds)
    right = max(base_offset_x + silhouette.right * scale for silhouette in bounds)
    top = min(base_offset_y + silhouette.top * scale for silhouette in bounds)
    bottom = max(base_offset_y + silhouette.bottom * scale for silhouette in bounds)
    required = max(
        0.0,
        1.0 - left,
        right - (logical_frame_size - 1.0),
        1.0 - top,
        bottom - (logical_frame_size - 1.0),
    )
    return int(math.ceil(required))


def fitted_frame_offset(
    silhouette: Silhouette | None,
    scale: float,
    desired_x: int,
    desired_y: int,
    frame_size: int,
) -> tuple[int, int]:
    if silhouette is None:
        return desired_x, desired_y

    min_x = int(math.ceil(1.0 - silhouette.left * scale))
    max_x = int(math.floor(frame_size - 1.0 - silhouette.right * scale))
    min_y = int(math.ceil(1.0 - silhouette.top * scale))
    max_y = int(math.floor(frame_size - 1.0 - silhouette.bottom * scale))
    if min_x > max_x:
        min_x = max_x = int(round((min_x + max_x) * 0.5))
    if min_y > max_y:
        min_y = max_y = int(round((min_y + max_y) * 0.5))
    return (
        min(max(desired_x, min_x), max_x),
        min(max(desired_y, min_y), max_y),
    )


def transform_fitted_frame(frame: Image.Image, stats: SheetStats) -> Image.Image:
    offset_x, offset_y = fitted_frame_offset(
        measure(frame),
        stats.scale,
        stats.offset_x,
        stats.offset_y,
        stats.output_frame_size,
    )
    return transform_frame(
        frame,
        stats.scale,
        offset_x,
        offset_y,
        stats.output_frame_size,
    )


def png_info_with_marker(
    source_info: dict[str, object], marker: str, logical_frame_size: int
) -> PngImagePlugin.PngInfo:
    info = PngImagePlugin.PngInfo()
    for key, value in source_info.items():
        if (
            isinstance(value, str)
            and key != "bayou.animation_size"
            and key != LOGICAL_FRAME_KEY
        ):
            info.add_text(key, value)
    info.add_text("bayou.animation_size", marker)
    info.add_text(LOGICAL_FRAME_KEY, str(logical_frame_size))
    return info


def checker(size: tuple[int, int], cell: int = 16) -> Image.Image:
    width, height = size
    yy, xx = np.indices((height, width))
    pattern = ((xx // cell + yy // cell) % 2).astype(np.uint8)
    light = np.array((70, 74, 82, 255), dtype=np.uint8)
    dark = np.array((40, 43, 49, 255), dtype=np.uint8)
    pixels = np.where(pattern[..., None] == 0, light, dark)
    return Image.fromarray(pixels, "RGBA")


def render_cell(
    sprite: Image.Image,
    size: int = 256,
    logical_view_size: int | None = None,
) -> Image.Image:
    cell = checker((size, size))
    image = sprite.convert("RGBA")
    if logical_view_size and image.width > logical_view_size:
        margin = (image.width - logical_view_size) // 2
        image = image.crop(
            (
                margin,
                margin,
                margin + logical_view_size,
                margin + logical_view_size,
            )
        )
    if image.size != (size, size):
        image = resize_premultiplied(image, (size, size))
    cell.alpha_composite(image)
    return cell.convert("RGB")


def make_preview(
    destination: Path,
    reference: Image.Image,
    before: Image.Image,
    after: Image.Image,
    stats: SheetStats,
) -> None:
    label_height = 46
    tile = 256
    preview = Image.new("RGB", (tile * 3, tile + label_height), (24, 26, 31))
    preview.paste(render_cell(reference), (0, label_height))
    preview.paste(render_cell(before), (tile, label_height))
    preview.paste(
        render_cell(after, logical_view_size=stats.logical_frame_size),
        (tile * 2, label_height),
    )
    draw = ImageDraw.Draw(preview)
    font = ImageFont.load_default()
    draw.text(
        (8, 6),
        f"{Path(stats.sheet).name}   scale {stats.scale:.3f}   "
        f"offset ({stats.offset_x}, {stats.offset_y})",
        fill=(235, 235, 235),
        font=font,
    )
    draw.text(
        (8, 24), "resting sprite", fill=(165, 215, 255), font=font
    )
    draw.text(
        (tile + 8, 24),
        "animation frame 1: before",
        fill=(255, 198, 128),
        font=font,
    )
    draw.text(
        (tile * 2 + 8, 24),
        "animation frame 1: normalized",
        fill=(164, 235, 170),
        font=font,
    )
    destination.parent.mkdir(parents=True, exist_ok=True)
    preview.save(destination, optimize=True)


def make_sequence_preview(
    destination: Path, sheet_path: Path, stats: SheetStats
) -> None:
    with Image.open(sheet_path) as opened_sheet:
        frames = frame_images(opened_sheet.convert("RGBA"))
    transformed = [
        transform_fitted_frame(frame, stats) for frame in frames
    ]
    columns = min(8, len(frames))
    rows = int(math.ceil(len(frames) / columns))
    tile = 128
    header = 30
    block_height = header + rows * tile
    preview = Image.new(
        "RGB", (columns * tile, block_height * 2), (24, 26, 31)
    )
    draw = ImageDraw.Draw(preview)
    font = ImageFont.load_default()
    groups = (
        ("before", frames, (255, 198, 128)),
        ("normalized", transformed, (164, 235, 170)),
    )
    for group_index, (label, images, color) in enumerate(groups):
        block_y = group_index * block_height
        draw.text(
            (8, block_y + 7),
            f"{sheet_path.name}: {label}   scale {stats.scale:.3f}   "
            f"overflow ({stats.max_overflow_x:.1f}, "
            f"{stats.max_overflow_y:.1f})",
            fill=color,
            font=font,
        )
        for index, frame in enumerate(images):
            column = index % columns
            row = index // columns
            left = column * tile
            top = block_y + header + row * tile
            logical_view = (
                stats.logical_frame_size if label == "normalized" else None
            )
            preview.paste(
                render_cell(frame, tile, logical_view), (left, top)
            )
            draw.rectangle(
                (left + 3, top + 3, left + 23, top + 17),
                fill=(15, 17, 20),
            )
            draw.text(
                (left + 7, top + 5),
                str(index + 1),
                fill=(235, 235, 235),
                font=font,
            )
    destination.parent.mkdir(parents=True, exist_ok=True)
    preview.save(destination, optimize=True)


def analyze_sheet(
    sheet_path: Path, reference_path: Path, mode: str
) -> tuple[
    SheetStats, tuple[Image.Image, Image.Image, Image.Image] | None
]:
    stats = SheetStats(sheet=str(sheet_path), reference=str(reference_path))
    with Image.open(reference_path) as opened_reference:
        reference = opened_reference.convert("RGBA")
    reference_silhouette = measure(reference)
    if reference_silhouette is None:
        stats.skipped = True
        stats.skip_reason = "reference sprite has no visible pixels"
        return stats, None

    with Image.open(sheet_path) as opened_sheet:
        stats.marker_before = str(opened_sheet.info.get("bayou.animation_size", ""))
        logical_frame_value = opened_sheet.info.get(LOGICAL_FRAME_KEY, "")
        source_sheet = opened_sheet.convert("RGBA")
        source_info = dict(opened_sheet.info)

    frames = frame_images(source_sheet)
    if not frames:
        stats.skipped = True
        stats.skip_reason = "sheet is not a horizontal row of square frames"
        return stats, None

    stats.frame_size = frames[0].width
    try:
        stats.logical_frame_size = int(str(logical_frame_value))
    except ValueError:
        stats.logical_frame_size = min(stats.frame_size, LOGICAL_FRAME_SIZE)
    if stats.logical_frame_size <= 0 or stats.logical_frame_size > stats.frame_size:
        stats.logical_frame_size = min(stats.frame_size, LOGICAL_FRAME_SIZE)
    stats.output_frame_size = stats.frame_size
    stats.frame_margin = max(
        0, (stats.frame_size - stats.logical_frame_size) // 2
    )

    silhouettes = [
        measure(frame, stats.logical_frame_size) for frame in frames
    ]
    first_silhouette = silhouettes[0]
    if first_silhouette is None or first_silhouette.mass_fraction <= 0.0:
        stats.skipped = True
        stats.skip_reason = "first frame has no visible pixels"
        return stats, None

    stats.frames = len(frames)
    stats.reference_mass = reference_silhouette.mass_fraction
    stats.source_mass = first_silhouette.mass_fraction
    measured_scale = math.sqrt(stats.reference_mass / stats.source_mass)
    stats.requested_scale = min(max(measured_scale, 0.25), 4.0)
    stats.fit_scale_limit = scale_limit_for_silhouettes(
        silhouettes, stats.logical_frame_size
    )
    if abs(math.log(max(measured_scale, 1e-6))) <= math.log1p(
        NORMALIZATION_MATCH_TOLERANCE
    ):
        stats.scale = 1.0
    else:
        scaled_size = max(
            1,
            int(round(stats.logical_frame_size * stats.requested_scale)),
        )
        stats.scale = scaled_size / stats.logical_frame_size

    if abs(stats.scale - 1.0) > 0.001:
        target_anchor_x = (
            reference_silhouette.anchor_x
            / reference.width
            * stats.logical_frame_size
        )
        target_anchor_y = (
            reference_silhouette.anchor_y
            / reference.height
            * stats.logical_frame_size
        )
        stats.frame_margin = (
            expanded_frame_margin(
                silhouettes,
                first_silhouette,
                stats.scale,
                target_anchor_x,
                target_anchor_y,
                stats.logical_frame_size,
            )
            if stats.scale > 1.0
            else 0
        )
        stats.output_frame_size = (
            stats.logical_frame_size + stats.frame_margin * 2
        )
        stats.fit_limited = stats.output_frame_size > stats.logical_frame_size
        stats.offset_x = int(
            round(
                stats.frame_margin
                + target_anchor_x
                - first_silhouette.anchor_x * stats.scale
            )
        )
        stats.offset_y = int(
            round(
                stats.frame_margin
                + target_anchor_y
                - first_silhouette.anchor_y * stats.scale
            )
        )

    stats.reference_width = reference_silhouette.width / reference.width
    stats.reference_height = reference_silhouette.height / reference.height
    stats.source_width = first_silhouette.width / stats.logical_frame_size
    stats.source_height = first_silhouette.height / stats.logical_frame_size
    stats.transformed_width = stats.source_width * stats.scale
    stats.transformed_height = stats.source_height * stats.scale
    stats.max_overflow_x, stats.max_overflow_y = transformed_overflow(
        silhouettes,
        stats.scale,
        stats.offset_x,
        stats.offset_y,
        stats.output_frame_size,
    )

    transformed_first = transform_fitted_frame(frames[0], stats)
    stats.changed = abs(stats.scale - 1.0) > 0.001

    if mode == "repair" and stats.changed:
        transformed_frames = [
            transform_fitted_frame(frame, stats) for frame in frames
        ]
        output = Image.new(
            "RGBA",
            (stats.output_frame_size * len(frames), stats.output_frame_size),
            (0, 0, 0, 0),
        )
        for index, frame in enumerate(transformed_frames):
            output.alpha_composite(
                frame, (index * stats.output_frame_size, 0)
            )
        stats.marker_after = NORMALIZATION_VERSION
        output.save(
            sheet_path,
            pnginfo=png_info_with_marker(
                source_info,
                NORMALIZATION_VERSION,
                stats.logical_frame_size,
            ),
            optimize=True,
        )
    else:
        stats.marker_after = stats.marker_before

    return stats, (reference, frames[0], transformed_first)


def write_report(path: Path, rows: list[SheetStats]) -> None:
    path.parent.mkdir(parents=True, exist_ok=True)
    with path.open("w", newline="", encoding="utf-8") as handle:
        writer = csv.DictWriter(handle, fieldnames=list(asdict(rows[0]).keys()))
        writer.writeheader()
        writer.writerows(asdict(row) for row in rows)


def main() -> int:
    args = parse_args()
    root = args.root.resolve()
    animations = root / "assets" / "animations"
    characters = root / "assets" / "characters"
    references = {
        path.stem.lower(): path for path in characters.glob("*.png")
    }
    sheets = sorted(
        animations.glob("*.png"), key=lambda path: path.name.lower()
    )
    if args.include:
        needles = [value.lower() for value in args.include]
        sheets = [
            path
            for path in sheets
            if any(value in path.name.lower() for value in needles)
        ]

    rows: list[SheetStats] = []
    previews: list[
        tuple[
            float,
            Path,
            tuple[Image.Image, Image.Image, Image.Image],
            SheetStats,
        ]
    ] = []
    for sheet in sheets:
        reference = find_reference(sheet, references)
        if reference is None:
            rows.append(
                SheetStats(
                    sheet=str(sheet),
                    skipped=True,
                    skip_reason=(
                        f"no resting sprite for key '{character_key(sheet)}'"
                    ),
                )
            )
            continue
        stats, preview = analyze_sheet(sheet, reference, args.mode)
        rows.append(stats)
        if preview is not None:
            previews.append(
                (
                    abs(math.log(max(stats.scale, 1e-6))),
                    sheet,
                    preview,
                    stats,
                )
            )

    if not rows:
        print("No animation sheets matched.")
        return 1

    report = args.report or root / "output" / f"animation-size-{args.mode}.csv"
    if not report.is_absolute():
        report = root / report
    write_report(report, rows)

    if args.preview_dir:
        preview_root = args.preview_dir
        if not preview_root.is_absolute():
            preview_root = root / preview_root
        previews.sort(key=lambda item: (-item[0], item[1].name.lower()))
        for _, sheet, preview, stats in previews[: max(0, args.preview_limit)]:
            make_preview(preview_root / sheet.name, *preview, stats)

    if args.sequence_preview_dir and args.mode == "scan":
        sequence_root = args.sequence_preview_dir
        if not sequence_root.is_absolute():
            sequence_root = root / sequence_root
        sequence_rows = [
            row
            for row in rows
            if not row.skipped
            and (row.max_overflow_x > 0.5 or row.max_overflow_y > 0.5)
        ]
        sequence_rows.sort(
            key=lambda row: (
                -max(row.max_overflow_x, row.max_overflow_y),
                Path(row.sheet).name.lower(),
            )
        )
        for stats in sequence_rows[: max(0, args.sequence_preview_limit)]:
            sheet = Path(stats.sheet)
            make_sequence_preview(sequence_root / sheet.name, sheet, stats)

    processed = [row for row in rows if not row.skipped]
    skipped = [row for row in rows if row.skipped]
    overflow = [
        row
        for row in processed
        if row.max_overflow_x > 0.5 or row.max_overflow_y > 0.5
    ]
    large_changes = [
        row
        for row in processed
        if abs(math.log(max(row.scale, 1e-6))) > math.log(1.1)
    ]
    print(f"Mode: {args.mode}")
    print(f"Processed: {len(processed)}")
    print(f"Skipped: {len(skipped)}")
    print(f"Scale changes over 10%: {len(large_changes)}")
    print(f"Sheets needing edge-safe placement: {len(overflow)}")
    print(f"Report: {report}")
    if skipped:
        print("Skipped sheets:")
        for row in skipped:
            print(f"  {Path(row.sheet).name}: {row.skip_reason}")

    if args.mode == "validate":
        failures = [
            row
            for row in processed
            if abs(
                math.log(
                    max(
                        math.sqrt(row.reference_mass / row.source_mass),
                        1e-6,
                    )
                )
            )
            > math.log1p(args.tolerance)
        ]
        if failures:
            print(f"Validation failures: {len(failures)}")
            return 2

    return 0


if __name__ == "__main__":
    raise SystemExit(main())
