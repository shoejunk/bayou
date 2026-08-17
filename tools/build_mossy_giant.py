from __future__ import annotations

import json
from pathlib import Path

import numpy as np
from PIL import Image, ImageDraw, ImageFilter

from animation_halo_cleanup import repair_frame, suspicious_count
from build_glimmer_stag import (
    bounds,
    harden_interior_alpha,
    premultiplied_resize,
    remove_tiny_components,
    zero_transparent_rgb,
)


CELL = 256
GRID_COLUMNS = 6
GRID_ROWS = 5
ALPHA_CUT = 4
GROUND_Y = 244

ROOT = Path(r"C:\Users\jarox\OneDrive\Desktop\Tandem Tales\Bayou Bonanza")
SOURCE = Path(r"C:\Users\jarox\OneDrive\Desktop\TT Temp")
OUTPUT = ROOT / "assets" / "animations"
REVIEW = ROOT / "tools" / "mossyGiant-review"

# The source sheets contain 30 complete frames in a 6x5 grid. The final walk
# frames settle back toward the original stance, so use the contiguous active
# stride instead. The death stops at frame 18, before the giant begins to raise
# its body again.
FRAME_SELECTIONS = {
    "walk": list(range(2, 26)),
    "attack": [2, 4, 7, 9, 12, 14, 17, 19, 22, 24, 27, 29],
    "damaged": [2, 4, 7, 9, 12, 14, 17, 19, 22, 24, 27, 29],
    "killed": [2, 3, 5, 6, 8, 9, 11, 13, 14, 16, 17, 18],
}

SOURCE_NAMES = {
    "walk": "walking",
    "attack": "attack",
    "damaged": "damaged",
    "killed": "killed",
}

SCALES = {
    "walk": 0.60,
    "attack": 0.57,
    "damaged": 0.63,
    "killed": 0.56,
}

TARGET_CORE_X = {
    "walk": 114,
    "attack": 118,
    "damaged": 108,
    "killed": 111,
}


