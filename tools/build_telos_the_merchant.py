from __future__ import annotations

import json
from collections.abc import Callable
from pathlib import Path

import numpy as np
from PIL import Image, ImageDraw

from animation_halo_cleanup import suspicious_count
from build_glimmer_stag import (
    bounds,
    harden_interior_alpha,
    premultiplied_resize,
    remove_tiny_components,
    zero_transparent_rgb,
)
from build_maggie_mudroot_mounted import repair_sheet


CELL = 256
GRID = 9
GROUND_Y = 244
MAX_CONTENT_WIDTH = 246
MAX_CONTENT_HEIGHT = 238

ROOT = Path(r"C:\Users\jarox\OneDrive\Desktop\Tandem Tales\Bayou Bonanza")
SOURCE = Path(r"C:\Users\jarox\OneDrive\Desktop\TT Temp")
OUTPUT = ROOT / "assets" / "animations"
REVIEW = ROOT / "tools" / "telosTheMerchant-review"
SOURCE_REVIEW = ROOT / "tools" / "telosTheMerchant-source-previews"

SOURCE_NAMES = {
    "attack": "attack",
    "damaged": "damaged",
    "killed": "killed",
    "walk": "walking",
}

# Each list is evenly distributed over the useful motion. Attack and damage
# stop once the action has returned to stance, while death stops at frame 37
# before the later prone frames begin raising Telos's head and shoulders.
FRAME_SELECTIONS = {
    "walk": [
        2, 5, 9, 12, 16, 19, 22, 26, 29, 33, 36, 39,
        43, 46, 49, 53, 56, 60, 63, 66, 70, 73, 77, 80,
    ],
    "attack": [2, 7, 12, 18, 23, 28, 33, 38, 43, 49, 54, 59],
    "damaged": [2, 8, 14, 19, 25, 31, 37, 43, 49, 54, 60, 66],
    "killed": [2, 5, 8, 12, 15, 18, 21, 24, 27, 31, 34, 37],
}


