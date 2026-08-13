from __future__ import annotations

from pathlib import Path

import numpy as np
from PIL import Image, ImageFilter


CELL = 256
ALPHA_CUT = 10

ROOT = Path(r"C:\Users\jarox\OneDrive\Desktop\Tandem Tales\Bayou Bonanza")
SOURCE = Path(r"C:\Users\jarox\OneDrive\Desktop\TT Temp")
OUTPUT = ROOT / "assets" / "animations"

SPECS = {
    "walk": {
        "source": "walking",
        "columns": 9,
        "rows": 9,
        "frames": [2, 5, 9, 12, 16, 19, 22, 26, 29, 33, 36, 39, 43, 46, 49, 53, 56, 60, 63, 66, 70, 73, 77, 80],
    },
    "attack": {
        "source": "attack",
        "columns": 9,
        "rows": 9,
        "frames": [2, 9, 16, 23, 30, 37, 45, 52, 59, 66, 73, 80],
    },
    "damaged": {
        "source": "damaged",
        "columns": 8,
        "rows": 8,
        "frames": [2, 5, 8, 11, 14, 16, 14, 11, 8, 5, 3, 2],
    },
    "killed": {
        "source": "killed",
        "columns": 9,
        "rows": 9,
        "frames": [2, 5, 9, 12, 16, 19, 23, 26, 30, 33, 37, 40],
    },
}


def zero_transparent_rgb(array: np.ndarray) -> np.ndarray:
    output = array.copy()
    output[output[..., 3] <= ALPHA_CUT] = 0
    output[output[..., 3] == 0, :3] = 0
    return output


def premultiplied_resize(image: Image.Image, size: tuple[int, int]) -> Image.Image:
    array = np.asarray(image.convert("RGBA"), dtype=np.float32)
    alpha = array[..., 3:4] / 255.0
    premultiplied = array[..., :3] * alpha
    premultiplied_image = Image.fromarray(
        np.clip(premultiplied, 0, 255).astype(np.uint8), "RGB"
    )
    alpha_image = Image.fromarray(array[..., 3].astype(np.uint8), "L")
    resized_rgb = np.asarray(
        premultiplied_image.resize(size, Image.Resampling.LANCZOS), dtype=np.float32
    )
    resized_alpha = np.asarray(
        alpha_image.resize(size, Image.Resampling.LANCZOS), dtype=np.float32
    )
    resized_alpha_fraction = resized_alpha[..., None] / 255.0
    rgb = np.zeros_like(resized_rgb)
    visible = resized_alpha_fraction[..., 0] > 0.001
    rgb[visible] = resized_rgb[visible] / resized_alpha_fraction[visible]
    output = np.dstack([np.clip(rgb, 0, 255), resized_alpha])
    return Image.fromarray(
        zero_transparent_rgb(np.clip(output, 0, 255).astype(np.uint8)), "RGBA"
    )


def crop_frame(
    sheet: Image.Image, frame_number: int, columns: int, rows: int
) -> Image.Image:
    x_edges = [round(index * sheet.width / columns) for index in range(columns + 1)]
    y_edges = [round(index * sheet.height / rows) for index in range(rows + 1)]
    index = frame_number - 1
    column = index % columns
    row = index // columns
    return sheet.crop(
        (x_edges[column], y_edges[row], x_edges[column + 1], y_edges[row + 1])
    ).convert("RGBA")


def crop_expanded_damaged_frame(sheet: Image.Image, frame_number: int) -> Image.Image:
    columns = 8
    rows = 8
    x_edges = [round(index * sheet.width / columns) for index in range(columns + 1)]
    y_edges = [round(index * sheet.height / rows) for index in range(rows + 1)]
    index = frame_number - 1
    column = index % columns
    row = index // columns
    left = max(0, x_edges[column] - 80)
    top = max(0, y_edges[row] - 100)
    right = min(sheet.width, x_edges[column + 1] + 80)
    bottom = min(sheet.height, y_edges[row + 1] + 180)
    return sheet.crop((left, top, right, bottom)).convert("RGBA")


def bounds(image: Image.Image) -> tuple[int, int, int, int] | None:
    alpha = np.asarray(image.convert("RGBA"))[..., 3]
    ys, xs = np.nonzero(alpha > ALPHA_CUT)
    if len(xs) == 0:
        return None
    return int(xs.min()), int(ys.min()), int(xs.max()), int(ys.max())


def remove_tiny_components(image: Image.Image, minimum_pixels: int = 24) -> Image.Image:
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


def largest_component(mask: np.ndarray) -> tuple[np.ndarray, np.ndarray]:
    height, width = mask.shape
    seen = np.zeros_like(mask, dtype=bool)
    largest: list[tuple[int, int]] = []
    for start_y in range(height):
        for start_x in range(width):
            if seen[start_y, start_x] or not mask[start_y, start_x]:
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
                        and mask[next_y, next_x]
                        and not seen[next_y, next_x]
                    ):
                        seen[next_y, next_x] = True
                        stack.append((next_x, next_y))
            if len(points) > len(largest):
                largest = points
    if not largest:
        return np.array([], dtype=int), np.array([], dtype=int)
    xs = np.array([point[0] for point in largest], dtype=int)
    ys = np.array([point[1] for point in largest], dtype=int)
    return xs, ys


def keep_largest_component(image: Image.Image) -> Image.Image:
    array = zero_transparent_rgb(np.asarray(image.convert("RGBA")))
    xs, ys = largest_component(array[..., 3] > ALPHA_CUT)
    if len(xs) == 0:
        return Image.fromarray(array, "RGBA")
    keep = np.zeros(array.shape[:2], dtype=bool)
    keep[ys, xs] = True
    array[~keep] = 0
    return Image.fromarray(zero_transparent_rgb(array), "RGBA")


