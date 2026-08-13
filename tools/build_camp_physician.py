from __future__ import annotations

from pathlib import Path

import numpy as np
from PIL import Image


CELL = 256
GRID = 9
ALPHA_CUT = 10

ROOT = Path(r"C:\Users\jarox\OneDrive\Desktop\Tandem Tales\Bayou Bonanza")
SOURCE = Path(r"C:\Users\jarox\OneDrive\Desktop\TT Temp")
OUTPUT = ROOT / "assets" / "animations"


FRAME_SELECTIONS = {
    "walk": [7, 10, 13, 16, 19, 22, 25, 28, 31, 34, 37, 40, 42, 45, 48, 51, 54, 57, 60, 63, 66, 69, 72, 75],
    "attack": [2, 4, 5, 7, 9, 10, 12, 13, 15, 17, 18, 20],
    "damaged": [3, 10, 17, 23, 30, 37, 44, 51, 58, 64, 71, 78],
    "killed": [2, 5, 8, 11, 14, 17, 20, 24, 28, 34, 41, 48],
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
    return sheet.crop((left, top, left + frame_width, top + frame_height)).convert("RGBA")


def bounds(image: Image.Image) -> tuple[int, int, int, int] | None:
    alpha = np.asarray(image.convert("RGBA"))[..., 3]
    ys, xs = np.nonzero(alpha > ALPHA_CUT)
    if len(xs) == 0:
        return None
    return int(xs.min()), int(ys.min()), int(xs.max()), int(ys.max())


def remove_tiny_components(image: Image.Image, minimum_pixels: int = 18) -> Image.Image:
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
                for next_x, next_y in ((x + 1, y), (x - 1, y), (x, y + 1), (x, y - 1)):
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
    visible = array[..., 3] > 24
    padded = np.pad(visible.astype(np.uint8), 2, mode="constant")
    integral = np.pad(padded, ((1, 0), (1, 0)), mode="constant").cumsum(0).cumsum(1)
    kernel = 5
    totals = (
        integral[kernel:, kernel:]
        - integral[:-kernel, kernel:]
        - integral[kernel:, :-kernel]
        + integral[:-kernel, :-kernel]
    )
    interior = totals == kernel * kernel
    array[interior, 3] = 255
    return Image.fromarray(zero_transparent_rgb(array), "RGBA")


def body_core_x(image: Image.Image) -> float:
    box = bounds(image)
    if box is None:
        return image.width / 2.0
    left, top, right, bottom = box
    alpha = np.asarray(image.convert("RGBA"))[..., 3]
    height = max(1, bottom - top + 1)
    y0 = top + round(height * 0.18)
    y1 = top + round(height * 0.64)
    region = alpha[y0 : y1 + 1] > ALPHA_CUT
    _ys, xs = np.nonzero(region)
    if len(xs) < 20:
        return (left + right) / 2.0
    return float(np.median(xs))


def lower_contact(image: Image.Image) -> tuple[float, float]:
    alpha = np.asarray(image.convert("RGBA"))[..., 3]
    ys, xs = np.nonzero(alpha > ALPHA_CUT)
    if len(xs) == 0:
        return image.width / 2.0, image.height - 1.0
    bottom = int(ys.max())
    band = alpha[max(0, bottom - 9) : bottom + 1] > ALPHA_CUT
    _band_y, band_x = np.nonzero(band)
    if len(band_x) < 5:
        return float(np.median(xs)), float(bottom)
    return float(np.median(band_x)), float(bottom)


def prepare_frames(source_name: str, frames: list[int]) -> list[Image.Image]:
    source_path = SOURCE / f"SS_campPhysician_{source_name}.png"
    with Image.open(source_path) as opened:
        sheet = opened.convert("RGBA")
    return [
        harden_interior_alpha(remove_tiny_components(crop_frame(sheet, frame)))
        for frame in frames
    ]


def safe_placement(image: Image.Image, frame_index: int, dx: int, dy: int) -> tuple[int, int]:
    box = bounds(image)
    if box is None:
        return dx, dy
    left, top, right, bottom = box
    cell_left = frame_index * CELL
    if dx + left < cell_left + 2:
        dx += cell_left + 2 - (dx + left)
    if dx + right > cell_left + CELL - 3:
        dx -= dx + right - (cell_left + CELL - 3)
    if dy + top < 2:
        dy += 2 - (dy + top)
    if dy + bottom > CELL - 3:
        dy -= dy + bottom - (CELL - 3)
    return dx, dy


def assemble(
    output_name: str,
    frames: list[int],
    images: list[Image.Image],
    placements: list[tuple[int, int]],
) -> Path:
    sheet = Image.new("RGBA", (CELL * len(images), CELL), (0, 0, 0, 0))
    for index, (image, (local_x, local_y)) in enumerate(zip(images, placements)):
        dx, dy = safe_placement(image, index, index * CELL + local_x, local_y)
        sheet.alpha_composite(image, (dx, dy))

    array = zero_transparent_rgb(np.asarray(sheet))
    for index in range(len(images)):
        left = index * CELL
        array[:, left] = 0
        array[:, left + CELL - 1] = 0
        array[0, left : left + CELL] = 0
        array[CELL - 1, left : left + CELL] = 0

    output = OUTPUT / f"campPhysician-{output_name}.png"
    Image.fromarray(array, "RGBA").save(output, format="PNG", optimize=False)
    print(output.name, (CELL * len(images), CELL), "frames=" + ",".join(map(str, frames)))
    return output


def build_walk() -> Path:
    frames = FRAME_SELECTIONS["walk"]
    images = prepare_frames("walking", frames)
    placements = []
    for image in images:
        box = bounds(image)
        assert box is not None
        placements.append((round(128 - body_core_x(image)), 244 - box[3]))
    return assemble("walk", frames, images, placements)


def build_attack() -> Path:
    frames = FRAME_SELECTIONS["attack"]
    images = prepare_frames("attack", frames)
    placements = []
    for image in images:
        box = bounds(image)
        assert box is not None
        placements.append((round(128 - body_core_x(image)), 244 - box[3]))
    return assemble("attack", frames, images, placements)


def build_damaged() -> Path:
    frames = FRAME_SELECTIONS["damaged"]
    images = prepare_frames("damaged", frames)
    placements = []
    for image in images:
        anchor_x, anchor_y = lower_contact(image)
        placements.append((round(104 - anchor_x), round(244 - anchor_y)))
    return assemble("damaged", frames, images, placements)


def build_killed() -> Path:
    frames = FRAME_SELECTIONS["killed"]
    images = prepare_frames("killed", frames)
    downed_index = next(index for index, frame in enumerate(frames) if frame >= 20)

    start_x = round(128 - body_core_x(images[0]))
    downed_centers = []
    for image in images[downed_index:]:
        box = bounds(image)
        assert box is not None
        downed_centers.append((box[0] + box[2]) / 2.0)
    final_x = round(128 - float(np.median(downed_centers)))

    placements = []
    for index, image in enumerate(images):
        box = bounds(image)
        assert box is not None
        if index < downed_index:
            progress = index / float(downed_index)
            smooth = progress * progress * (3.0 - 2.0 * progress)
            x = round(start_x + (final_x - start_x) * smooth)
        else:
            x = final_x
        placements.append((x, 244 - box[3]))
    return assemble("killed", frames, images, placements)


def main() -> None:
    OUTPUT.mkdir(parents=True, exist_ok=True)
    build_walk()
    build_attack()
    build_damaged()
    build_killed()


if __name__ == "__main__":
    main()
