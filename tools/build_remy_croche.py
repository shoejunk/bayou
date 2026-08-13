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
REVIEW = ROOT / "tools" / "remyCroche-review"

FRAME_SELECTIONS = {
    "walk": [2, 5, 9, 12, 16, 19, 22, 26, 29, 33, 36, 39,
             43, 46, 49, 53, 56, 60, 63, 66, 70, 73, 77, 80],
    "attack": [2, 9, 16, 23, 30, 37, 45, 52, 59, 66, 73, 80],
    "damaged": [2, 9, 16, 23, 30, 37, 45, 52, 59, 66, 73, 80],
    # Stop before the subtle torso lift that precedes the obvious recovery phase.
    "killed": [2, 6, 10, 14, 18, 22, 26, 30, 34, 38, 42, 45],
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
    frame = np.asarray(
        sheet.crop((left, top, left + frame_width, top + frame_height)).convert("RGBA")
    ).copy()
    frame[0] = 0
    frame[-1] = 0
    frame[:, 0] = 0
    frame[:, -1] = 0
    return Image.fromarray(zero_transparent_rgb(frame), "RGBA")


def prepare_alpha(image: Image.Image) -> Image.Image:
    array = zero_transparent_rgb(np.asarray(image.convert("RGBA")))
    array[array[..., 3] >= 192, 3] = 255

    visible = Image.fromarray(
        np.where(array[..., 3] > 24, 255, 0).astype(np.uint8), "L"
    )
    interior = np.asarray(visible.filter(ImageFilter.MinFilter(5))) == 255
    array[interior, 3] = 255
    return Image.fromarray(zero_transparent_rgb(array), "RGBA")


def remove_tiny_components(image: Image.Image, minimum_pixels: int = 12) -> Image.Image:
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
    y0 = top + round(height * 0.20)
    y1 = top + round(height * 0.66)
    _ys, xs = np.nonzero(alpha[y0 : y1 + 1] > ALPHA_CUT)
    if len(xs) < 20:
        return (left + right) / 2.0
    return float(np.median(xs))


def prepare_frames(animation: str) -> list[Image.Image]:
    path = SOURCE / f"SS_remyCroche_{SOURCE_NAMES[animation]}.png"
    with Image.open(path) as opened:
        sheet = opened.convert("RGBA")
    if sheet.width % GRID or sheet.height % GRID:
        raise ValueError(f"{path.name} is not an exact {GRID}x{GRID} sheet")
    return [
        prepare_alpha(remove_tiny_components(crop_frame(sheet, frame_number)))
        for frame_number in FRAME_SELECTIONS[animation]
    ]


def standard_placements(images: list[Image.Image]) -> list[tuple[int, int]]:
    placements = []
    for image in images:
        box = bounds(image)
        if box is None:
            placements.append((0, 0))
            continue
        placements.append((round(128 - body_core_x(image)), 244 - box[3]))
    return placements


def foot_contact(
    image: Image.Image,
    minimum_x: int,
    maximum_x: int,
) -> tuple[float, float]:
    alpha = np.asarray(image.convert("RGBA"))[..., 3]
    y_grid, x_grid = np.indices(alpha.shape)
    foot = (
        (alpha > ALPHA_CUT)
        & (x_grid >= minimum_x)
        & (x_grid <= maximum_x)
        & (y_grid >= 140)
    )
    ys, _xs = np.nonzero(foot)
    if len(ys) == 0:
        raise RuntimeError("Could not find Remy's attack support foot")
    bottom = int(ys.max())
    band = foot[max(0, bottom - 10) : bottom + 1]
    _band_ys, band_xs = np.nonzero(band)
    return float(np.median(band_xs)), float(bottom)


def attack_placements(images: list[Image.Image]) -> list[tuple[int, int]]:
    placements = []
    for image in images:
        # The screen-right foot is the support foot throughout the attack. Its
        # toes sit higher in the opening pose, but tracking this same foot keeps
        # the kick from switching anchors as the opposite leg reaches downward.
        anchor_x, anchor_y = foot_contact(image, 110, 175)
        placements.append((round(150 - anchor_x), round(222 - anchor_y)))
    return placements


def damaged_placements(images: list[Image.Image]) -> list[tuple[int, int]]:
    # Remy's viewer-left foot remains at source x=65, y=231 through this motion.
    return [(96 - 65, 244 - 231) for _image in images]


def killed_placements(images: list[Image.Image]) -> list[tuple[int, int]]:
    downed_start = 5
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
        y = round(grounded_y + (final_y - grounded_y) * smooth)
        placements.append((x, y))
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
    path = OUTPUT / f"remyCroche-{animation}.png"
    Image.fromarray(array, "RGBA").save(path, format="PNG", optimize=False)
    print(path.name, Image.open(path).size, "frames=" + ",".join(map(str, frame_numbers)))
    return path


def build_all() -> None:
    for animation in ("walk", "attack", "damaged", "killed"):
        images = prepare_frames(animation)
        if animation == "attack":
            placements = attack_placements(images)
        elif animation == "damaged":
            placements = damaged_placements(images)
        elif animation == "killed":
            placements = killed_placements(images)
        else:
            placements = standard_placements(images)
        assemble(animation, images, placements)


def checkerboard() -> Image.Image:
    checker = Image.new("RGBA", (CELL, CELL), (42, 42, 42, 255))
    draw = ImageDraw.Draw(checker)
    for y in range(0, CELL, 16):
        for x in range(0, CELL, 16):
            if (x // 16 + y // 16) % 2:
                draw.rectangle((x, y, x + 15, y + 15), fill=(72, 64, 78, 255))
    return checker


def write_reviews() -> None:
    REVIEW.mkdir(parents=True, exist_ok=True)
    checker = checkerboard()
    for animation, frame_numbers in FRAME_SELECTIONS.items():
        path = OUTPUT / f"remyCroche-{animation}.png"
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
            contact.alpha_composite(tile, ((index % columns) * CELL, (index // columns) * CELL))
            gif_frames.append(tile.convert("P", palette=Image.Palette.ADAPTIVE))
        contact.convert("RGB").save(REVIEW / f"remyCroche-{animation}-contact.png")
        gif_frames[0].save(
            REVIEW / f"remyCroche-{animation}.gif",
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
