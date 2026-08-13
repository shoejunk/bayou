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
SOURCE = Path(r"C:\Users\jarox\OneDrive\Desktop\TT Temp\SS_blackPowderKeg_attack.png")
OUTPUT = ROOT / "assets" / "animations" / "blackPowderKeg-attack.png"
REVIEW = ROOT / "tools" / "blackPowderKeg-review"

# The source returns to an unlit keg after frame 66. Ending at the flame peak
# creates a direct fuse-to-detonation transition instead of an extinguished pause.
FUSE_FRAMES = [2, 8, 14, 19, 25, 31, 37, 43, 49, 54, 60, 66]


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


def harden_keg_alpha(image: Image.Image) -> Image.Image:
    array = zero_transparent_rgb(np.asarray(image.convert("RGBA")))
    array[array[..., 3] >= 192, 3] = 255
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


def with_opacity(image: Image.Image, opacity: float) -> Image.Image:
    array = np.asarray(image.convert("RGBA")).copy()
    array[..., 3] = np.clip(
        array[..., 3].astype(np.float32) * opacity, 0, 255
    ).astype(np.uint8)
    return Image.fromarray(zero_transparent_rgb(array), "RGBA")


SUPER = 3
ORIGIN = (128.0, 184.0)


def scaled(value: float) -> int:
    return round(value * SUPER)


def ellipse(
    draw: ImageDraw.ImageDraw,
    center_x: float,
    center_y: float,
    radius_x: float,
    radius_y: float,
    fill: tuple[int, int, int, int],
) -> None:
    draw.ellipse(
        (
            scaled(center_x - radius_x),
            scaled(center_y - radius_y),
            scaled(center_x + radius_x),
            scaled(center_y + radius_y),
        ),
        fill=fill,
    )


def fractal_noise(seed: int, frame_index: int) -> np.ndarray:
    size = CELL * SUPER
    output = np.zeros((size, size), dtype=np.float32)
    total_weight = 0.0
    for octave, (grid_size, weight) in enumerate(((7, 0.56), (13, 0.29), (25, 0.15))):
        random = np.random.default_rng(seed + octave * 997)
        grid = random.integers(0, 256, (grid_size, grid_size), dtype=np.uint8)
        enlarged = np.asarray(
            Image.fromarray(grid, "L").resize((size, size), Image.Resampling.BICUBIC),
            dtype=np.float32,
        ) / 255.0
        shift_y = frame_index * (5 + octave * 3) * SUPER
        shift_x = frame_index * (3 - octave) * SUPER
        output += np.roll(enlarged, (shift_y, shift_x), axis=(0, 1)) * weight
        total_weight += weight
    return output / total_weight


def fireball_layer(frame_index: int) -> Image.Image:
    growth = [16, 38, 68, 94, 112, 120, 123, 120, 112, 98, 80, 60][frame_index]
    fire_strength = [1.0, 1.0, 1.0, 1.0, 0.96, 0.88, 0.74, 0.56, 0.38, 0.22, 0.10, 0.0][frame_index]
    if fire_strength <= 0:
        return Image.new("RGBA", (CELL * SUPER, CELL * SUPER), (0, 0, 0, 0))

    y, x = np.mgrid[: CELL * SUPER, : CELL * SUPER].astype(np.float32)
    x /= SUPER
    y /= SUPER
    noise = fractal_noise(271, frame_index)
    detail = fractal_noise(619, frame_index) - 0.5
    center_x = ORIGIN[0] + math.sin(frame_index * 0.61) * 2.2
    center_y = ORIGIN[1] - min(frame_index, 7) * 1.4
    radius_x = max(4.0, growth * 0.93)
    radius_up = max(4.0, growth * 0.78)
    radius_down = max(4.0, growth * 0.50)
    normalized_x = (x - center_x) / radius_x
    normalized_y = np.where(
        y < center_y,
        (y - center_y) / radius_up,
        (y - center_y) / radius_down,
    )
    distance = np.sqrt(normalized_x**2 + normalized_y**2)
    angle = np.arctan2(normalized_y, normalized_x)
    irregular_boundary = (
        0.88
        + (noise - 0.5) * 0.42
        + np.sin(angle * 7.0 + frame_index * 0.82) * 0.055
        + np.sin(angle * 11.0 - frame_index * 0.47) * 0.035
    )
    signed_depth = irregular_boundary - distance
    alpha = np.clip((signed_depth + 0.06) / 0.19, 0.0, 1.0) ** 0.72
    alpha *= fire_strength

    heat = np.clip(signed_depth * 1.18 + noise * 0.38 + detail * 0.30, 0.0, 1.0)
    veins = np.clip(
        np.sin(x * 0.105 + noise * 8.0 - frame_index * 0.75)
        + np.sin(y * 0.082 - noise * 6.0 + frame_index * 0.52),
        -1.0,
        1.0,
    )
    heat = np.clip(heat + veins * 0.10, 0.0, 1.0)
    red = np.clip(66.0 + np.sqrt(heat) * 248.0, 0, 255)
    green = np.clip(2.0 + np.power(heat, 1.55) * 225.0, 0, 232)
    blue = np.clip(np.power(heat, 4.2) * 80.0, 0, 82)
    array = np.dstack([red, green, blue, alpha * 255.0]).astype(np.uint8)
    flame = Image.fromarray(zero_transparent_rgb(array), "RGBA")
    if frame_index < 3:
        flame = flame.filter(ImageFilter.GaussianBlur(scaled(1.15 - frame_index * 0.20)))
    glow = with_opacity(
        flame.filter(ImageFilter.GaussianBlur(scaled(max(1.4, growth * 0.028)))),
        0.30,
    )
    glow.alpha_composite(flame)
    return glow


