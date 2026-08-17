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
GROUND_Y = 244
MAX_CONTENT_WIDTH = 246
MAX_CONTENT_HEIGHT = 238
HALO_PASSES = 4

ROOT = Path(r"C:\Users\jarox\OneDrive\Desktop\Tandem Tales\Bayou Bonanza")
SOURCE = Path(r"C:\Users\jarox\OneDrive\Desktop\TT Temp")
OUTPUT = ROOT / "assets" / "animations"
REVIEW = ROOT / "tools" / "maggieMudroot-mounted-review"

# These sheets are exact 6x5 grids containing 30 complete frames. The walk
# selection is an even 24-frame sample of frames 2-29. The death stops with
# Maggie slumped against the cabin at frame 13, before she sits back up.
FRAME_SELECTIONS = {
    "walk": [
        2, 3, 4, 6, 7, 8, 9, 10, 11, 13, 14, 15,
        16, 17, 18, 20, 21, 22, 23, 24, 25, 27, 28, 29,
    ],
    "attack": [2, 4, 7, 9, 12, 14, 17, 19, 22, 24, 27, 29],
    "damaged": [2, 4, 7, 9, 12, 14, 17, 19, 22, 24, 27, 29],
    "killed": list(range(2, 14)),
}

SOURCE_NAMES = {
    "walk": "walking",
    "attack": "attack",
    "damaged": "damaged",
    "killed": "killed",
}

FIXED_CHASSIS = {"walk", "attack", "killed"}


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


def cabin_anchor(image: Image.Image) -> tuple[float, float]:
    """Track the cabin windows instead of moving jaws, limbs, or the rider."""
    array = np.asarray(image.convert("RGBA"))
    box = bounds(image)
    if box is None:
        return image.width / 2.0, image.height / 2.0
    left, top, right, bottom = box
    width = right - left + 1
    height = bottom - top + 1
    yy, xx = np.indices(array.shape[:2])
    red = array[..., 0].astype(np.int16)
    green = array[..., 1].astype(np.int16)
    blue = array[..., 2].astype(np.int16)
    warm_window = (
        (array[..., 3] > 64)
        & (xx >= left + round(width * 0.10))
        & (xx <= left + round(width * 0.56))
        & (yy >= top + round(height * 0.03))
        & (yy <= top + round(height * 0.64))
        & (red > 48)
        & (red > green + 10)
        & (green > blue + 4)
    )
    ys, xs = np.nonzero(warm_window)
    if len(xs) >= 20:
        return float(np.median(xs)), float(np.median(ys))

    alpha = array[..., 3]
    solid = Image.fromarray(
        np.where(alpha > 32, 255, 0).astype(np.uint8), "L"
    ).filter(ImageFilter.MinFilter(11))
    core = np.asarray(solid) > 0
    core &= (
        (xx >= left + round(width * 0.10))
        & (xx <= left + round(width * 0.56))
        & (yy <= top + round(height * 0.64))
    )
    ys, xs = np.nonzero(core)
    if len(xs) >= 20:
        return float(np.median(xs)), float(np.median(ys))
    return (left + right) / 2.0, top + height * 0.42


def load_raw_frames() -> dict[str, list[Image.Image]]:
    output: dict[str, list[Image.Image]] = {}
    for animation, frame_numbers in FRAME_SELECTIONS.items():
        source_path = (
            SOURCE
            / f"SS_maggieMudroot_mounted_{SOURCE_NAMES[animation]}.png"
        )
        with Image.open(source_path) as opened:
            sheet = opened.convert("RGBA")
        if sheet.width % GRID_COLUMNS or sheet.height % GRID_ROWS:
            raise ValueError(f"{source_path.name} is not an exact 6x5 sheet")
        output[animation] = [
            harden_interior_alpha(
                remove_tiny_components(crop_frame(sheet, frame_number), 5)
            )
            for frame_number in frame_numbers
        ]
    return output


def shared_scale(raw: dict[str, list[Image.Image]]) -> float:
    boxes = [
        box
        for images in raw.values()
        for image in images
        if (box := bounds(image)) is not None
    ]
    widest = max(box[2] - box[0] + 1 for box in boxes)
    tallest = max(box[3] - box[1] + 1 for box in boxes)
    return min(
        MAX_CONTENT_WIDTH / widest,
        MAX_CONTENT_HEIGHT / tallest,
        1.0,
    )


def fitted_anchor_target(
    images: list[Image.Image],
) -> tuple[int, int]:
    anchors = [cabin_anchor(image) for image in images]
    x_lower = -10_000.0
    x_upper = 10_000.0
    y_lower = -10_000.0
    y_upper = 10_000.0
    centered_x: list[float] = []
    grounded_y: list[float] = []
    for image, (anchor_x, anchor_y) in zip(images, anchors):
        box = bounds(image)
        if box is None:
            continue
        left, top, right, bottom = box
        x_lower = max(x_lower, anchor_x + 2 - left)
        x_upper = min(x_upper, anchor_x + CELL - 3 - right)
        y_lower = max(y_lower, anchor_y + 2 - top)
        y_upper = min(y_upper, anchor_y + CELL - 3 - bottom)
        centered_x.append(128.0 + anchor_x - (left + right) / 2.0)
        grounded_y.append(GROUND_Y + anchor_y - bottom)
    requested_x = float(np.median(centered_x))
    requested_y = float(np.median(grounded_y))
    return (
        round(min(max(requested_x, x_lower), x_upper)),
        round(min(max(requested_y, y_lower), y_upper)),
    )


