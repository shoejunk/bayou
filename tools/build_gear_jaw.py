from __future__ import annotations

from pathlib import Path

import numpy as np
from PIL import Image, ImageDraw, ImageFilter


CELL = 256
GRID = 9
ALPHA_CUT = 10

ROOT = Path(r"C:\Users\jarox\OneDrive\Desktop\Tandem Tales\Bayou Bonanza")
SOURCE = Path(r"C:\Users\jarox\OneDrive\Desktop\TT Temp")
OUTPUT = ROOT / "assets" / "animations"
REVIEW = ROOT / "tools" / "gearJaw-review"

FRAME_SELECTIONS = {
    "walk": [2, 5, 9, 12, 16, 19, 22, 26, 29, 33, 36, 39, 43, 46, 49, 53, 56, 60, 63, 66, 70, 73, 77, 80],
    "attack": [8, 10, 13, 15, 18, 20, 23, 25, 28, 30, 33, 35],
    "damaged": [6, 9, 12, 15, 18, 21, 24, 27, 29, 31, 33, 35],
    "killed": [5, 8, 11, 14, 17, 20, 24, 29, 35, 42, 46, 50],
}


def zero_transparent_rgb(array: np.ndarray) -> np.ndarray:
    output = array.copy()
    output[output[..., 3] <= ALPHA_CUT] = 0
    output[output[..., 3] == 0, :3] = 0
    return output


def normalize_source_alpha(
    image: Image.Image, transparent_at: int = 96, opaque_at: int = 192
) -> Image.Image:
    array = np.asarray(image.convert("RGBA")).copy()
    source_alpha = array[..., 3].astype(np.float32)
    remapped = np.clip(
        (source_alpha - transparent_at) * 255.0 / (opaque_at - transparent_at),
        0,
        255,
    ).astype(np.uint8)
    array[..., 3] = remapped
    array[remapped == 0, :3] = 0
    return Image.fromarray(zero_transparent_rgb(array), "RGBA")


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
    alpha_fraction = resized_alpha[..., None] / 255.0
    rgb = np.zeros_like(resized_rgb)
    visible = alpha_fraction[..., 0] > 0.001
    rgb[visible] = resized_rgb[visible] / alpha_fraction[visible]
    output = np.dstack([np.clip(rgb, 0, 255), resized_alpha])
    return Image.fromarray(
        zero_transparent_rgb(np.clip(output, 0, 255).astype(np.uint8)), "RGBA"
    )


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


def bounds(image: Image.Image) -> tuple[int, int, int, int] | None:
    alpha = np.asarray(image.convert("RGBA"))[..., 3]
    ys, xs = np.nonzero(alpha > ALPHA_CUT)
    if len(xs) == 0:
        return None
    return int(xs.min()), int(ys.min()), int(xs.max()), int(ys.max())


def components(mask: np.ndarray) -> list[list[tuple[int, int]]]:
    height, width = mask.shape
    seen = np.zeros_like(mask, dtype=bool)
    found: list[list[tuple[int, int]]] = []
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
                for next_x, next_y in (
                    (x + 1, y),
                    (x - 1, y),
                    (x, y + 1),
                    (x, y - 1),
                ):
                    if (
                        0 <= next_x < width
                        and 0 <= next_y < height
                        and mask[next_y, next_x]
                        and not seen[next_y, next_x]
                    ):
                        seen[next_y, next_x] = True
                        stack.append((next_x, next_y))
            found.append(points)
    return found


def remove_tiny_components(image: Image.Image, minimum_pixels: int = 10) -> Image.Image:
    array = zero_transparent_rgb(np.asarray(image.convert("RGBA")))
    for points in components(array[..., 3] > ALPHA_CUT):
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


def trim_contaminated_contour(image: Image.Image) -> Image.Image:
    array = np.asarray(image.convert("RGBA")).copy()
    core = Image.fromarray(
        np.where(array[..., 3] >= 128, 255, 0).astype(np.uint8), "L"
    )
    core = core.filter(ImageFilter.MinFilter(3))
    alpha = np.asarray(core)
    array[..., 3] = alpha
    array[alpha == 0, :3] = 0
    return Image.fromarray(zero_transparent_rgb(array), "RGBA")


def body_anchor(image: Image.Image) -> tuple[float, float]:
    alpha = np.asarray(image.convert("RGBA"))[..., 3]
    solid = Image.fromarray(np.where(alpha > 32, 255, 0).astype(np.uint8), "L")
    solid = solid.filter(ImageFilter.MinFilter(21))
    found = components(np.asarray(solid) > 0)
    if found:
        largest = max(found, key=len)
        xs = [point[0] for point in largest]
        ys = [point[1] for point in largest]
        return (min(xs) + max(xs)) / 2.0, (min(ys) + max(ys)) / 2.0
    box = bounds(image)
    if box is None:
        return image.width / 2.0, image.height / 2.0
    left, top, right, bottom = box
    return (left + right) / 2.0, top + 0.42 * (bottom - top)