def crop_frame(sheet: Image.Image, frame_number: int) -> Image.Image:
    if sheet.width % GRID or sheet.height % GRID:
        raise ValueError(f"{sheet.width}x{sheet.height} is not an exact 9x9 sheet")
    frame_width = sheet.width // GRID
    frame_height = sheet.height // GRID
    index = frame_number - 1
    left = (index % GRID) * frame_width
    top = (index // GRID) * frame_height
    frame = sheet.crop((left, top, left + frame_width, top + frame_height))
    return Image.fromarray(
        zero_transparent_rgb(np.asarray(frame.convert("RGBA"))), "RGBA"
    )


def body_core_x(image: Image.Image) -> float:
    """Find Telos's dark torso without letting the cane or spell pull x."""
    array = np.asarray(image.convert("RGBA"))
    box = bounds(image)
    if box is None:
        return image.width / 2.0
    left, top, right, bottom = box
    height = bottom - top + 1
    yy, _xx = np.indices(array.shape[:2])
    brightness = np.mean(array[..., :3], axis=2)
    core = (
        (array[..., 3] > 40)
        & (brightness < 160)
        & (yy >= top + round(height * 0.10))
        & (yy <= top + round(height * 0.72))
    )
    _ys, xs = np.nonzero(core)
    if len(xs) < 20:
        return (left + right) / 2.0
    return float(np.median(xs))


def ground_contact(image: Image.Image) -> tuple[float, float]:
    """Find the lower boot/body contact while excluding the offset cane."""
    array = np.asarray(image.convert("RGBA"))
    alpha = array[..., 3]
    box = bounds(image)
    if box is None:
        return image.width / 2.0, image.height - 1.0
    left, top, right, bottom = box
    width = right - left + 1
    height = bottom - top + 1
    core_x = body_core_x(image)
    half_width = max(42, round(width * 0.35))
    yy, xx = np.indices(alpha.shape)
    brightness = np.mean(array[..., :3], axis=2)
    contact = (
        (alpha > 10)
        & (brightness < 200)
        & (xx >= core_x - half_width)
        & (xx <= core_x + half_width)
        & (yy >= top + round(height * 0.48))
    )
    ys, xs = np.nonzero(contact)
    if len(xs) < 5:
        return core_x, float(bottom)
    low = int(ys.max())
    near_low = ys >= low - 5
    return float(np.median(xs[near_low])), float(low)


def animation_anchor(animation: str, image: Image.Image) -> tuple[float, float]:
    if animation == "walk":
        return body_core_x(image), ground_contact(image)[1]
    if animation == "damaged":
        return ground_contact(image)
    if animation == "killed":
        return body_core_x(image), ground_contact(image)[1]
    return ground_contact(image)


def load_raw_frames() -> dict[str, list[Image.Image]]:
    output: dict[str, list[Image.Image]] = {}
    for animation, frame_numbers in FRAME_SELECTIONS.items():
        source_path = SOURCE / f"SS_telosTheMerchant_{SOURCE_NAMES[animation]}.png"
        with Image.open(source_path) as opened:
            sheet = opened.convert("RGBA")
        if sheet.width % GRID or sheet.height % GRID:
            raise ValueError(f"{source_path.name} is not an exact 9x9 sheet")
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
    scale = min(MAX_CONTENT_WIDTH / widest, MAX_CONTENT_HEIGHT / tallest, 1.0)

    for animation, images in raw.items():
        if animation == "attack":
            continue
        relative_left: list[float] = []
        relative_right: list[float] = []
        relative_top: list[float] = []
        relative_bottom: list[float] = []
        for image in images:
            box = bounds(image)
            if box is None:
                continue
            anchor_x, anchor_y = animation_anchor(animation, image)
            relative_left.append(box[0] - anchor_x)
            relative_right.append(box[2] - anchor_x)
            relative_top.append(box[1] - anchor_y)
            relative_bottom.append(box[3] - anchor_y)
        width = max(relative_right) - min(relative_left) + 1
        height = max(relative_bottom) - min(relative_top) + 1
        scale = min(scale, MAX_CONTENT_WIDTH / width, MAX_CONTENT_HEIGHT / height)
    return scale


def fitted_target_x(
    images: list[Image.Image],
    anchor_for: Callable[[Image.Image], tuple[float, float]],
) -> int:
    lower = -10_000.0
    upper = 10_000.0
    centered: list[float] = []
    for image in images:
        box = bounds(image)
        if box is None:
            continue
        anchor_x, _anchor_y = anchor_for(image)
        left, _top, right, _bottom = box
        lower = max(lower, anchor_x + 2 - left)
        upper = min(upper, anchor_x + CELL - 3 - right)
        centered.append(128.0 + anchor_x - (left + right) / 2.0)
    if lower > upper:
        raise RuntimeError(f"anchored frames cannot fit: {lower:.2f}>{upper:.2f}")
    requested = float(np.median(centered))
    return round(min(max(requested, lower), upper))


def grounded_placements(
    animation: str, images: list[Image.Image]
) -> tuple[list[tuple[int, int]], tuple[int, int]]:
    anchor_for = lambda image: animation_anchor(animation, image)
    target_x = fitted_target_x(images, anchor_for)
    placements: list[tuple[int, int]] = []
    for image in images:
        anchor_x, anchor_y = anchor_for(image)
        placements.append((round(target_x - anchor_x), round(GROUND_Y - anchor_y)))
    return placements, (target_x, GROUND_Y)


def fixed_attack_placements(
    images: list[Image.Image],
) -> tuple[list[tuple[int, int]], tuple[int, int]]:
    boxes = [box for image in images if (box := bounds(image)) is not None]
    union_left = min(box[0] for box in boxes)
    union_top = min(box[1] for box in boxes)
    union_right = max(box[2] for box in boxes)
    union_bottom = max(box[3] for box in boxes)
    foot_x, foot_y = ground_contact(images[0])

    requested_x = round(128 - (union_left + union_right) / 2.0)
    requested_y = round(GROUND_Y - foot_y)
    local_x = min(max(requested_x, 2 - union_left), CELL - 3 - union_right)
    local_y = min(max(requested_y, 2 - union_top), CELL - 3 - union_bottom)
    placement = (local_x, local_y)
    return [placement] * len(images), (round(foot_x + local_x), round(foot_y + local_y))


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
                local_x + box[0], local_y + box[1],
                local_x + box[2], local_y + box[3],
            )
            if (
                output_box[0] < 2 or output_box[1] < 2
                or output_box[2] > CELL - 3 or output_box[3] > CELL - 3
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
    path = OUTPUT / f"telosTheMerchant-{animation}.png"
    repaired.save(path, format="PNG", optimize=False)
    return path, cleanup


def make_source_preview(animation: str) -> Path:
    source_path = SOURCE / f"SS_telosTheMerchant_{SOURCE_NAMES[animation]}.png"
    with Image.open(source_path) as opened:
        sheet = opened.convert("RGBA")
    preview_cell = 180
    canvas = Image.new("RGBA", (9 * preview_cell, 9 * preview_cell), (28, 24, 35, 255))
    draw = ImageDraw.Draw(canvas)
    for frame_number in range(1, 82):
        frame = crop_frame(sheet, frame_number)
        frame.thumbnail((preview_cell - 4, preview_cell - 4), Image.Resampling.LANCZOS)
        x = ((frame_number - 1) % 9) * preview_cell
        y = ((frame_number - 1) // 9) * preview_cell
        for py in range(y, y + preview_cell, 15):
            for px in range(x, x + preview_cell, 15):
                color = (
                    (55, 50, 64, 255)
                    if ((px // 15 + py // 15) % 2)
                    else (38, 34, 46, 255)
                )
                draw.rectangle((px, py, px + 14, py + 14), fill=color)
        canvas.alpha_composite(frame, (x + 2, y + 2))
        draw.rectangle((x + 3, y + 3, x + 40, y + 20), fill=(0, 0, 0, 220))
        draw.text((x + 8, y + 5), str(frame_number), fill="white")
    SOURCE_REVIEW.mkdir(parents=True, exist_ok=True)
    output_path = SOURCE_REVIEW / f"telosTheMerchant-{animation}-source.png"
    canvas.convert("RGB").save(output_path, format="PNG")
    return output_path


def make_contact_sheet(animation: str, sheet: Image.Image) -> Path:
    frame_count = len(FRAME_SELECTIONS[animation])
    preview_cell = 192
    contact = Image.new("RGBA", (6 * preview_cell, 4 * preview_cell), (28, 24, 35, 255))
    draw = ImageDraw.Draw(contact)
    for index, source_number in enumerate(FRAME_SELECTIONS[animation]):
        frame = sheet.crop((index * CELL, 0, (index + 1) * CELL, CELL))
        frame.thumbnail((preview_cell, preview_cell), Image.Resampling.LANCZOS)
        x = (index % 6) * preview_cell
        y = (index // 6) * preview_cell
        for py in range(y, y + preview_cell, 16):
            for px in range(x, x + preview_cell, 16):
                color = (
                    (55, 50, 64, 255)
                    if ((px // 16 + py // 16) % 2)
                    else (38, 34, 46, 255)
                )
                draw.rectangle((px, py, px + 15, py + 15), fill=color)
        contact.alpha_composite(frame, (x, y))
        draw.rectangle((x, y, x + 61, y + 17), fill=(0, 0, 0, 210))
        draw.text((x + 4, y + 2), f"{index + 1}: {source_number}", fill="white")
    used_rows = (frame_count + 5) // 6
    contact = contact.crop((0, 0, contact.width, used_rows * preview_cell))
    REVIEW.mkdir(parents=True, exist_ok=True)
    path = REVIEW / f"telosTheMerchant-{animation}-contact.png"
    contact.convert("RGB").save(path, format="PNG")
    return path


def make_edge_preview(animation: str, sheet: Image.Image) -> Path:
    frame_count = len(FRAME_SELECTIONS[animation])
    sample_indices = sorted({0, frame_count // 3, (2 * frame_count) // 3, frame_count - 1})
    backgrounds = [(0, 0, 0), (38, 18, 54), (0, 210, 70), (112, 112, 112)]
    canvas = Image.new("RGB", (len(sample_indices) * CELL, len(backgrounds) * CELL))
    for row, background in enumerate(backgrounds):
        for column, frame_index in enumerate(sample_indices):
            frame = sheet.crop((frame_index * CELL, 0, (frame_index + 1) * CELL, CELL))
            base = Image.new("RGBA", (CELL, CELL), (*background, 255))
            base.alpha_composite(frame)
            canvas.paste(base.convert("RGB"), (column * CELL, row * CELL))
    REVIEW.mkdir(parents=True, exist_ok=True)
    path = REVIEW / f"telosTheMerchant-{animation}-edges.png"
    canvas.save(path, format="PNG")
    return path


def validate_output(
    animation: str,
    path: Path,
    placements: list[tuple[int, int]],
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
    if animation == "attack" and len(set(placements)) != 1:
        failures.append("attack frames do not use one fixed placement")

    anchor_x_values: list[float] = []
    anchor_y_values: list[float] = []
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
        if animation != "attack":
            anchor_x, anchor_y = animation_anchor(animation, frame)
            anchor_x_values.append(anchor_x)
            anchor_y_values.append(anchor_y)

    if anchor_x_values and max(abs(value - target[0]) for value in anchor_x_values) > 1.5:
        failures.append("horizontal anchor drift exceeds 1.5px")
    if anchor_y_values and max(abs(value - target[1]) for value in anchor_y_values) > 1.5:
        failures.append("ground anchor drift exceeds 1.5px")

    return {
        "animation": animation,
        "path": str(path),
        "size": list(image.size),
        "mode": source_mode,
        "frames": frame_count,
        "source_grid": [9, 9],
        "source_frames": FRAME_SELECTIONS[animation],
        "target_anchor": list(target),
        "anchor_x_range": [min(anchor_x_values), max(anchor_x_values)] if anchor_x_values else None,
        "anchor_y_range": [min(anchor_y_values), max(anchor_y_values)] if anchor_y_values else None,
        "frame_bounds": frame_bounds,
        "placements": placements,
        "suspicious_final": suspicious_count(array),
        "failures": failures,
    }


def main() -> None:
    for animation in SOURCE_NAMES:
        make_source_preview(animation)

    raw = load_raw_frames()
    scale = shared_scale(raw)
    resized = {
        animation: [
            premultiplied_resize(image, (round(image.width * scale), round(image.height * scale)))
            for image in images
        ]
        for animation, images in raw.items()
    }

    placements: dict[str, list[tuple[int, int]]] = {}
    targets: dict[str, tuple[int, int]] = {}
    for animation, images in resized.items():
        if animation == "attack":
            placements[animation], targets[animation] = fixed_attack_placements(images)
        else:
            placements[animation], targets[animation] = grounded_placements(animation, images)

    report: list[dict[str, object]] = []
    for animation, images in resized.items():
        path, cleanup = assemble(animation, images, placements[animation])
        with Image.open(path) as opened:
            sheet = opened.convert("RGBA")
        make_contact_sheet(animation, sheet)
        make_edge_preview(animation, sheet)
        result = validate_output(animation, path, placements[animation], targets[animation])
        result["scale"] = scale
        result["halo_cleanup"] = cleanup
        report.append(result)
        print(
            f"{path.name}: {sheet.width}x{sheet.height}, "
            f"{len(FRAME_SELECTIONS[animation])} frames, "
            f"{cleanup['pixels_recolored']} halo pixels recolored, "
            f"suspicious {cleanup['suspicious_before']}->{cleanup['suspicious_after']}"
        )

    REVIEW.mkdir(parents=True, exist_ok=True)
    report_path = REVIEW / "telosTheMerchant-validation.json"
    report_path.write_text(json.dumps(report, indent=2), encoding="ascii")
    failures = [item for item in report if item["failures"]]
    if failures:
        raise RuntimeError(f"Telos validation failed; see {report_path}")
    print(f"Shared scale: {scale:.6f}")
    print(f"Validation passed: {report_path}")


if __name__ == "__main__":
    main()