def chassis_anchor(image: Image.Image) -> tuple[float, float]:
    alpha = np.asarray(image.convert("RGBA"))[..., 3]
    solid = Image.fromarray(np.where(alpha > 32, 255, 0).astype(np.uint8), "L")
    solid = solid.filter(ImageFilter.MinFilter(15))
    xs, ys = largest_component(np.asarray(solid) > 0)
    if len(xs) >= 20:
        return float(np.median(xs)), float(np.median(ys))
    box = bounds(image)
    if box is None:
        return image.width / 2.0, image.height / 2.0
    left, top, right, bottom = box
    return (left + right) / 2.0, top + 0.45 * (bottom - top)


def load_raw_frames() -> dict[str, list[Image.Image]]:
    output: dict[str, list[Image.Image]] = {}
    for animation, spec in SPECS.items():
        source_path = SOURCE / f"SS_fizzlewickGearwright_mounted_{spec['source']}.png"
        with Image.open(source_path) as opened:
            sheet = opened.convert("RGBA")
        frames = []
        for frame in spec["frames"]:
            if animation == "damaged":
                image = crop_expanded_damaged_frame(sheet, frame)
            else:
                image = crop_frame(
                    sheet,
                    frame,
                    int(spec["columns"]),
                    int(spec["rows"]),
                )
            image = remove_tiny_components(image)
            if animation == "damaged":
                image = keep_largest_component(image)
            frames.append(harden_interior_alpha(image))
        output[animation] = frames
    return output


def shared_scale(raw_frames: dict[str, list[Image.Image]]) -> float:
    boxes = [
        box
        for images in raw_frames.values()
        for image in images
        if (box := bounds(image)) is not None
    ]
    maximum_width = max(box[2] - box[0] + 1 for box in boxes)
    maximum_height = max(box[3] - box[1] + 1 for box in boxes)
    return min(246 / maximum_width, 246 / maximum_height, 1.0)


def target_x_for(images: list[Image.Image], requested: int) -> int:
    minimum = -10_000.0
    maximum = 10_000.0
    for image in images:
        box = bounds(image)
        assert box is not None
        anchor_x, _anchor_y = chassis_anchor(image)
        minimum = max(minimum, anchor_x + 2 - box[0])
        maximum = min(maximum, anchor_x + 253 - box[2])
    return round(min(max(float(requested), minimum), maximum))


def assemble(
    animation: str,
    images: list[Image.Image],
    placements: list[tuple[int, int]],
) -> Path:
    sheet = Image.new("RGBA", (CELL * len(images), CELL), (0, 0, 0, 0))
    for index, (image, (local_x, local_y)) in enumerate(zip(images, placements)):
        sheet.alpha_composite(image, (index * CELL + local_x, local_y))

    array = zero_transparent_rgb(np.asarray(sheet))
    for index in range(len(images)):
        left = index * CELL
        array[:, left] = 0
        array[:, left + CELL - 1] = 0
        array[0, left : left + CELL] = 0
        array[CELL - 1, left : left + CELL] = 0

    output = OUTPUT / f"fizzlewickGearwright_mounted-{animation}.png"
    Image.fromarray(array, "RGBA").save(output, format="PNG", optimize=False)
    print(
        output.name,
        (CELL * len(images), CELL),
        "frames=" + ",".join(str(frame) for frame in SPECS[animation]["frames"]),
    )
    return output


def fixed_chassis_placements(
    images: list[Image.Image], requested_x: int, target_y: int
) -> list[tuple[int, int]]:
    target_x = target_x_for(images, requested_x)
    placements = []
    for image in images:
        anchor_x, anchor_y = chassis_anchor(image)
        placements.append((round(target_x - anchor_x), round(target_y - anchor_y)))
    return placements


def lowest_foot_placements(
    images: list[Image.Image], requested_x: int, ground_y: int
) -> list[tuple[int, int]]:
    target_x = target_x_for(images, requested_x)
    placements = []
    for image in images:
        anchor_x, _anchor_y = chassis_anchor(image)
        box = bounds(image)
        assert box is not None
        placements.append((round(target_x - anchor_x), ground_y - box[3]))
    return placements


def killed_placements(images: list[Image.Image]) -> list[tuple[int, int]]:
    target_x = target_x_for(images, 132)
    target_y = [150, 154, 162, 174, 186, 192, 192, 192, 192, 192, 192, 192]
    placements = []
    for image, y in zip(images, target_y):
        anchor_x, anchor_y = chassis_anchor(image)
        placements.append((round(target_x - anchor_x), round(y - anchor_y)))
    return placements


def main() -> None:
    OUTPUT.mkdir(parents=True, exist_ok=True)
    raw = load_raw_frames()
    scale = shared_scale(raw)
    print(f"shared scale={scale:.6f}")
    resized = {
        animation: [
            premultiplied_resize(
                image, (round(image.width * scale), round(image.height * scale))
            )
            for image in images
        ]
        for animation, images in raw.items()
    }

    assemble("walk", resized["walk"], fixed_chassis_placements(resized["walk"], 128, 150))
    assemble("attack", resized["attack"], fixed_chassis_placements(resized["attack"], 112, 150))
    assemble("damaged", resized["damaged"], lowest_foot_placements(resized["damaged"], 128, 244))
    assemble("killed", resized["killed"], killed_placements(resized["killed"]))


if __name__ == "__main__":
    main()