def load_frames() -> dict[str, list[Image.Image]]:
    output: dict[str, list[Image.Image]] = {}
    for animation, frames in FRAME_SELECTIONS.items():
        source_name = "walking" if animation == "walk" else animation
        with Image.open(SOURCE / f"SS_gearJaw_{source_name}.png") as opened:
            sheet = opened.convert("RGBA")
        output[animation] = [
            harden_interior_alpha(
                remove_tiny_components(normalize_source_alpha(crop_frame(sheet, frame)))
            )
            for frame in frames
        ]
    return output


def shared_scale(raw: dict[str, list[Image.Image]]) -> float:
    boxes = [
        box
        for images in raw.values()
        for image in images
        if (box := bounds(image)) is not None
    ]
    maximum_width = max(box[2] - box[0] + 1 for box in boxes)
    maximum_height = max(box[3] - box[1] + 1 for box in boxes)
    return min(246 / maximum_width, 246 / maximum_height, 2.0)


def safe_target_x(images: list[Image.Image], requested: int) -> int:
    minimum = -10_000.0
    maximum = 10_000.0
    for image in images:
        box = bounds(image)
        assert box is not None
        anchor_x, _anchor_y = body_anchor(image)
        minimum = max(minimum, anchor_x + 2 - box[0])
        maximum = min(maximum, anchor_x + 253 - box[2])
    return round(min(max(float(requested), minimum), maximum))


def fixed_body_placements(
    images: list[Image.Image], requested_x: int, target_y: int
) -> list[tuple[int, int]]:
    target_x = safe_target_x(images, requested_x)
    placements = []
    for image in images:
        anchor_x, anchor_y = body_anchor(image)
        placements.append((round(target_x - anchor_x), round(target_y - anchor_y)))
    return placements


def grounded_placements(
    images: list[Image.Image], requested_x: int, ground_y: int
) -> list[tuple[int, int]]:
    target_x = safe_target_x(images, requested_x)
    placements = []
    for image in images:
        anchor_x, _anchor_y = body_anchor(image)
        box = bounds(image)
        assert box is not None
        placements.append((round(target_x - anchor_x), ground_y - box[3]))
    return placements


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

    output = OUTPUT / f"gearJaw-{animation}.png"
    Image.fromarray(array, "RGBA").save(output, format="PNG", optimize=False)
    print(
        output.name,
        (CELL * len(images), CELL),
        "frames=" + ",".join(str(frame) for frame in FRAME_SELECTIONS[animation]),
    )
    return output


def write_review_previews() -> None:
    REVIEW.mkdir(parents=True, exist_ok=True)
    checker = Image.new("RGBA", (CELL, CELL), (42, 42, 42, 255))
    draw = ImageDraw.Draw(checker)
    for y in range(0, CELL, 16):
        for x in range(0, CELL, 16):
            if (x // 16 + y // 16) % 2:
                draw.rectangle((x, y, x + 15, y + 15), fill=(72, 64, 78, 255))

    for animation, frame_numbers in FRAME_SELECTIONS.items():
        with Image.open(OUTPUT / f"gearJaw-{animation}.png") as opened:
            sheet = opened.convert("RGBA")
        frames = [
            sheet.crop((index * CELL, 0, (index + 1) * CELL, CELL))
            for index in range(len(frame_numbers))
        ]
        columns = 6
        rows = (len(frames) + columns - 1) // columns
        contact = Image.new("RGBA", (columns * CELL, rows * CELL), (28, 28, 28, 255))
        for index, (frame, source_number) in enumerate(zip(frames, frame_numbers)):
            tile = checker.copy()
            tile.alpha_composite(frame)
            ImageDraw.Draw(tile).text(
                (5, 5), f"{index + 1}: src {source_number}", fill="white"
            )
            contact.alpha_composite(
                tile, ((index % columns) * CELL, (index // columns) * CELL)
            )
        contact.convert("RGB").save(REVIEW / f"gearJaw-{animation}-contact.png")

        gif_frames = []
        for frame in frames:
            tile = checker.copy()
            tile.alpha_composite(frame)
            gif_frames.append(tile.convert("P", palette=Image.Palette.ADAPTIVE))
        gif_frames[0].save(
            REVIEW / f"gearJaw-{animation}.gif",
            save_all=True,
            append_images=gif_frames[1:],
            duration=85,
            loop=0,
            disposal=2,
        )


def main() -> None:
    OUTPUT.mkdir(parents=True, exist_ok=True)
    raw = load_frames()
    scale = shared_scale(raw)
    print(f"shared scale={scale:.6f}")
    resized = {
        animation: [
            trim_contaminated_contour(
                premultiplied_resize(
                    image, (round(image.width * scale), round(image.height * scale))
                )
            )
            for image in images
        ]
        for animation, images in raw.items()
    }

    assemble("walk", resized["walk"], fixed_body_placements(resized["walk"], 128, 130))
    assemble("attack", resized["attack"], grounded_placements(resized["attack"], 128, 238))
    assemble("damaged", resized["damaged"], grounded_placements(resized["damaged"], 128, 238))
    assemble("killed", resized["killed"], grounded_placements(resized["killed"], 128, 238))


if __name__ == "__main__":
    main()
