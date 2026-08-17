from __future__ import annotations

import json
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
GRID = 9
GROUND_Y = 244
MAX_CONTENT_WIDTH = 246
MAX_CONTENT_HEIGHT = 238

ROOT = Path(r"C:\Users\jarox\OneDrive\Desktop\Tandem Tales\Bayou Bonanza")
SOURCE = Path(r"C:\Users\jarox\OneDrive\Desktop\TT Temp")
OUTPUT = ROOT / "assets" / "animations"
REVIEW = ROOT / "tools" / "fatherGrub-review"

SOURCE_NAMES = {
    "walk": "walking",
    "attack": "attack",
    "damaged": "damaged",
    "killed": "killed",
}

# The three looping/recovering actions use an even sample of frames 2-80.
# Death stops at frame 50, after the collapse and before the later head lift.
FRAME_SELECTIONS = {
    "walk": [
        2, 5, 9, 12, 16, 19, 22, 26, 29, 33, 36, 39,
        43, 46, 49, 53, 56, 60, 63, 66, 70, 73, 77, 80,
    ],
    "attack": [2, 9, 16, 23, 30, 37, 44, 51, 58, 65, 72, 80],
    "damaged": [2, 9, 16, 23, 30, 37, 44, 51, 58, 65, 72, 80],
    "killed": [2, 6, 11, 15, 19, 24, 28, 33, 37, 41, 46, 50],
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


def mask_bounds(mask: np.ndarray) -> tuple[int, int, int, int] | None:
    ys, xs = np.nonzero(mask)
    if len(xs) == 0:
        return None
    return int(xs.min()), int(ys.min()), int(xs.max()), int(ys.max())


def torso_x(image: Image.Image) -> float:
    """Track Father Grub's broad body instead of independently moving legs."""
    array = np.asarray(image.convert("RGBA"))
    alpha = array[..., 3]
    visible = alpha > 28
    box = mask_bounds(visible)
    if box is None:
        return image.width / 2.0
    left, top, right, bottom = box
    width = right - left + 1
    height = bottom - top + 1

    solid = np.asarray(
        Image.fromarray(np.where(visible, 255, 0).astype(np.uint8), "L").filter(
            ImageFilter.MinFilter(15)
        )
    ) > 0
    yy, xx = np.indices(solid.shape)
    solid &= (
        (xx >= left + round(width * 0.10))
        & (xx <= left + round(width * 0.90))
        & (yy >= top + round(height * 0.04))
        & (yy <= top + round(height * 0.70))
    )
    ys, xs = np.nonzero(solid)
    if len(xs) >= 30:
        return float(np.median(xs))

    fallback = visible & (
        (xx >= left + round(width * 0.18))
        & (xx <= left + round(width * 0.82))
        & (yy <= top + round(height * 0.68))
    )
    _ys, xs = np.nonzero(fallback)
    if len(xs) >= 20:
        return float(np.median(xs))
    return (left + right) / 2.0


def ground_y(image: Image.Image) -> float:
    """Use the lowest substantial contact while ignoring isolated noise."""
    array = np.asarray(image.convert("RGBA"))
    alpha = array[..., 3]
    visible = alpha > 28
    box = mask_bounds(visible)
    if box is None:
        return image.height / 2.0
    left, top, right, bottom = box
    width = right - left + 1
    height = bottom - top + 1
    yy, xx = np.indices(visible.shape)
    lower = visible & (
        (xx >= left + round(width * 0.05))
        & (xx <= right - round(width * 0.05))
        & (yy >= top + round(height * 0.55))
    )
    row_counts = np.count_nonzero(lower, axis=1)
    rows = np.flatnonzero(row_counts >= 3)
    return float(rows.max()) if len(rows) else float(bottom)


def animation_anchor(image: Image.Image) -> tuple[float, float]:
    return torso_x(image), ground_y(image)


def load_raw_frames() -> dict[str, list[Image.Image]]:
    output: dict[str, list[Image.Image]] = {}
    for animation, frame_numbers in FRAME_SELECTIONS.items():
        source_path = SOURCE / f"SS_fatherGrub_{SOURCE_NAMES[animation]}.png"
        with Image.open(source_path) as opened:
            sheet = opened.convert("RGBA")
        if sheet.width % GRID or sheet.height % GRID:
            raise ValueError(f"{source_path.name} is not an exact 9x9 sheet")
        output[animation] = [
            harden_interior_alpha(
                remove_tiny_components(crop_frame(sheet, frame_number), 4)
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
    for images in raw.values():
        lefts: list[float] = []
        rights: list[float] = []
        tops: list[float] = []
        bottoms: list[float] = []
        for image in images:
            box = bounds(image)
            if box is None:
                continue
            anchor_x, anchor_y = animation_anchor(image)
            lefts.append(box[0] - anchor_x)
            rights.append(box[2] - anchor_x)
            tops.append(box[1] - anchor_y)
            bottoms.append(box[3] - anchor_y)
        scale = min(
            scale,
            MAX_CONTENT_WIDTH / (max(rights) - min(lefts) + 1),
            MAX_CONTENT_HEIGHT / (max(bottoms) - min(tops) + 1),
        )
    return scale


def fitted_target_x(images: list[Image.Image]) -> int:
    lower = -10_000.0
    upper = 10_000.0
    centered: list[float] = []
    for image in images:
        box = bounds(image)
        if box is None:
            continue
        anchor_x, _anchor_y = animation_anchor(image)
        lower = max(lower, anchor_x + 3 - box[0])
        upper = min(upper, anchor_x + CELL - 4 - box[2])
        centered.append(128.0 + anchor_x - (box[0] + box[2]) / 2.0)
    if lower > upper:
        raise RuntimeError(f"Cannot fit anchored frames: {lower:.2f}>{upper:.2f}")
    return round(min(max(float(np.median(centered)), lower), upper))


def anchored_placements(
    images: list[Image.Image],
) -> tuple[list[tuple[int, int]], tuple[int, int]]:
    target_x = fitted_target_x(images)
    placements = []
    for image in images:
        anchor_x, anchor_y = animation_anchor(image)
        placements.append((round(target_x - anchor_x), round(GROUND_Y - anchor_y)))
    return placements, (target_x, GROUND_Y)


def assemble(
    animation: str,
    images: list[Image.Image],
    placements: list[tuple[int, int]],
) -> tuple[Path, dict[str, int]]:
    sheet = Image.new("RGBA", (CELL * len(images), CELL), (0, 0, 0, 0))
    for index, (image, (local_x, local_y)) in enumerate(
        zip(images, placements, strict=True)
    ):
        box = bounds(image)
        if box is not None:
            output_box = (
                local_x + box[0],
                local_y + box[1],
                local_x + box[2],
                local_y + box[3],
            )
            if (
                output_box[0] < 3
                or output_box[1] < 3
                or output_box[2] > CELL - 4
                or output_box[3] > CELL - 4
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
    path = OUTPUT / f"fatherGrub-{animation}.png"
    repaired.save(path, format="PNG", optimize=False)
    return path, cleanup


def make_contact_sheet(animation: str, sheet: Image.Image) -> Path:
    frame_count = len(FRAME_SELECTIONS[animation])
    columns = 6
    rows = (frame_count + columns - 1) // columns
    canvas = Image.new("RGBA", (columns * CELL, rows * CELL), (0, 0, 0, 255))
    draw = ImageDraw.Draw(canvas)
    tile = 16
    for y in range(canvas.height):
        for x in range(canvas.width):
            value = 45 if ((x // tile + y // tile) % 2 == 0) else 59
            canvas.putpixel((x, y), (value, value - 3, value + 8, 255))
    for index, source_number in enumerate(FRAME_SELECTIONS[animation]):
        frame = sheet.crop((index * CELL, 0, (index + 1) * CELL, CELL))
        x = (index % columns) * CELL
        y = (index // columns) * CELL
        canvas.alpha_composite(frame, (x, y))
        draw.rectangle((x, y, x + 64, y + 22), fill=(0, 0, 0, 230))
        draw.text((x + 4, y + 4), f"{index + 1}:{source_number}", fill="white")
    REVIEW.mkdir(parents=True, exist_ok=True)
    path = REVIEW / f"fatherGrub-{animation}-contact.png"
    canvas.save(path, format="PNG")
    return path


def make_edge_preview(animation: str, sheet: Image.Image) -> Path:
    frame_count = len(FRAME_SELECTIONS[animation])
    indices = sorted({0, frame_count // 3, (2 * frame_count) // 3, frame_count - 1})
    backgrounds = [(0, 0, 0), (34, 15, 52), (0, 220, 70), (112, 112, 112)]
    canvas = Image.new("RGB", (4 * CELL, 4 * CELL), "black")
    for row, background in enumerate(backgrounds):
        for column, index in enumerate(indices):
            frame = sheet.crop((index * CELL, 0, (index + 1) * CELL, CELL))
            base = Image.new("RGBA", (CELL, CELL), (*background, 255))
            base.alpha_composite(frame)
            canvas.paste(base.convert("RGB"), (column * CELL, row * CELL))
    REVIEW.mkdir(parents=True, exist_ok=True)
    path = REVIEW / f"fatherGrub-{animation}-edges.png"
    canvas.save(path, format="PNG")
    return path


def validate_output(
    animation: str,
    path: Path,
    source_images: list[Image.Image],
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
        else:
            frame_bounds.append(list(box))

    placed_anchors = [
        (
            animation_anchor(source)[0] + placement[0],
            animation_anchor(source)[1] + placement[1],
        )
        for source, placement in zip(source_images, placements, strict=True)
    ]
    placed_x = [anchor[0] for anchor in placed_anchors]
    placed_y = [anchor[1] for anchor in placed_anchors]
    if max(abs(value - target[0]) for value in placed_x) > 1.5:
        failures.append("horizontal anchor drift exceeds 1.5px")
    if max(abs(value - target[1]) for value in placed_y) > 1.5:
        failures.append("vertical anchor drift exceeds 1.5px")

    return {
        "animation": animation,
        "path": str(path),
        "size": list(image.size),
        "mode": source_mode,
        "frames": frame_count,
        "source_grid": [GRID, GRID],
        "source_frames": FRAME_SELECTIONS[animation],
        "target_anchor": list(target),
        "placed_anchor_x_range": [min(placed_x), max(placed_x)],
        "placed_anchor_y_range": [min(placed_y), max(placed_y)],
        "frame_bounds": frame_bounds,
        "placements": placements,
        "suspicious_final": suspicious_count(array),
        "failures": failures,
    }


def main() -> None:
    raw = load_raw_frames()
    scale = shared_scale(raw)
    resized = {
        animation: [
            premultiplied_resize(
                image, (round(image.width * scale), round(image.height * scale))
            )
            for image in images
        ]
        for animation, images in raw.items()
    }

    placements: dict[str, list[tuple[int, int]]] = {}
    targets: dict[str, tuple[int, int]] = {}
    for animation, images in resized.items():
        placements[animation], targets[animation] = anchored_placements(images)

    report: list[dict[str, object]] = []
    for animation, images in resized.items():
        path, cleanup = assemble(animation, images, placements[animation])
        with Image.open(path) as opened:
            sheet = opened.convert("RGBA")
        make_contact_sheet(animation, sheet)
        make_edge_preview(animation, sheet)
        result = validate_output(
            animation,
            path,
            resized[animation],
            placements[animation],
            targets[animation],
        )
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
    report_path = REVIEW / "fatherGrub-validation.json"
    report_path.write_text(json.dumps(report, indent=2), encoding="ascii")
    failures = [item for item in report if item["failures"]]
    if failures:
        raise RuntimeError(f"Father Grub validation failed; see {report_path}")
    print(f"Shared scale: {scale:.6f}")
    print(f"Validation passed: {report_path}")


if __name__ == "__main__":
    main()
