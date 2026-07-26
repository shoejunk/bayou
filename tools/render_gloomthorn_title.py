#!/usr/bin/env python3
"""Render the Gloomthorn title texture from Gloomthorn Display."""

from __future__ import annotations

import argparse
import math
import random
from pathlib import Path

from PIL import Image, ImageChops, ImageDraw, ImageFilter, ImageFont, ImageOps


WIDTH = 1600
HEIGHT = 320
TEXT = "Gloomthorn"


def shifted(mask: Image.Image, x: int, y: int) -> Image.Image:
    result = Image.new("L", mask.size)
    source_left = max(0, -x)
    source_top = max(0, -y)
    source_right = min(mask.width, mask.width - x)
    source_bottom = min(mask.height, mask.height - y)
    if source_right > source_left and source_bottom > source_top:
        result.paste(
            mask.crop((source_left, source_top, source_right, source_bottom)),
            (source_left + x, source_top + y),
        )
    return result


def scaled_alpha(mask: Image.Image, scale: float) -> Image.Image:
    return mask.point(lambda value: min(255, round(value * scale)))


def gradient_image(size: tuple[int, int]) -> Image.Image:
    stops = [
        (0.00, (255, 236, 174)),
        (0.16, (205, 146, 65)),
        (0.36, (255, 224, 145)),
        (0.55, (150, 91, 34)),
        (0.73, (232, 184, 91)),
        (1.00, (104, 58, 24)),
    ]
    gradient = Image.new("RGB", size)
    draw = ImageDraw.Draw(gradient)
    for y in range(size[1]):
        position = y / max(1, size[1] - 1)
        for index in range(len(stops) - 1):
            start_position, start_color = stops[index]
            end_position, end_color = stops[index + 1]
            if position <= end_position:
                amount = (position - start_position) / (end_position - start_position)
                color = tuple(
                    round(start + (end - start) * amount)
                    for start, end in zip(start_color, end_color)
                )
                draw.line((0, y, size[0], y), fill=color)
                break
    return gradient


def fit_font(font_path: Path) -> ImageFont.FreeTypeFont:
    size = 300
    while size > 20:
        font = ImageFont.truetype(str(font_path), size)
        bounds = font.getbbox(TEXT)
        if bounds[2] - bounds[0] <= WIDTH - 110 and bounds[3] - bounds[1] <= HEIGHT - 82:
            return font
        size -= 2
    raise RuntimeError("Could not fit title text")


def render(font_path: Path, output: Path) -> None:
    font = fit_font(font_path)
    bounds = font.getbbox(TEXT)
    text_width = bounds[2] - bounds[0]
    text_height = bounds[3] - bounds[1]
    position = (
        round((WIDTH - text_width) * 0.5 - bounds[0]),
        round((HEIGHT - text_height) * 0.5 - bounds[1] - 2),
    )

    mask = Image.new("L", (WIDTH, HEIGHT))
    ImageDraw.Draw(mask).text(position, TEXT, font=font, fill=255)

    result = Image.new("RGBA", (WIDTH, HEIGHT))

    glow = scaled_alpha(mask.filter(ImageFilter.GaussianBlur(18)), 0.12)
    glow_layer = Image.new("RGBA", result.size, (219, 145, 48, 0))
    glow_layer.putalpha(glow)
    result.alpha_composite(glow_layer)

    shadow = shifted(mask, 7, 10).filter(ImageFilter.GaussianBlur(7))
    shadow_layer = Image.new("RGBA", result.size, (0, 0, 0, 0))
    shadow_layer.putalpha(scaled_alpha(shadow, 0.78))
    result.alpha_composite(shadow_layer)

    outer = ImageChops.subtract(mask.filter(ImageFilter.MaxFilter(15)), mask)
    outer_layer = Image.new("RGBA", result.size, (54, 29, 15, 0))
    outer_layer.putalpha(scaled_alpha(outer, 0.96))
    result.alpha_composite(outer_layer)

    bright_rim = ImageChops.subtract(
        mask.filter(ImageFilter.MaxFilter(7)),
        mask.filter(ImageFilter.MaxFilter(3)),
    )
    rim_layer = Image.new("RGBA", result.size, (255, 214, 126, 0))
    rim_layer.putalpha(scaled_alpha(bright_rim, 0.9))
    result.alpha_composite(rim_layer)

    gold = gradient_image(result.size)
    noise = Image.effect_noise(result.size, 31).filter(ImageFilter.GaussianBlur(0.7))
    metallic = ImageOps.colorize(noise, (118, 73, 29), (255, 238, 175))
    gold = Image.blend(gold, metallic, 0.17)
    fill_layer = gold.convert("RGBA")
    fill_layer.putalpha(mask)
    result.alpha_composite(fill_layer)

    inner_edge = ImageChops.subtract(mask, mask.filter(ImageFilter.MinFilter(9)))
    inner_layer = Image.new("RGBA", result.size, (77, 41, 18, 0))
    inner_layer.putalpha(scaled_alpha(inner_edge, 0.42))
    result.alpha_composite(inner_layer)

    top_left = ImageChops.subtract(mask, shifted(mask, 3, 3))
    highlight_layer = Image.new("RGBA", result.size, (255, 246, 204, 0))
    highlight_layer.putalpha(scaled_alpha(top_left, 0.82))
    result.alpha_composite(highlight_layer)

    bottom_right = ImageChops.subtract(mask, shifted(mask, -3, -3))
    shade_layer = Image.new("RGBA", result.size, (55, 28, 13, 0))
    shade_layer.putalpha(scaled_alpha(bottom_right, 0.72))
    result.alpha_composite(shade_layer)

    scratches = Image.new("L", result.size)
    scratch_draw = ImageDraw.Draw(scratches)
    randomizer = random.Random(0x6100_7A)
    for _ in range(155):
        x = randomizer.randrange(65, WIDTH - 65)
        y = randomizer.randrange(55, HEIGHT - 55)
        length = randomizer.randrange(4, 18)
        angle = randomizer.uniform(-0.3, 0.3)
        scratch_draw.line(
            (x, y, x + math.cos(angle) * length, y + math.sin(angle) * length),
            fill=randomizer.randrange(28, 78),
            width=1,
        )
    scratches = ImageChops.multiply(scratches, mask)
    scratch_layer = Image.new("RGBA", result.size, (62, 34, 18, 0))
    scratch_layer.putalpha(scratches)
    result.alpha_composite(scratch_layer)

    output.parent.mkdir(parents=True, exist_ok=True)
    result.save(output, optimize=True)


def main() -> None:
    parser = argparse.ArgumentParser()
    parser.add_argument("font", type=Path)
    parser.add_argument("output", type=Path)
    args = parser.parse_args()
    render(args.font, args.output)


if __name__ == "__main__":
    main()
