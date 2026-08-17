from __future__ import annotations

import json
from collections.abc import Callable
from pathlib import Path

import numpy as np
from PIL import Image, ImageDraw, ImageFilter

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
GROUND_Y = 244
MAX_CONTENT_WIDTH = 246
MAX_CONTENT_HEIGHT = 238

ROOT = Path(r"C:\Users\jarox\OneDrive\Desktop\Tandem Tales\Bayou Bonanza")
SOURCE = Path(r"C:\Users\jarox\OneDrive\Desktop\TT Temp")
OUTPUT = ROOT / "assets" / "animations"
REVIEW = ROOT / "tools" / "rowan-review"
SOURCE_REVIEW = ROOT / "tools" / "rowan-source-previews"

SOURCE_SPECS = {
    "attack": ("attack", 6, 5),
    "damaged": ("damaged", 6, 5),
    "killed": ("killed", 6, 5),
    "walk": ("walking", 9, 9),
}

FRAME_SELECTIONS = {
    "walk": [
        2, 5, 9, 12, 16, 19, 22, 26, 29, 33, 36, 39,
        43, 46, 50, 53, 57, 60, 63, 67, 70, 74, 77, 80,
    ],
    "attack": [2, 4, 7, 9, 12, 14, 17, 19, 22, 24, 27, 29],
    "damaged": [2, 4, 7, 9, 12, 14, 17, 19, 22, 24, 27, 29],
    # Frames 14-15 begin preparing to rise; stop while Rowan is fully prone.
    "killed": list(range(2, 14)),
}


