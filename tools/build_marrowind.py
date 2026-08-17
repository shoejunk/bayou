from __future__ import annotations

import argparse
import math
from pathlib import Path

import numpy as np
from PIL import Image, ImageDraw, ImageFilter


CELL = 256
GRID = 9
ALPHA_CUT = 4

ROOT = Path(r"C:\Users\jarox\OneDrive\Desktop\Tandem Tales\Bayou Bonanza")
SOURCE = Path(r"C:\Users\jarox\OneDrive\Desktop\TT Temp")
OUTPUT = ROOT / "assets" / "animations"
REVIEW = ROOT / "tools" / "marrowind-review"

AIM_FRAMES = [2, 5, 8, 11, 14, 17, 20, 23, 26, 29, 43, 54]
FRAME_SELECTIONS = {
    "walk": [
        11, 13, 15, 16, 18, 20, 22, 23, 25, 27, 29, 30,
        32, 34, 36, 37, 39, 41, 43, 45, 47, 48, 50, 52,
    ],
    "aim": AIM_FRAMES,
    # Show a short real draw, advance through the held pose, then release.
    "attack": [28, 30, 36, 43, 50, 54, 57, 58, 59, 60, 61, 61],
    "lower-weapon": list(reversed(AIM_FRAMES)),
    "damaged": [2, 9, 16, 23, 30, 37, 45, 52, 59, 66, 73, 80],
    # The get-up begins after this stable prone range.
    "killed": [2, 6, 10, 14, 18, 22, 25, 29, 33, 37, 41, 45],
}

SOURCE_NAMES = {
    "walk": "walking",
    "aim": "attack",
    "attack": "attack",
    "lower-weapon": "attack",
    "damaged": "damaged",
    "killed": "killed",
}

SCALES = {
    "walk": 0.91,
    "aim": 0.83,
    "attack": 0.82,
    "lower-weapon": 0.83,
    "damaged": 0.92,
    "killed": 0.89,
}