def crop_frame(sheet: Image.Image, frame_number: int) -> Image.Image:
    if sheet.width % GRID_COLUMNS or sheet.height % GRID_ROWS:
        raise ValueError(
            f"{sheet.width}x{sheet.height} is not an exact "
            f"{GRID_COLUMNS}x{GRID_ROWS} sheet"
        )
    frame_width = sheet.width // GRID_COLUMNS
    frame_height = sheet.height // GRID_ROWS
    index = frame_number - 1
    left = (index % GRID_COLUMNS) * frame_width
    top = (index // GRID_COLUMNS) * frame_height
    frame = sheet.crop((left, top, left + frame_width, top + frame_height))
    return Image.fromarray(
        zero_transparent_rgb(np.asarray(frame.convert("RGBA"))), "RGBA"
    )


def dense_torso_core(image: Image.Image) -> tuple[float, float]:
    alpha = np.asarray(image.convert("RGBA"))[..., 3]
    core_mask = Image.fromarray(
        np.where(alpha > 20, 255, 0).astype(np.uint8), "L"
    )
    core = np.asarray(core_mask.filter(ImageFilter.MinFilter(17))) > 0
    ys, xs = np.nonzero(core)
    if len(xs) < 20:
        ys, xs = np.nonzero(alpha > 20)
    if len(xs) == 0:
        return image.width / 2.0, image.height / 2.0

    x_low, x_high = np.percentile(xs, [28, 72])
    y_low, y_high = np.percentile(ys, [25, 78])
    use = (
        (xs >= x_low)
        & (xs <= x_high)
        & (ys >= y_low)
        & (ys <= y_high)
    )
    if np.count_nonzero(use) < 20:
        use = (xs >= x_low) & (xs <= x_high)
    return float(np.median(xs[use])), float(np.median(ys[use]))


def prepare_frames(animation: str) -> list[Image.Image]:
    source_path = SOURCE / f"SS_mossyGiant_{SOURCE_NAMES[animation]}.png"
    with Image.open(source_path) as opened:
        sheet = opened.convert("RGBA")
    if sheet.width % GRID_COLUMNS or sheet.height % GRID_ROWS:
        raise ValueError(f"{source_path.name} is not an exact 6x5 sheet")

    scale = SCALES[animation]
    size = (
        round((sheet.width // GRID_COLUMNS) * scale),
        round((sheet.height // GRID_ROWS) * scale),
    )
    return [
        harden_interior_alpha(
            remove_tiny_components(
                premultiplied_resize(crop_frame(sheet, frame_number), size),
                minimum_pixels=5,
            )
        )
        for frame_number in FRAME_SELECTIONS[animation]
    ]


def grounded_placements(
    animation: str, images: list[Image.Image]
) -> list[tuple[int, int]]:
    placements: list[tuple[int, int]] = []
    for image in images:
        box = bounds(image)
        if box is None:
            placements.append((0, 0))
            continue
        core_x, _core_y = dense_torso_core(image)
        placements.append(
            (round(TARGET_CORE_X[animation] - core_x), GROUND_Y - box[3])
        )
    return placements


def assemble(
    animation: str,
    images: list[Image.Image],
    placements: list[tuple[int, int]],
) -> tuple[Path, dict[str, int]]:
    sheet = Image.new("RGBA", (CELL * len(images), CELL), (0, 0, 0, 0))
    for index, (image, (local_x, local_y)) in enumerate(zip(images, placements)):
        box = bounds(image)
        if box is not None:
            output_box = (
                local_x + box[0],
                local_y + box[1],
                local_x + box[2],
                local_y + box[3],
            )
            if (
                output_box[0] < 2
                or output_box[1] < 2
                or output_box[2] > CELL - 3
                or output_box[3] > CELL - 3
            ):
                source_frame = FRAME_SELECTIONS[animation][index]
                raise RuntimeError(
                    f"{animation} source frame {source_frame} clips at {output_box}"
                )
        sheet.alpha_composite(image, (index * CELL + local_x, local_y))

    array = zero_transparent_rgb(np.asarray(sheet))
    for index in range(len(images)):
        left = index * CELL
        array[:, left] = 0
        array[:, left + CELL - 1] = 0
        array[0, left : left + CELL] = 0
        array[-1, left : left + CELL] = 0
    cleaned = array.copy()
    suspicious_before = 0
    suspicious_after = 0
    changed = 0
    for index in range(len(images)):
        left = index * CELL
        frame = array[:, left : left + CELL]
        suspicious_before += suspicious_count(frame)
        repaired_frame, frame_changed, _zero_fixed = repair_frame(frame)
        cleaned[:, left : left + CELL] = repaired_frame
        suspicious_after += suspicious_count(repaired_frame)
        changed += frame_changed

    repaired = Image.fromarray(zero_transparent_rgb(cleaned), "RGBA")
    OUTPUT.mkdir(parents=True, exist_ok=True)
    path = OUTPUT / f"mossyGiant-{animation}.png"
    repaired.save(path, format="PNG", optimize=False)
    return path, {
        "pixels_recolored": changed,
        "suspicious_before": suspicious_before,
        "suspicious_after": suspicious_after,
    }


def make_contact_sheet(animation: str, sheet: Image.Image) -> Path:
    frame_count = len(FRAME_SELECTIONS[animation])
    columns = 6
    rows = (frame_count + columns - 1) // columns
    preview_cell = 192
    contact = Image.new(
        "RGBA", (columns * preview_cell, rows * preview_cell), (28, 24, 35, 255)
    )
    draw = ImageDraw.Draw(contact)
    for index, source_number in enumerate(FRAME_SELECTIONS[animation]):
        frame = sheet.crop((index * CELL, 0, (index + 1) * CELL, CELL))
        frame.thumbnail((preview_cell, preview_cell), Image.Resampling.LANCZOS)
        x = (index % columns) * preview_cell
        y = (index // columns) * preview_cell
        for py in range(y, y + preview_cell, 16):
            for px in range(x, x + preview_cell, 16):
                color = (
                    (55, 50, 64, 255)
                    if ((px // 16 + py // 16) % 2)
                    else (38, 34, 46, 255)
                )
                draw.rectangle((px, py, px + 15, py + 15), fill=color)
        contact.alpha_composite(frame, (x, y))
        draw.rectangle((x, y, x + 51, y + 17), fill=(0, 0, 0, 210))
        draw.text((x + 4, y + 2), f"{index + 1}: {source_number}", fill="white")
    REVIEW.mkdir(parents=True, exist_ok=True)
    path = REVIEW / f"mossyGiant-{animation}-contact.png"
    contact.convert("RGB").save(path, format="PNG")
    return path


def make_edge_preview(animation: str, sheet: Image.Image) -> Path:
    frame_count = len(FRAME_SELECTIONS[animation])
    sample_indices = sorted(
        {0, frame_count // 3, (2 * frame_count) // 3, frame_count - 1}
    )
    backgrounds = [(0, 0, 0), (38, 18, 54), (0, 210, 70), (112, 112, 112)]
    canvas = Image.new("RGB", (len(sample_indices) * CELL, len(backgrounds) * CELL))
    for row, background in enumerate(backgrounds):
        for column, frame_index in enumerate(sample_indices):
            frame = sheet.crop(
                (frame_index * CELL, 0, (frame_index + 1) * CELL, CELL)
            )
            base = Image.new("RGBA", (CELL, CELL), (*background, 255))
            base.alpha_composite(frame)
            canvas.paste(base.convert("RGB"), (column * CELL, row * CELL))
    REVIEW.mkdir(parents=True, exist_ok=True)
    path = REVIEW / f"mossyGiant-{animation}-edges.png"
    canvas.save(path, format="PNG")
    return path


def validate_output(animation: str, path: Path) -> dict[str, object]:
    frame_count = len(FRAME_SELECTIONS[animation])
    with Image.open(path) as opened:
        source_mode = opened.mode
        image = opened.convert("RGBA")
    array = np.asarray(image)
    failures: list[str] = []
    if source_mode != "RGBA":
        failures.append(f"mode is {source_mode}, not RGBA")
    if image.size != (frame_count * CELL, CELL):
        failures.append(f"wrong dimensions {image.size}")
    if np.any(array[array[..., 3] == 0, :3]):
        failures.append("nonzero RGB under zero alpha")

    core_x_values: list[float] = []
    bottom_values: list[int] = []
    frame_bounds: list[list[int]] = []
    for index in range(frame_count):
        frame = image.crop((index * CELL, 0, (index + 1) * CELL, CELL))
        frame_array = np.asarray(frame)
        if np.any(frame_array[0]) or np.any(frame_array[-1]):
            failures.append(f"frame {index + 1} touches a horizontal border")
        if np.any(frame_array[:, 0]) or np.any(frame_array[:, -1]):
            failures.append(f"frame {index + 1} touches a vertical border")
        box = bounds(frame)
        if box is None:
            failures.append(f"frame {index + 1} is empty")
            continue
        frame_bounds.append(list(box))
        core_x, _core_y = dense_torso_core(frame)
        core_x_values.append(core_x)
        bottom_values.append(box[3])

    target_x = TARGET_CORE_X[animation]
    if core_x_values and max(abs(value - target_x) for value in core_x_values) > 1.0:
        failures.append("torso anchor drift exceeds 1px")
    if bottom_values and any(value != GROUND_Y for value in bottom_values):
        failures.append("ground contact is not locked")

    return {
        "animation": animation,
        "path": str(path),
        "size": list(image.size),
        "frames": frame_count,
        "source_grid": [GRID_COLUMNS, GRID_ROWS],
        "source_frames": FRAME_SELECTIONS[animation],
        "core_x_range": [min(core_x_values), max(core_x_values)] if core_x_values else None,
        "bottom_y_range": [min(bottom_values), max(bottom_values)] if bottom_values else None,
        "frame_bounds": frame_bounds,
        "failures": failures,
    }


def main() -> None:
    REVIEW.mkdir(parents=True, exist_ok=True)
    report: list[dict[str, object]] = []
    for animation in FRAME_SELECTIONS:
        images = prepare_frames(animation)
        placements = grounded_placements(animation, images)
        path, cleanup = assemble(animation, images, placements)
        with Image.open(path) as opened:
            sheet = opened.convert("RGBA")
        make_contact_sheet(animation, sheet)
        make_edge_preview(animation, sheet)
        result = validate_output(animation, path)
        result["halo_cleanup"] = cleanup
        result["placements"] = placements
        report.append(result)
        print(
            f"{path.name}: {sheet.width}x{sheet.height}, "
            f"{len(FRAME_SELECTIONS[animation])} frames, "
            f"{cleanup['pixels_recolored']} fringe pixels recolored, "
            f"suspicious {cleanup['suspicious_before']}"
            f"->{cleanup['suspicious_after']}"
        )

    report_path = REVIEW / "mossyGiant-validation.json"
    report_path.write_text(json.dumps(report, indent=2), encoding="ascii")
    failures = [item for item in report if item["failures"]]
    if failures:
        raise RuntimeError(f"MossyGiant validation failed; see {report_path}")
    print(f"Validation passed: {report_path}")


if __name__ == "__main__":
    main()