def crop_frame(
    sheet: Image.Image, frame_number: int, columns: int, rows: int
) -> Image.Image:
    if sheet.width % columns or sheet.height % rows:
        raise ValueError(
            f"{sheet.width}x{sheet.height} is not an exact {columns}x{rows} sheet"
        )
    frame_width = sheet.width // columns
    frame_height = sheet.height // rows
    index = frame_number - 1
    left = (index % columns) * frame_width
    top = (index // columns) * frame_height
    frame = sheet.crop((left, top, left + frame_width, top + frame_height))
    return Image.fromarray(
        zero_transparent_rgb(np.asarray(frame.convert("RGBA"))), "RGBA"
    )


def body_core_x(image: Image.Image) -> float:
    alpha = np.asarray(image.convert("RGBA"))[..., 3]
    box = bounds(image)
    if box is None:
        return image.width / 2.0
    left, top, right, bottom = box
    height = bottom - top + 1
    solid = Image.fromarray(
        np.where(alpha > 32, 255, 0).astype(np.uint8), "L"
    ).filter(ImageFilter.MinFilter(13))
    core = np.asarray(solid) > 0
    yy, _xx = np.indices(core.shape)
    core &= (yy >= top + round(height * 0.18)) & (
        yy <= top + round(height * 0.70)
    )
    _ys, xs = np.nonzero(core)
    if len(xs) < 20:
        region = alpha[
            top + round(height * 0.18) : top + round(height * 0.70) + 1
        ] > 32
        _ys, xs = np.nonzero(region)
    if len(xs) < 20:
        return (left + right) / 2.0
    return float(np.median(xs))


def lower_foot_anchor(image: Image.Image) -> tuple[float, float]:
    """Track boots near the torso while excluding the sword and shield."""
    alpha = np.asarray(image.convert("RGBA"))[..., 3]
    box = bounds(image)
    if box is None:
        return image.width / 2.0, image.height - 1.0
    left, top, right, bottom = box
    width = right - left + 1
    height = bottom - top + 1
    core_x = body_core_x(image)
    half_width = max(48, round(width * 0.25))
    yy, xx = np.indices(alpha.shape)
    contact = (
        (alpha > 10)
        & (xx >= core_x - half_width)
        & (xx <= core_x + half_width)
        & (yy >= top + round(height * 0.52))
    )
    ys, xs = np.nonzero(contact)
    if len(xs) < 5:
        return core_x, float(bottom)
    low = int(ys.max())
    near_low = ys >= low - 5
    return float(np.median(xs[near_low])), float(low)


def animation_anchor(
    animation: str, image: Image.Image
) -> tuple[float, float]:
    box = bounds(image)
    if box is None:
        return image.width / 2.0, image.height - 1.0
    if animation == "walk":
        return body_core_x(image), lower_foot_anchor(image)[1]
    if animation in {"attack", "damaged"}:
        return lower_foot_anchor(image)
    return body_core_x(image), float(box[3])


def load_raw_frames() -> dict[str, list[Image.Image]]:
    output: dict[str, list[Image.Image]] = {}
    for animation, frame_numbers in FRAME_SELECTIONS.items():
        source_name, columns, rows = SOURCE_SPECS[animation]
        source_path = SOURCE / f"SS_rowan_{source_name}.png"
        with Image.open(source_path) as opened:
            sheet = opened.convert("RGBA")
        if sheet.width % columns or sheet.height % rows:
            raise ValueError(
                f"{source_path.name} is not an exact {columns}x{rows} sheet"
            )
        output[animation] = [
            harden_interior_alpha(
                remove_tiny_components(
                    crop_frame(sheet, frame_number, columns, rows), 5
                )
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

    # Also fit the union relative to each animation's chosen anchor. This
    # prevents stable anchoring from pushing a shield or sword through an edge.
    for animation, images in raw.items():
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
        placements.append(
            (round(target_x - anchor_x), round(GROUND_Y - anchor_y))
        )
    return placements, (target_x, GROUND_Y)


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
    path = OUTPUT / f"rowan-{animation}.png"
    repaired.save(path, format="PNG", optimize=False)
    return path, cleanup


def make_source_preview(animation: str) -> Path:
    source_name, columns, rows = SOURCE_SPECS[animation]
    source_path = SOURCE / f"SS_rowan_{source_name}.png"
    with Image.open(source_path) as opened:
        sheet = opened.convert("RGBA")
    preview_cell = 180
    canvas = Image.new(
        "RGBA", (columns * preview_cell, rows * preview_cell), (28, 24, 35, 255)
    )
    draw = ImageDraw.Draw(canvas)
    for frame_number in range(1, columns * rows + 1):
        frame = crop_frame(sheet, frame_number, columns, rows)
        frame.thumbnail((preview_cell - 4, preview_cell - 4), Image.Resampling.LANCZOS)
        x = ((frame_number - 1) % columns) * preview_cell
        y = ((frame_number - 1) // columns) * preview_cell
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
    output_path = SOURCE_REVIEW / f"rowan-{animation}-source.png"
    canvas.convert("RGB").save(output_path, format="PNG")
    return output_path


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
        draw.rectangle((x, y, x + 61, y + 17), fill=(0, 0, 0, 210))
        draw.text((x + 4, y + 2), f"{index + 1}: {source_number}", fill="white")
    REVIEW.mkdir(parents=True, exist_ok=True)
    path = REVIEW / f"rowan-{animation}-contact.png"
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
    path = REVIEW / f"rowan-{animation}-edges.png"
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
        "source_grid": list(SOURCE_SPECS[animation][1:]),
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
    for animation in SOURCE_SPECS:
        make_source_preview(animation)

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

    placements: dict[str, list[tuple[int, int]]] = {}
    targets: dict[str, tuple[int, int]] = {}
    for animation, images in resized.items():
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
    report_path = REVIEW / "rowan-validation.json"
    report_path.write_text(json.dumps(report, indent=2), encoding="ascii")
    failures = [item for item in report if item["failures"]]
    if failures:
        raise RuntimeError(f"Rowan validation failed; see {report_path}")
    print(f"Shared scale: {scale:.6f}")
    print(f"Validation passed: {report_path}")


if __name__ == "__main__":
    main()
