from __future__ import annotations

import argparse
import math
from pathlib import Path

import numpy as np
from PIL import Image, ImageDraw, ImageFilter


CELL = 256
GRID = 9
ALPHA_CUT = 10

ROOT = Path(r"C:\Users\jarox\OneDrive\Desktop\Tandem Tales\Bayou Bonanza")
SOURCE = Path(r"C:\Users\jarox\OneDrive\Desktop\TT Temp")
OUTPUT = ROOT / "assets" / "animations"
REVIEW = ROOT / "tools" / "blackthornEngineer-review"

FRAME_SELECTIONS = {
    "walk": [
        2, 5, 9, 12, 16, 19, 22, 26, 29, 33, 36, 39,
        43, 46, 49, 53, 56, 60, 63, 66, 70, 73, 77, 80,
    ],
    "attack": [2, 9, 16, 23, 30, 37, 45, 52, 59, 66, 73, 80],
    "damaged": [2, 9, 16, 23, 30, 37, 45, 52, 59, 66, 73, 80],
    # The recovery begins after frame 45. End on the stable prone phase instead.
    "killed": [2, 5, 8, 11, 14, 17, 20, 23, 27, 32, 37, 42],
}

SOURCE_NAMES = {
    "walk": "walking",
    "attack": "attack",
    "damaged": "damaged",
    "killed": "killed",
}


def zero_transparent_rgb(array: np.ndarray) -> np.ndarray:
    output = array.copy()
    output[output[..., 3] <= ALPHA_CUT] = 0
    output[output[..., 3] == 0, :3] = 0
    return output