def smoke_layer(frame_index: int) -> Image.Image:
    smoke_strength = [0.0, 0.0, 0.08, 0.18, 0.34, 0.52, 0.70, 0.82, 0.86, 0.76, 0.58, 0.34][frame_index]
    if smoke_strength <= 0:
        return Image.new("RGBA", (CELL * SUPER, CELL * SUPER), (0, 0, 0, 0))

    age = max(0, frame_index - 2)
    y, x = np.mgrid[: CELL * SUPER, : CELL * SUPER].astype(np.float32)
    x /= SUPER
    y /= SUPER
    noise = fractal_noise(887, frame_index)
    detail = fractal_noise(1231, frame_index)
    center_x = ORIGIN[0] + math.sin(age * 0.53) * 4.0
    center_y = ORIGIN[1] - age * 7.5
    radius_x = min(121.0, 28.0 + age * 12.0)
    radius_up = min(126.0, 30.0 + age * 13.0)
    radius_down = min(76.0, 26.0 + age * 5.0)
    normalized_x = (x - center_x) / radius_x
    normalized_y = np.where(
        y < center_y,
        (y - center_y) / radius_up,
        (y - center_y) / radius_down,
    )
    distance = np.sqrt(normalized_x**2 + normalized_y**2)
    angle = np.arctan2(normalized_y, normalized_x)
    boundary = (
        0.91
        + (noise - 0.5) * 0.45
        + np.sin(angle * 6.0 - frame_index * 0.36) * 0.05
        + np.sin(angle * 10.0 + frame_index * 0.24) * 0.03
    )
    signed_depth = boundary - distance
    density = np.clip((signed_depth + 0.07) / 0.28, 0.0, 1.0)
    density *= np.clip(0.72 + noise * 0.42 + (detail - 0.5) * 0.24, 0.0, 1.0)
    alpha = np.clip(density, 0.0, 1.0) ** 0.82 * smoke_strength
    shade = np.clip(25.0 + (1.0 - density) * 70.0 + (noise - 0.5) * 24.0, 22, 106)
    array = np.dstack(
        [
            shade,
            shade * 0.94,
            shade * 0.91,
            alpha * 225.0,
        ]
    ).astype(np.uint8)
    return Image.fromarray(zero_transparent_rgb(array), "RGBA").filter(
        ImageFilter.GaussianBlur(scaled(0.45))
    )


def build_fuse_frames() -> tuple[list[Image.Image], Image.Image]:
    with Image.open(SOURCE) as opened:
        sheet = opened.convert("RGBA")
    if sheet.width % GRID or sheet.height % GRID:
        raise ValueError(f"{SOURCE.name} is not an exact {GRID}x{GRID} sheet")

    raw = [harden_keg_alpha(crop_frame(sheet, frame)) for frame in FUSE_FRAMES]
    frames = []
    for image in raw:
        canvas = Image.new("RGBA", (CELL, CELL), (0, 0, 0, 0))
        # Fixed source-space anchor: the keg's base is x=52.5, y=243.
        canvas.alpha_composite(image, (round(128 - 52.5), 244 - 243))
        frames.append(canvas)
    return frames, raw[0]