def placements_for(
    animation: str, images: list[Image.Image]
) -> tuple[list[tuple[int, int]], tuple[int, int]]:
    target_x, target_y = fitted_anchor_target(images)
    placements: list[tuple[int, int]] = []
    for image in images:
        anchor_x, anchor_y = cabin_anchor(image)
        box = bounds(image)
        if box is None:
            placements.append((0, 0))
            continue
        if animation == "damaged":
            local_y = GROUND_Y - box[3]
        else:
            local_y = round(target_y - anchor_y)
        placements.append((round(target_x - anchor_x), local_y))
    return placements, (target_x, target_y)


def repair_sheet(
    image: Image.Image, frame_count: int
) -> tuple[Image.Image, dict[str, int]]:
    array = zero_transparent_rgb(np.asarray(image.convert("RGBA")))
    output = array.copy()
    suspicious_before = 0
    suspicious_after = 0
    changed = 0
    for index in range(frame_count):
        left = index * CELL
        frame = array[:, left : left + CELL]
        suspicious_before += suspicious_count(frame)
        repaired = frame
        for _pass in range(HALO_PASSES):
            repaired, _frame_changed, _zero_fixed = repair_frame(repaired)
        output[:, left : left + CELL] = repaired
        suspicious_after += suspicious_count(repaired)
        changed += int(np.count_nonzero(np.any(repaired != frame, axis=2)))
    return Image.fromarray(zero_transparent_rgb(output), "RGBA"), {
        "pixels_recolored": changed,
        "suspicious_before": suspicious_before,
        "suspicious_after": suspicious_after,
    }


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
    repaired, cleanup = repair_sheet(Image.fromarray(array, "RGBA"), len(images))
    OUTPUT.mkdir(parents=True, exist_ok=True)
    path = OUTPUT / f"maggieMudroot_mounted-{animation}.png"
    repaired.save(path, format="PNG", optimize=False)
    return path, cleanup


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
        draw.rectangle((x, y, x + 55, y + 17), fill=(0, 0, 0, 210))
        draw.text((x + 4, y + 2), f"{index + 1}: {source_number}", fill="white")
    REVIEW.mkdir(parents=True, exist_ok=True)
    path = REVIEW / f"maggieMudroot-mounted-{animation}-contact.png"
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
    path = REVIEW / f"maggieMudroot-mounted-{animation}-edges.png"
    canvas.save(path, format="PNG")
    return path


def validate_output(
    animation: str,
    path: Path,
    target: tuple[int, int],
) -> dict[str, object]:
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

    anchor_x_values: list[float] = []
    anchor_y_values: list[float] = []
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
        anchor_x, anchor_y = cabin_anchor(frame)
        anchor_x_values.append(anchor_x)
        anchor_y_values.append(anchor_y)
        bottom_values.append(box[3])

    if anchor_x_values and max(abs(value - target[0]) for value in anchor_x_values) > 1.0:
        failures.append("cabin horizontal anchor drift exceeds 1px")
    if animation in FIXED_CHASSIS:
        if anchor_y_values and max(abs(value - target[1]) for value in anchor_y_values) > 1.0:
            failures.append("cabin vertical anchor drift exceeds 1px")
    elif bottom_values and any(value != GROUND_Y for value in bottom_values):
        failures.append("damaged ground contact is not locked")

    return {
        "animation": animation,
        "path": str(path),
        "size": list(image.size),
        "frames": frame_count,
        "source_grid": [GRID_COLUMNS, GRID_ROWS],
        "source_frames": FRAME_SELECTIONS[animation],
        "target_anchor": list(target),
        "cabin_x_range": [min(anchor_x_values), max(anchor_x_values)] if anchor_x_values else None,
        "cabin_y_range": [min(anchor_y_values), max(anchor_y_values)] if anchor_y_values else None,
        "bottom_y_range": [min(bottom_values), max(bottom_values)] if bottom_values else None,
        "frame_bounds": frame_bounds,
        "failures": failures,
    }


def main() -> None:
    REVIEW.mkdir(parents=True, exist_ok=True)
    raw = load_raw_frames()
    scale = shared_scale(raw)
    resized = {
        animation: [
            premultiplied_resize(
                image,
                (round(image.width * scale), round(image.height * scale)),
            )
            for image in images
        ]
        for animation, images in raw.items()
    }

    report: list[dict[str, object]] = []
    for animation, images in resized.items():
        placements, target = placements_for(animation, images)
        path, cleanup = assemble(animation, images, placements)
        with Image.open(path) as opened:
            sheet = opened.convert("RGBA")
        make_contact_sheet(animation, sheet)
        make_edge_preview(animation, sheet)
        result = validate_output(animation, path, target)
        result["scale"] = scale
        result["placements"] = placements
        result["halo_cleanup"] = cleanup
        report.append(result)
        print(
            f"{path.name}: {sheet.width}x{sheet.height}, "
            f"{len(FRAME_SELECTIONS[animation])} frames, "
            f"{cleanup['pixels_recolored']} halo pixels recolored, "
            f"suspicious {cleanup['suspicious_before']}"
            f"->{cleanup['suspicious_after']}"
        )

    report_path = REVIEW / "maggieMudroot-mounted-validation.json"
    report_path.write_text(json.dumps(report, indent=2), encoding="ascii")
    failures = [item for item in report if item["failures"]]
    if failures:
        raise RuntimeError(f"Maggie mounted validation failed; see {report_path}")
    print(f"Shared scale: {scale:.6f}")
    print(f"Validation passed: {report_path}")


if __name__ == "__main__":
    main()