TARGET_X = {
    "walk": 128,
    "aim": 128,
    "attack": 128,
    "lower-weapon": 128,
    "damaged": 128,
    "killed": 121,
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
    array[0] = 0
    array[-1] = 0
    array[:, 0] = 0
    array[:, -1] = 0
    return Image.fromarray(zero_transparent_rgb(array), "RGBA")


def premultiplied_resize(image: Image.Image, size: tuple[int, int]) -> Image.Image:
    array = np.asarray(image.convert("RGBA"), dtype=np.float32)
    alpha = array[..., 3:4] / 255.0
    premultiplied = array[..., :3] * alpha
    rgb_image = Image.fromarray(np.clip(premultiplied, 0, 255).astype(np.uint8), "RGB")
    alpha_image = Image.fromarray(array[..., 3].astype(np.uint8), "L")
    resized_rgb = np.asarray(
        rgb_image.resize(size, Image.Resampling.LANCZOS), dtype=np.float32
    )
    resized_alpha = np.asarray(
        alpha_image.resize(size, Image.Resampling.LANCZOS), dtype=np.float32
    )
    alpha_fraction = resized_alpha[..., None] / 255.0
    rgb = np.zeros_like(resized_rgb)
    nonzero = alpha_fraction[..., 0] > 0.001
    rgb[nonzero] = resized_rgb[nonzero] / alpha_fraction[nonzero]
    output = np.dstack([np.clip(rgb, 0, 255), resized_alpha])
    return Image.fromarray(
        zero_transparent_rgb(np.clip(output, 0, 255).astype(np.uint8)), "RGBA"
    )


def remove_tiny_components(image: Image.Image, minimum_pixels: int) -> Image.Image:
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


def repair_extreme_white_fringe(image: Image.Image, frame_count: int) -> Image.Image:
    """Repair only unmistakable white contamination while preserving soft fur edges."""
    array = zero_transparent_rgb(np.asarray(image.convert("RGBA")))
    output = array.copy()
    for frame_index in range(frame_count):
        left = frame_index * CELL
        frame = array[:, left : left + CELL]
        alpha = frame[..., 3]
        for y in range(2, CELL - 2):
            for x in range(2, CELL - 2):
                pixel = frame[y, x]
                if pixel[3] == 0 or pixel[3] > 48:
                    continue
                if not np.any(alpha[y - 1 : y + 2, x - 1 : x + 2] == 0):
                    continue
                rgb = pixel[:3].astype(np.int16)
                if int(rgb.min()) < 245 or int(rgb.max() - rgb.min()) > 12:
                    continue
                neighborhood = frame[y - 2 : y + 3, x - 2 : x + 3]
                opaque = neighborhood[neighborhood[..., 3] >= 192]
                if len(opaque) < 2:
                    continue
                replacement = np.median(opaque[:, :3], axis=0)
                if float(np.mean(rgb)) <= float(np.mean(replacement)) + 70:
                    continue
                output[y, left + x, :3] = np.clip(replacement, 0, 255).astype(np.uint8)
    return Image.fromarray(zero_transparent_rgb(output), "RGBA")


def bounds(image: Image.Image) -> tuple[int, int, int, int] | None:
    alpha = np.asarray(image.convert("RGBA"))[..., 3]
    ys, xs = np.nonzero(alpha > ALPHA_CUT)
    if len(xs) == 0:
        return None
    return int(xs.min()), int(ys.min()), int(xs.max()), int(ys.max())


def body_core(image: Image.Image) -> tuple[float, float]:
    alpha = np.asarray(image.convert("RGBA"))[..., 3]
    core = Image.fromarray(np.where(alpha > 20, 255, 0).astype(np.uint8), "L")
    core = np.asarray(core.filter(ImageFilter.MinFilter(15))) > 0
    ys, xs = np.nonzero(core)
    if len(xs) < 20:
        box = bounds(image)
        if box is None:
            return image.width / 2.0, image.height / 2.0
        return (box[0] + box[2]) / 2.0, (box[1] + box[3]) / 2.0
    low, high = np.percentile(ys, [18, 72])
    use = (ys >= low) & (ys <= high)
    return float(np.median(xs[use])), float(np.median(ys[use]))


def prepare_frames(animation: str) -> list[Image.Image]:
    source_path = SOURCE / f"SS_marrowind_{SOURCE_NAMES[animation]}.png"
    with Image.open(source_path) as opened:
        sheet = opened.convert("RGBA")
    if sheet.width % GRID or sheet.height % GRID:
        raise ValueError(f"{source_path.name} is not an exact {GRID}x{GRID} sheet")
    scale = SCALES[animation]
    size = (
        round((sheet.width // GRID) * scale),
        round((sheet.height // GRID) * scale),
    )
    minimum_component = 4 if animation in ("aim", "attack", "lower-weapon") else 6
    return [
        harden_interior_alpha(
            remove_tiny_components(
                premultiplied_resize(crop_frame(sheet, number), size),
                minimum_component,
            )
        )
        for number in FRAME_SELECTIONS[animation]
    ]


def grounded_placements(
    images: list[Image.Image], target_x: int
) -> list[tuple[int, int]]:
    placements: list[tuple[int, int]] = []
    for image in images:
        box = bounds(image)
        if box is None:
            placements.append((0, 0))
            continue
        core_x, _core_y = body_core(image)
        placements.append((round(target_x - core_x), 244 - box[3]))
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
    result = repair_extreme_white_fringe(Image.fromarray(array, "RGBA"), len(images))
    OUTPUT.mkdir(parents=True, exist_ok=True)
    path = OUTPUT / f"marrowind-{animation}.png"
    result.save(path, format="PNG", optimize=False)
    print(path.name, (CELL * len(images), CELL), "frames=" + ",".join(map(str, frame_numbers)))
    return path


def build(animation: str) -> None:
    images = prepare_frames(animation)
    assemble(animation, images, grounded_placements(images, TARGET_X[animation]))


def build_all() -> None:
    for animation in ("walk", "aim", "attack", "lower-weapon", "damaged", "killed"):
        build(animation)


def checkerboard() -> Image.Image:
    image = Image.new("RGBA", (CELL, CELL), (42, 42, 42, 255))
    draw = ImageDraw.Draw(image)
    for y in range(0, CELL, 16):
        for x in range(0, CELL, 16):
            if (x // 16 + y // 16) % 2:
                draw.rectangle((x, y, x + 15, y + 15), fill=(72, 64, 78, 255))
    draw.line((0, 244, CELL - 1, 244), fill=(82, 190, 140, 255), width=1)
    return image


def write_reviews() -> None:
    REVIEW.mkdir(parents=True, exist_ok=True)
    checker = checkerboard()
    for animation, numbers in FRAME_SELECTIONS.items():
        path = OUTPUT / f"marrowind-{animation}.png"
        if not path.exists():
            continue
        with Image.open(path) as opened:
            sheet = opened.convert("RGBA")
        frames = [
            sheet.crop((index * CELL, 0, (index + 1) * CELL, CELL))
            for index in range(len(numbers))
        ]
        columns = 6
        rows = math.ceil(len(frames) / columns)
        contact = Image.new("RGBA", (columns * CELL, rows * CELL), (28, 28, 28, 255))
        gif_frames = []
        for index, (frame, source_number) in enumerate(zip(frames, numbers)):
            tile = checker.copy()
            tile.alpha_composite(frame)
            ImageDraw.Draw(tile).text(
                (5, 5), f"{index + 1}: src {source_number}", fill="white"
            )
            contact.alpha_composite(
                tile, ((index % columns) * CELL, (index // columns) * CELL)
            )
            gif_frames.append(tile.convert("P", palette=Image.Palette.ADAPTIVE))
        contact.convert("RGB").save(REVIEW / f"marrowind-{animation}-contact.png")
        gif_frames[0].save(
            REVIEW / f"marrowind-{animation}.gif",
            save_all=True,
            append_images=gif_frames[1:],
            duration=85,
            loop=0,
            disposal=2,
        )


def main() -> None:
    parser = argparse.ArgumentParser()
    parser.add_argument("--review-only", action="store_true")
    parser.add_argument("--animation", choices=tuple(FRAME_SELECTIONS))
    args = parser.parse_args()
    if not args.review_only:
        if args.animation:
            build(args.animation)
        else:
            build_all()
    write_reviews()


if __name__ == "__main__":
    main()
