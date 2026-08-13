from __future__ import annotations

from pathlib import Path

import numpy as np
from PIL import Image, ImageDraw


CELL = 256
GRID = 9
ALPHA_CUT = 10

ROOT = Path(r"C:\Users\jarox\OneDrive\Desktop\Tandem Tales\Bayou Bonanza")
SOURCE = Path(r"C:\Users\jarox\OneDrive\Desktop\TT Temp\SS_gildedCage_attack.png")
OUTPUT = ROOT / "assets" / "animations" / "gildedCage-attack.png"
REVIEW = ROOT / "tools" / "gildedCage-review"

FRAMES = [2, 5, 9, 12, 16, 19, 22, 26, 29, 33, 36, 39,
          43, 46, 49, 53, 56, 60, 63, 66, 70, 73, 77, 80]


def zero_transparent_rgb(array: np.ndarray) -> np.ndarray:
    output = array.copy()
    output[output[..., 3] <= ALPHA_CUT] = 0
    output[output[..., 3] == 0, :3] = 0
    return output


def prepare_source_alpha(image: Image.Image) -> Image.Image:
    array = zero_transparent_rgb(np.asarray(image.convert("RGBA")))
    alpha = array[..., 3]
    array[alpha >= 192, 3] = 255

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


def build() -> None:
    with Image.open(SOURCE) as opened:
        sheet = opened.convert("RGBA")
    frame_width = sheet.width // GRID
    frame_height = sheet.height // GRID
    scale = 250.0 / frame_height
    resized_size = (round(frame_width * scale), round(frame_height * scale))

    # These are fixed coordinates of the physical cage in every source cell.
    # Keeping one transform prevents the moving door and glow from shifting it.
    source_anchor_x = 121.5
    source_ground_y = 222.0
    placement = (
        round(128 - source_anchor_x * scale),
        round(244 - source_ground_y * scale),
    )

    frames = [
        premultiplied_resize(
            prepare_source_alpha(crop_frame(sheet, frame_number)), resized_size
        )
        for frame_number in FRAMES
    ]

    output = Image.new("RGBA", (CELL * len(frames), CELL), (0, 0, 0, 0))
    for index, frame in enumerate(frames):
        output.alpha_composite(frame, (index * CELL + placement[0], placement[1]))

    array = zero_transparent_rgb(np.asarray(output))
    for index in range(len(frames)):
        left = index * CELL
        array[:, left] = 0
        array[:, left + CELL - 1] = 0
        array[0, left : left + CELL] = 0
        array[CELL - 1, left : left + CELL] = 0
    OUTPUT.parent.mkdir(parents=True, exist_ok=True)
    Image.fromarray(array, "RGBA").save(OUTPUT, format="PNG", optimize=False)
    print(
        OUTPUT.name,
        Image.open(OUTPUT).size,
        f"scale={scale:.6f}",
        "frames=" + ",".join(str(frame) for frame in FRAMES),
    )


def write_review_previews() -> None:
    REVIEW.mkdir(parents=True, exist_ok=True)
    with Image.open(OUTPUT) as opened:
        sheet = opened.convert("RGBA")
    frames = [
        sheet.crop((index * CELL, 0, (index + 1) * CELL, CELL))
        for index in range(len(FRAMES))
    ]

    checker = Image.new("RGBA", (CELL, CELL), (42, 42, 42, 255))
    draw = ImageDraw.Draw(checker)
    for y in range(0, CELL, 16):
        for x in range(0, CELL, 16):
            if (x // 16 + y // 16) % 2:
                draw.rectangle((x, y, x + 15, y + 15), fill=(72, 64, 78, 255))

    contact = Image.new("RGBA", (6 * CELL, 4 * CELL), (28, 28, 28, 255))
    gif_frames = []
    for index, (frame, source_number) in enumerate(zip(frames, FRAMES)):
        tile = checker.copy()
        tile.alpha_composite(frame)
        ImageDraw.Draw(tile).text(
            (5, 5), f"{index + 1}: src {source_number}", fill="white"
        )
        contact.alpha_composite(tile, ((index % 6) * CELL, (index // 6) * CELL))
        gif_frames.append(tile.convert("P", palette=Image.Palette.ADAPTIVE))
    contact.convert("RGB").save(REVIEW / "gildedCage-attack-contact.png")
    gif_frames[0].save(
        REVIEW / "gildedCage-attack.gif",
        save_all=True,
        append_images=gif_frames[1:],
        duration=85,
        loop=0,
        disposal=2,
    )


if __name__ == "__main__":
    build()