def build_explosion_frames(keg: Image.Image) -> list[Image.Image]:
    frames = []
    for index in range(12):
        canvas = Image.new("RGBA", (CELL * SUPER, CELL * SUPER), (0, 0, 0, 0))
        if index < 3:
            keg_opacity = [0.92, 0.48, 0.12][index]
            large_keg = premultiplied_resize(keg, (keg.width * SUPER, keg.height * SUPER))
            canvas.alpha_composite(
                with_opacity(large_keg, keg_opacity),
                (scaled(76), scaled(1)),
            )

        canvas.alpha_composite(smoke_layer(index))
        canvas.alpha_composite(fireball_layer(index))
        frames.append(premultiplied_resize(canvas, (CELL, CELL)))
    return frames


def clear_frame_edges(image: Image.Image) -> Image.Image:
    array = zero_transparent_rgb(np.asarray(image.convert("RGBA")))
    array[0] = 0
    array[-1] = 0
    array[:, 0] = 0
    array[:, -1] = 0
    return Image.fromarray(array, "RGBA")


def build() -> None:
    fuse_frames, keg = build_fuse_frames()
    explosion_frames = build_explosion_frames(keg)
    frames = [clear_frame_edges(frame) for frame in fuse_frames + explosion_frames]

    sheet = Image.new("RGBA", (CELL * len(frames), CELL), (0, 0, 0, 0))
    for index, frame in enumerate(frames):
        sheet.alpha_composite(frame, (index * CELL, 0))
    array = zero_transparent_rgb(np.asarray(sheet))
    OUTPUT.parent.mkdir(parents=True, exist_ok=True)
    Image.fromarray(array, "RGBA").save(OUTPUT, format="PNG", optimize=False)
    print(
        OUTPUT.name,
        Image.open(OUTPUT).size,
        "fuse=" + ",".join(map(str, FUSE_FRAMES)),
        "explosion=12",
    )


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
    with Image.open(OUTPUT) as opened:
        sheet = opened.convert("RGBA")
    frames = [sheet.crop((index * CELL, 0, (index + 1) * CELL, CELL)) for index in range(24)]
    checker = checkerboard()
    contact = Image.new("RGBA", (6 * CELL, 4 * CELL), (28, 28, 28, 255))
    gif_frames = []
    for index, frame in enumerate(frames):
        tile = checker.copy()
        tile.alpha_composite(frame)
        label = f"{index + 1}: src {FUSE_FRAMES[index]}" if index < 12 else f"{index + 1}: blast {index - 11}"
        ImageDraw.Draw(tile).text((5, 5), label, fill="white")
        contact.alpha_composite(tile, ((index % 6) * CELL, (index // 6) * CELL))
        gif_frames.append(tile.convert("P", palette=Image.Palette.ADAPTIVE))
    contact.convert("RGB").save(REVIEW / "blackPowderKeg-attack-contact.png")
    gif_frames[0].save(
        REVIEW / "blackPowderKeg-attack.gif",
        save_all=True,
        append_images=gif_frames[1:],
        duration=85,
        loop=0,
        disposal=2,
    )

    backgrounds = [
        (0, 0, 0, 255),
        (38, 8, 55, 255),
        (0, 220, 50, 255),
        (118, 118, 118, 255),
    ]
    sample_indices = [12, 14, 17, 20, 23]
    multibackground = Image.new(
        "RGBA", (len(sample_indices) * CELL, len(backgrounds) * CELL), (0, 0, 0, 255)
    )
    for column, frame_index in enumerate(sample_indices):
        for row, background in enumerate(backgrounds):
            tile = Image.new("RGBA", (CELL, CELL), background)
            tile.alpha_composite(frames[frame_index])
            ImageDraw.Draw(tile).text(
                (5, 5), f"blast {frame_index - 11}", fill="white"
            )
            multibackground.alpha_composite(tile, (column * CELL, row * CELL))
    multibackground.convert("RGB").save(REVIEW / "blackPowderKeg-blast-multibg.png")


def main() -> None:
    parser = argparse.ArgumentParser()
    parser.add_argument("--review-only", action="store_true")
    args = parser.parse_args()
    if not args.review_only:
        build()
    write_reviews()


if __name__ == "__main__":
    main()
