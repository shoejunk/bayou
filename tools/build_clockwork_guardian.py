from __future__ import annotations

from pathlib import Path

import numpy as np
from PIL import Image, ImageFilter


CELL = 256
GRID = 9
ALPHA_CUT = 10

ROOT = Path(r"C:\Users\jarox\OneDrive\Desktop\Tandem Tales\Bayou Bonanza")
SOURCE = Path(r"C:\Users\jarox\OneDrive\Desktop\TT Temp")
OUTPUT = ROOT / "assets" / "animations"


FRAME_SELECTIONS = {
    "walk": [2, 5, 9, 12, 16, 19, 22, 26, 29, 33, 36, 39, 43, 46, 49, 53, 56, 60, 63, 66, 70, 73, 77, 80],
    "attack": [7, 13, 18, 24, 30, 36, 41, 47, 53, 59, 64, 70],
    "damaged": [2, 10, 16, 23, 30, 37, 45, 52, 59, 66, 73, 80],
    "killed": [2, 4, 7, 9, 12, 14, 17, 19, 22, 24, 27, 29],
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
    array[totals == kernel * kernel, 3] = 255
    return Image.fromarray(zero_transparent_rgb(array), "RGBA")


def chassis_anchor(image: Image.Image) -> tuple[float, float]:
    alpha = np.asarray(image.convert("RGBA"))[..., 3]
    solid = Image.fromarray(np.where(alpha > 24, 255, 0).astype(np.uint8), "L")
    solid = solid.filter(ImageFilter.MinFilter(7)).filter(ImageFilter.MinFilter(5))
    ys, xs = np.nonzero(np.asarray(solid) > 0)
    if len(xs) >= 20:
        return float(np.median(xs)), float(np.median(ys))

    box = bounds(image)
    if box is None:
        return image.width / 2.0, image.height / 2.0
    left, top, right, bottom = box
    visible_y, visible_x = np.nonzero(alpha > 24)
    keep = (
        (visible_y <= top + 0.65 * (bottom - top))
        & (visible_x >= left + 0.2 * (right - left))
        & (visible_x <= left + 0.8 * (right - left))
    )
    if int(np.count_nonzero(keep)) >= 20:
        return float(np.median(visible_x[keep])), float(np.median(visible_y[keep]))
    return (left + right) / 2.0, (top + bottom) / 2.0


def prepare_frames(source_name: str, frames: list[int]) -> list[Image.Image]:
    source_path = SOURCE / f"SS_clockworkGuardian_{source_name}.png"
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

    output = OUTPUT / f"clockworkGuardian-{output_name}.png"
    Image.fromarray(array, "RGBA").save(output, format="PNG", optimize=False)
    print(output.name, (CELL * len(images), CELL), "frames=" + ",".join(map(str, frames)))
    return output


def chassis_placements(images: list[Image.Image], target: tuple[int, int]) -> list[tuple[int, int]]:
    target_x, target_y = target
    placements = []
    for image in images:
        anchor_x, anchor_y = chassis_anchor(image)
        placements.append((round(target_x - anchor_x), round(target_y - anchor_y)))
    return placements


def build_walk() -> Path:
    frames = FRAME_SELECTIONS["walk"]
    images = prepare_frames("walking", frames)
    return assemble("walk", frames, images, chassis_placements(images, (124, 104)))


def build_attack() -> Path:
    frames = FRAME_SELECTIONS["attack"]
    images = prepare_frames("attack", frames)
    placements = []
    for image in images:
        anchor_x, _anchor_y = chassis_anchor(image)
        box = bounds(image)
        assert box is not None
        placements.append((round(120 - anchor_x), 244 - box[3]))
    return assemble("attack", frames, images, placements)


def build_damaged() -> Path:
    frames = FRAME_SELECTIONS["damaged"]
    images = prepare_frames("damaged", frames)
    placements = []
    for image in images:
        anchor_x, _anchor_y = chassis_anchor(image)
        box = bounds(image)
        assert box is not None
        placements.append((round(126 - anchor_x), 244 - box[3]))
    return assemble("damaged", frames, images, placements)


def build_killed() -> Path:
    frames = FRAME_SELECTIONS["killed"]
    images = prepare_frames("killed", frames)
    chassis_y = [135, 142, 150, 156, 161, 165, 169, 170, 170, 170, 170, 170]
    placements = []
    for image, target_y in zip(images, chassis_y):
        anchor_x, anchor_y = chassis_anchor(image)
        placements.append((round(124 - anchor_x), round(target_y - anchor_y)))
    return assemble("killed", frames, images, placements)


def main() -> None:
    OUTPUT.mkdir(parents=True, exist_ok=True)
    build_walk()
    build_attack()
    build_damaged()
    build_killed()


if __name__ == "__main__":
    main()