def crop_frame(sheet: Image.Image, frame_number: int) -> Image.Image:
    frame_width = sheet.width // GRID
    frame_height = sheet.height // GRID
    index = frame_number - 1
    left = (index % GRID) * frame_width
    top = (index // GRID) * frame_height
    array = np.asarray(
        sheet.crop((left, top, left + frame_width, top + frame_height)).convert("RGBA")
    ).copy()

    # Source art occasionally reaches a cell boundary. Clearing only the exact
    # boundary prevents that pixel from wrapping into the neighboring frame.
    array[0] = 0
    array[-1] = 0
    array[:, 0] = 0
    array[:, -1] = 0
    return Image.fromarray(zero_transparent_rgb(array), "RGBA")


def remove_tiny_components(image: Image.Image, minimum_pixels: int = 14) -> Image.Image:
    array = zero_transparent_rgb(np.asarray(image.convert("RGBA")))
    visible = array[..., 3] > ALPHA_CUT
    height, width = visible.shape
    seen = np.zeros_like(visible, dtype=bool)

    for start_y in range(height):
        for start_x in range(width):
            if seen[start_y, start_x] or not visible[start_y, start_x]:
                continue
            stack = [(start_x, start_y)]
            seen[start_y, start_x] = True
            points: list[tuple[int, int]] = []
            while stack:
                x, y = stack.pop()
                points.append((x, y))
                for next_x, next_y in (
                    (x + 1, y),
                    (x - 1, y),
                    (x, y + 1),
                    (x, y - 1),
                ):
                    if (
                        0 <= next_x < width
                        and 0 <= next_y < height
                        and visible[next_y, next_x]
                        and not seen[next_y, next_x]
                    ):
                        seen[next_y, next_x] = True
                        stack.append((next_x, next_y))
            if len(points) < minimum_pixels:
                for x, y in points:
                    array[y, x] = 0

    return Image.fromarray(array, "RGBA")


def harden_interior_alpha(image: Image.Image) -> Image.Image:
    array = zero_transparent_rgb(np.asarray(image.convert("RGBA")))
    array[array[..., 3] >= 196, 3] = 255
    visible = Image.fromarray(
        np.where(array[..., 3] > 24, 255, 0).astype(np.uint8), "L"
    )
    interior = np.asarray(visible.filter(ImageFilter.MinFilter(5))) == 255
    array[interior, 3] = 255
    return Image.fromarray(zero_transparent_rgb(array), "RGBA")


def bounds(image: Image.Image) -> tuple[int, int, int, int] | None:
    alpha = np.asarray(image.convert("RGBA"))[..., 3]
    ys, xs = np.nonzero(alpha > ALPHA_CUT)
    if len(xs) == 0:
        return None
    return int(xs.min()), int(ys.min()), int(xs.max()), int(ys.max())


def body_core_x(image: Image.Image) -> float:
    box = bounds(image)
    if box is None:
        return image.width / 2.0
    left, top, right, bottom = box
    alpha = np.asarray(image.convert("RGBA"))[..., 3]
    height = max(1, bottom - top + 1)
    y0 = top + round(height * 0.18)
    y1 = top + round(height * 0.64)
    _ys, xs = np.nonzero(alpha[y0 : y1 + 1] > ALPHA_CUT)
    if len(xs) < 20:
        return (left + right) / 2.0
    return float(np.median(xs))


def lower_contact(
    image: Image.Image,
    minimum_x: int | None = None,
    maximum_x: int | None = None,
) -> tuple[float, float]:
    alpha = np.asarray(image.convert("RGBA"))[..., 3]
    y_grid, x_grid = np.indices(alpha.shape)
    contact = alpha > ALPHA_CUT
    if minimum_x is not None:
        contact &= x_grid >= minimum_x
    if maximum_x is not None:
        contact &= x_grid <= maximum_x
    ys, _xs = np.nonzero(contact)
    if len(ys) == 0:
        raise RuntimeError("Could not locate the support foot")
    bottom = int(ys.max())
    band = contact[max(0, bottom - 18) : bottom + 1]
    _band_y, band_x = np.nonzero(band)
    if len(band_x) < 5:
        return body_core_x(image), float(bottom)
    return float(np.median(band_x)), float(bottom)


def prepare_frames(animation: str) -> list[Image.Image]:
    path = SOURCE / f"SS_blackthornEngineer_{SOURCE_NAMES[animation]}.png"
    with Image.open(path) as opened:
        sheet = opened.convert("RGBA")
    if sheet.width % GRID or sheet.height % GRID:
        raise ValueError(f"{path.name} is not an exact {GRID}x{GRID} sheet")
    return [
        harden_interior_alpha(remove_tiny_components(crop_frame(sheet, number)))
        for number in FRAME_SELECTIONS[animation]
    ]


def walk_placements(images: list[Image.Image]) -> list[tuple[int, int]]:
    placements = []
    for image in images:
        box = bounds(image)
        if box is None:
            placements.append((0, 0))
            continue
        # Center the torso horizontally while grounding whichever boot is in
        # contact. This preserves the alternating stride without lateral drift.
        placements.append((round(128 - body_core_x(image)), 244 - box[3]))
    return placements


def attack_placements(images: list[Image.Image]) -> list[tuple[int, int]]:
    placements = []
    for image in images:
        # The long viewer-left boot is the support foot. In the source it stays
        # at approximately x=40, y=230 throughout the entire knife motion.
        anchor_x, anchor_y = lower_contact(image, 24, 58)
        placements.append((round(90 - anchor_x), round(244 - anchor_y)))
    return placements


def damaged_placements(images: list[Image.Image]) -> list[tuple[int, int]]:
    placements = []
    for image in images:
        box = bounds(image)
        if box is None:
            placements.append((0, 0))
            continue
        # The lower boot changes during the wide recoil. Ground the true lowest
        # contact vertically, but keep horizontal placement tied to the torso so
        # that switching feet cannot drag the entire sprite across the cell.
        anchor_x, _anchor_y = lower_contact(image, 10, 80)
        placements.append((round(100 - anchor_x), 244 - box[3]))
    return placements


def killed_placements(images: list[Image.Image]) -> list[tuple[int, int]]:
    downed_start = 7
    first_x = round(128 - body_core_x(images[0]))

    downed_boxes = [bounds(image) for image in images[downed_start:]]
    downed_boxes = [box for box in downed_boxes if box is not None]
    final_center = float(np.median([(box[0] + box[2]) / 2.0 for box in downed_boxes]))
    final_bottom = int(round(float(np.median([box[3] for box in downed_boxes]))))
    final_x = round(128 - final_center)
    final_y = 244 - final_bottom

    placements: list[tuple[int, int]] = []
    for index, image in enumerate(images):
        box = bounds(image)
        if box is None:
            placements.append((0, 0))
            continue
        if index >= downed_start:
            placements.append((final_x, final_y))
            continue

        progress = index / float(downed_start)
        smooth = progress * progress * (3.0 - 2.0 * progress)
        x = round(first_x + (final_x - first_x) * smooth)
        grounded_y = 244 - box[3]
        placements.append((x, grounded_y))
    return placements


def assemble(
    animation: str,
    images: list[Image.Image],
    placements: list[tuple[int, int]],
) -> Path:
    frame_numbers = FRAME_SELECTIONS[animation]
    sheet = Image.new("RGBA", (CELL * len(images), CELL), (0, 0, 0, 0))
    for index, (image, (local_x, local_y)) in enumerate(zip(images, placements)):
        box = bounds(image)
        if box is not None:
            left, top, right, bottom = box
            output_box = (
                local_x + left,
                local_y + top,
                local_x + right,
                local_y + bottom,
            )
            if (
                output_box[0] < 2
                or output_box[1] < 2
                or output_box[2] > CELL - 3
                or output_box[3] > CELL - 3
            ):
                raise RuntimeError(
                    f"{animation} source frame {frame_numbers[index]} clips at {output_box}"
                )
        sheet.alpha_composite(image, (index * CELL + local_x, local_y))

    array = zero_transparent_rgb(np.asarray(sheet))
    for index in range(len(images)):
        left = index * CELL
        array[:, left] = 0
        array[:, left + CELL - 1] = 0
        array[0, left : left + CELL] = 0
        array[CELL - 1, left : left + CELL] = 0

    OUTPUT.mkdir(parents=True, exist_ok=True)
    path = OUTPUT / f"blackthornEngineer-{animation}.png"
    Image.fromarray(array, "RGBA").save(path, format="PNG", optimize=False)
    print(path.name, Image.open(path).size, "frames=" + ",".join(map(str, frame_numbers)))
    return path


def build_all() -> None:
    placement_builders = {
        "walk": walk_placements,
        "attack": attack_placements,
        "damaged": damaged_placements,
        "killed": killed_placements,
    }
    for animation in ("walk", "attack", "damaged", "killed"):
        images = prepare_frames(animation)
        assemble(animation, images, placement_builders[animation](images))


def checkerboard() -> Image.Image:
    checker = Image.new("RGBA", (CELL, CELL), (42, 42, 42, 255))
    draw = ImageDraw.Draw(checker)
    for y in range(0, CELL, 16):
        for x in range(0, CELL, 16):
            if (x // 16 + y // 16) % 2:
                draw.rectangle((x, y, x + 15, y + 15), fill=(72, 64, 78, 255))
    draw.line((0, 244, CELL - 1, 244), fill=(82, 190, 140, 255), width=1)
    return checker


def write_reviews() -> None:
    REVIEW.mkdir(parents=True, exist_ok=True)
    checker = checkerboard()
    for animation, frame_numbers in FRAME_SELECTIONS.items():
        path = OUTPUT / f"blackthornEngineer-{animation}.png"
        with Image.open(path) as opened:
            sheet = opened.convert("RGBA")
        frames = [
            sheet.crop((index * CELL, 0, (index + 1) * CELL, CELL))
            for index in range(len(frame_numbers))
        ]
        columns = 6
        rows = math.ceil(len(frames) / columns)
        contact = Image.new("RGBA", (columns * CELL, rows * CELL), (28, 28, 28, 255))
        gif_frames = []
        for index, (frame, source_number) in enumerate(zip(frames, frame_numbers)):
            tile = checker.copy()
            tile.alpha_composite(frame)
            ImageDraw.Draw(tile).text(
                (5, 5), f"{index + 1}: src {source_number}", fill="white"
            )
            contact.alpha_composite(
                tile, ((index % columns) * CELL, (index // columns) * CELL)
            )
            gif_frames.append(tile.convert("P", palette=Image.Palette.ADAPTIVE))
        contact.convert("RGB").save(REVIEW / f"blackthornEngineer-{animation}-contact.png")
        gif_frames[0].save(
            REVIEW / f"blackthornEngineer-{animation}.gif",
            save_all=True,
            append_images=gif_frames[1:],
            duration=85,
            loop=0,
            disposal=2,
        )


def main() -> None:
    parser = argparse.ArgumentParser()
    parser.add_argument("--review-only", action="store_true")
    args = parser.parse_args()
    if not args.review_only:
        build_all()
    write_reviews()


if __name__ == "__main__":
    main()
