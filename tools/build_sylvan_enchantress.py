from __future__ import annotations

import argparse
import math
from pathlib import Path

import numpy as np
from PIL import Image, ImageDraw, ImageFilter


CELL = 256
GRID = 9
ALPHA_CUT = 10
SCALE = 0.84

ROOT = Path(r"C:\Users\jarox\OneDrive\Desktop\Tandem Tales\Bayou Bonanza")
SOURCE = Path(r"C:\Users\jarox\OneDrive\Desktop\TT Temp")
OUTPUT = ROOT / "assets" / "animations"
REVIEW = ROOT / "tools" / "sylvanEnchantress-review"

FRAME_SELECTIONS = {
    "walk": [
        2, 5, 9, 12, 16, 19, 22, 26, 29, 33, 36, 39,
        43, 46, 49, 53, 56, 60, 63, 66, 70, 73, 77, 80,
    ],
    # Source frame 37 contains a detached hand with the connecting arm missing.
    "attack": [2, 9, 16, 23, 30, 41, 45, 52, 59, 66, 73, 80],
    "damaged": [2, 9, 16, 23, 30, 37, 45, 52, 59, 66, 73, 80],
    # The explicit get-up begins after frame 45. End in the stable prone phase.
    "killed": [2, 6, 9, 13, 17, 20, 24, 27, 31, 35, 38, 42],
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
    low, high = np.percentile(ys, [20, 72])
    use = (ys >= low) & (ys <= high)
    return float(np.median(xs[use])), float(np.median(ys[use]))


def prepare_frames(animation: str) -> list[Image.Image]:
    source_path = SOURCE / f"SS_sylvanEnchantress_{SOURCE_NAMES[animation]}.png"
    with Image.open(source_path) as opened:
        sheet = opened.convert("RGBA")
    if sheet.width % GRID or sheet.height % GRID:
        raise ValueError(f"{source_path.name} is not an exact {GRID}x{GRID} sheet")
    size = (
        round((sheet.width // GRID) * SCALE),
        round((sheet.height // GRID) * SCALE),
    )
    minimum_component = 8 if animation == "attack" else 16
    return [
        harden_interior_alpha(
            remove_tiny_components(
                premultiplied_resize(crop_frame(sheet, number), size),
                minimum_component,
            )
        )
        for number in FRAME_SELECTIONS[animation]
    ]


def walk_placements(images: list[Image.Image]) -> list[tuple[int, int]]:
    placements = []
    for image in images:
        core_x, core_y = body_core(image)
        placements.append((round(128 - core_x), round(122 - core_y)))
    return placements


def grounded_core_placements(images: list[Image.Image]) -> list[tuple[int, int]]:
    placements = []
    for image in images:
        box = bounds(image)
        if box is None:
            placements.append((0, 0))
            continue
        core_x, _core_y = body_core(image)
        placements.append((round(128 - core_x), 244 - box[3]))
    return placements


def killed_placements(images: list[Image.Image]) -> list[tuple[int, int]]:
    downed_start = 6
    start_core_x, _start_core_y = body_core(images[0])
    start_x = round(128 - start_core_x)
    downed_boxes = [bounds(image) for image in images[downed_start:]]
    downed_boxes = [box for box in downed_boxes if box is not None]
    final_center = float(np.median([(box[0] + box[2]) / 2.0 for box in downed_boxes]))
    final_x = round(128 - final_center)

    placements: list[tuple[int, int]] = []
    for index, image in enumerate(images):
        box = bounds(image)
        if box is None:
            placements.append((0, 0))
            continue
        grounded_y = 244 - box[3]
        if index >= downed_start:
            placements.append((final_x, grounded_y))
            continue
        progress = index / float(downed_start)
        smooth = progress * progress * (3.0 - 2.0 * progress)
        x = round(start_x + (final_x - start_x) * smooth)
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
    OUTPUT.mkdir(parents=True, exist_ok=True)
    path = OUTPUT / f"sylvanEnchantress-{animation}.png"
    Image.fromarray(array, "RGBA").save(path, format="PNG", optimize=False)
    print(path.name, (CELL * len(images), CELL), "frames=" + ",".join(map(str, frame_numbers)))
    return path


def build(animation: str) -> None:
    builders = {
        "walk": walk_placements,
        "attack": grounded_core_placements,
        "damaged": grounded_core_placements,
        "killed": killed_placements,
    }
    images = prepare_frames(animation)
    assemble(animation, images, builders[animation](images))


def build_all() -> None:
    for animation in ("walk", "attack", "damaged", "killed"):
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
        with Image.open(OUTPUT / f"sylvanEnchantress-{animation}.png") as opened:
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
        contact.convert("RGB").save(REVIEW / f"sylvanEnchantress-{animation}-contact.png")
        gif_frames[0].save(
            REVIEW / f"sylvanEnchantress-{animation}.gif",
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
