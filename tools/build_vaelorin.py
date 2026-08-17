from __future__ import annotations

import json
from pathlib import Path

import numpy as np
from PIL import Image, ImageDraw, ImageFilter


CELL = 256
GRID = 9
ALPHA_CUT = 4
GROUND_Y = 244

ROOT = Path(r"C:\Users\jarox\OneDrive\Desktop\Tandem Tales\Bayou Bonanza")
SOURCE = Path(r"C:\Users\jarox\OneDrive\Desktop\TT Temp")
OUTPUT = ROOT / "assets" / "animations"
REVIEW = ROOT / "tools" / "vaelorin-review"

FRAME_SELECTIONS = {
    "walk": [
        2, 5, 9, 12, 16, 19, 22, 26, 29, 33, 36, 39,
        43, 46, 49, 53, 56, 60, 63, 66, 70, 73, 77, 80,
    ],
    "attack": [2, 9, 16, 23, 30, 37, 45, 52, 59, 66, 73, 80],
    "damaged": [2, 9, 16, 23, 30, 37, 45, 52, 59, 66, 73, 80],
    # Vaelorin is fully prone by frame 22. Recovery starts after frame 45.
    "killed": [2, 6, 10, 14, 18, 22, 25, 29, 33, 37, 41, 45],
}

SOURCE_NAMES = {
    "walk": "walking",
    "attack": "attack",
    "damaged": "damaged",
    "killed": "killed",
}

# Leave several transparent pixels around the full tail fan in every frame.
SCALES = {
    "walk": 1.00,
    "attack": 0.90,
    "damaged": 1.00,
    "killed": 0.89,
}

# The forward torso is used instead of the whole silhouette so the nine tails
# cannot drag the apparent character position left and right.
TARGET_CORE_X = {
    "walk": 172,
    "attack": 172,
    "damaged": 165,
    "killed": 153,
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
    frame = sheet.crop((left, top, left + frame_width, top + frame_height))
    return Image.fromarray(
        zero_transparent_rgb(np.asarray(frame.convert("RGBA"))), "RGBA"
    )


def resize_float_channel(channel: np.ndarray, size: tuple[int, int]) -> np.ndarray:
    image = Image.fromarray(channel.astype(np.float32), "F")
    return np.asarray(image.resize(size, Image.Resampling.LANCZOS), dtype=np.float32)


def premultiplied_resize(image: Image.Image, size: tuple[int, int]) -> Image.Image:
    rgba = np.asarray(image.convert("RGBA"), dtype=np.float32) / 255.0
    alpha = rgba[..., 3]
    premultiplied = rgba[..., :3] * alpha[..., None]

    resized_alpha = np.clip(resize_float_channel(alpha, size), 0.0, 1.0)
    resized_premultiplied = np.stack(
        [resize_float_channel(premultiplied[..., channel], size) for channel in range(3)],
        axis=-1,
    )
    resized_premultiplied = np.clip(resized_premultiplied, 0.0, 1.0)

    resized_rgb = np.zeros_like(resized_premultiplied)
    visible = resized_alpha > 1.0e-5
    resized_rgb[visible] = (
        resized_premultiplied[visible] / resized_alpha[visible, None]
    )
    output = np.dstack([resized_rgb, resized_alpha])
    output = np.clip(np.rint(output * 255.0), 0, 255).astype(np.uint8)
    return Image.fromarray(zero_transparent_rgb(output), "RGBA")


def remove_tiny_components(image: Image.Image, minimum_pixels: int = 5) -> Image.Image:
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
                    (x + 1, y), (x - 1, y), (x, y + 1), (x, y - 1)
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
    mask = Image.fromarray(
        np.where(array[..., 3] > 24, 255, 0).astype(np.uint8), "L"
    )
    interior = np.asarray(mask.filter(ImageFilter.MinFilter(5))) == 255
    array[interior, 3] = 255
    return Image.fromarray(zero_transparent_rgb(array), "RGBA")


def repair_extreme_white_fringe(
    image: Image.Image, frame_count: int
) -> tuple[Image.Image, int]:
    """Recolor only unmistakable low-alpha white contamination.

    Alpha is never reduced here. Pale fur and every opaque pixel remain intact.
    """
    array = zero_transparent_rgb(np.asarray(image.convert("RGBA")))
    output = array.copy()
    changed = 0
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
                if float(np.mean(rgb)) <= float(np.mean(replacement)) + 65:
                    continue
                output[y, left + x, :3] = np.clip(replacement, 0, 255).astype(np.uint8)
                changed += 1
    return Image.fromarray(zero_transparent_rgb(output), "RGBA"), changed


def bounds(image: Image.Image) -> tuple[int, int, int, int] | None:
    alpha = np.asarray(image.convert("RGBA"))[..., 3]
    ys, xs = np.nonzero(alpha > ALPHA_CUT)
    if len(xs) == 0:
        return None
    return int(xs.min()), int(ys.min()), int(xs.max()), int(ys.max())


def forward_body_core(image: Image.Image) -> tuple[float, float]:
    alpha = np.asarray(image.convert("RGBA"))[..., 3]
    core_mask = Image.fromarray(
        np.where(alpha > 20, 255, 0).astype(np.uint8), "L"
    )
    core = np.asarray(core_mask.filter(ImageFilter.MinFilter(13))) > 0
    ys, xs = np.nonzero(core)
    if len(xs) < 20:
        ys, xs = np.nonzero(alpha > 20)
    if len(xs) == 0:
        return image.width / 2.0, image.height / 2.0

    x_cut = np.percentile(xs, 55)
    y_low, y_high = np.percentile(ys, [25, 75])
    use = (xs >= x_cut) & (ys >= y_low) & (ys <= y_high)
    if np.count_nonzero(use) < 10:
        use = xs >= x_cut
    return float(np.median(xs[use])), float(np.median(ys[use]))


def prepare_frames(animation: str) -> list[Image.Image]:
    source_path = SOURCE / f"SS_vaelorin_{SOURCE_NAMES[animation]}.png"
    with Image.open(source_path) as opened:
        sheet = opened.convert("RGBA")
    if sheet.width % GRID or sheet.height % GRID:
        raise ValueError(f"{source_path.name} is not an exact {GRID}x{GRID} sheet")

    scale = SCALES[animation]
    size = (
        round((sheet.width // GRID) * scale),
        round((sheet.height // GRID) * scale),
    )
    return [
        harden_interior_alpha(
            remove_tiny_components(
                premultiplied_resize(crop_frame(sheet, number), size)
            )
        )
        for number in FRAME_SELECTIONS[animation]
    ]


def grounded_placements(
    animation: str, images: list[Image.Image]
) -> list[tuple[int, int]]:
    placements: list[tuple[int, int]] = []
    target_x = TARGET_CORE_X[animation]
    for image in images:
        box = bounds(image)
        if box is None:
            placements.append((0, 0))
            continue
        core_x, _core_y = forward_body_core(image)
        placements.append((round(target_x - core_x), GROUND_Y - box[3]))
    return placements


def assemble(
    animation: str,
    images: list[Image.Image],
    placements: list[tuple[int, int]],
) -> tuple[Path, int]:
    frame_numbers = FRAME_SELECTIONS[animation]
    sheet = Image.new("RGBA", (CELL * len(images), CELL), (0, 0, 0, 0))
    for index, (image, (local_x, local_y)) in enumerate(zip(images, placements)):
        box = bounds(image)
        if box is not None:
            output_box = (
                local_x + box[0], local_y + box[1],
                local_x + box[2], local_y + box[3],
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
        array[-1, left : left + CELL] = 0

    repaired, changed = repair_extreme_white_fringe(
        Image.fromarray(array, "RGBA"), len(images)
    )
    OUTPUT.mkdir(parents=True, exist_ok=True)
    output_path = OUTPUT / f"vaelorin-{animation}.png"
    repaired.save(output_path, format="PNG", optimize=False)
    return output_path, changed


def make_contact_sheet(animation: str, sheet: Image.Image) -> Path:
    frame_count = len(FRAME_SELECTIONS[animation])
    columns = 6
    rows = (frame_count + columns - 1) // columns
    preview_cell = 192
    contact = Image.new(
        "RGBA", (columns * preview_cell, rows * preview_cell), (28, 24, 35, 255)
    )
    draw = ImageDraw.Draw(contact)
    for index, source_number in enumerate(FRAME_SELECTIONS[animation]):
        frame = sheet.crop((index * CELL, 0, (index + 1) * CELL, CELL))
        frame.thumbnail((preview_cell, preview_cell), Image.Resampling.LANCZOS)
        x = (index % columns) * preview_cell
        y = (index // columns) * preview_cell
        for py in range(y, y + preview_cell, 16):
            for px in range(x, x + preview_cell, 16):
                color = (55, 50, 64, 255) if ((px // 16 + py // 16) % 2) else (38, 34, 46, 255)
                draw.rectangle((px, py, px + 15, py + 15), fill=color)
        contact.alpha_composite(frame, (x, y))
        draw.rectangle((x, y, x + 51, y + 17), fill=(0, 0, 0, 210))
        draw.text((x + 4, y + 2), f"{index + 1}: {source_number}", fill="white")
    REVIEW.mkdir(parents=True, exist_ok=True)
    path = REVIEW / f"vaelorin-{animation}-contact.png"
    contact.convert("RGB").save(path, format="PNG")
    return path


def make_edge_preview(animation: str, sheet: Image.Image) -> Path:
    frame_count = len(FRAME_SELECTIONS[animation])
    sample_indices = sorted({0, frame_count // 3, (2 * frame_count) // 3, frame_count - 1})
    backgrounds = [(0, 0, 0), (38, 18, 54), (0, 210, 70), (112, 112, 112)]
    canvas = Image.new("RGB", (len(sample_indices) * CELL, len(backgrounds) * CELL))
    for row, background in enumerate(backgrounds):
        for column, frame_index in enumerate(sample_indices):
            frame = sheet.crop((frame_index * CELL, 0, (frame_index + 1) * CELL, CELL))
            base = Image.new("RGBA", (CELL, CELL), (*background, 255))
            base.alpha_composite(frame)
            canvas.paste(base.convert("RGB"), (column * CELL, row * CELL))
    REVIEW.mkdir(parents=True, exist_ok=True)
    path = REVIEW / f"vaelorin-{animation}-edges.png"
    canvas.save(path, format="PNG")
    return path


def validate_output(animation: str, path: Path) -> dict[str, object]:
    expected_frames = len(FRAME_SELECTIONS[animation])
    with Image.open(path) as opened:
        image = opened.convert("RGBA")
    array = np.asarray(image)
    failures: list[str] = []
    if image.mode != "RGBA":
        failures.append("not RGBA")
    if image.size != (expected_frames * CELL, CELL):
        failures.append(f"wrong dimensions {image.size}")
    if np.any(array[array[..., 3] == 0, :3]):
        failures.append("nonzero RGB under zero alpha")

    core_x_values: list[float] = []
    bottom_values: list[int] = []
    for index in range(expected_frames):
        frame = image.crop((index * CELL, 0, (index + 1) * CELL, CELL))
        frame_array = np.asarray(frame)
        if np.any(frame_array[0]) or np.any(frame_array[-1]):
            failures.append(f"frame {index + 1} touches horizontal border")
        if np.any(frame_array[:, 0]) or np.any(frame_array[:, -1]):
            failures.append(f"frame {index + 1} touches vertical border")
        box = bounds(frame)
        if box is None:
            failures.append(f"frame {index + 1} is empty")
            continue
        core_x, _core_y = forward_body_core(frame)
        core_x_values.append(core_x)
        bottom_values.append(box[3])

    target_x = TARGET_CORE_X[animation]
    if core_x_values and max(abs(value - target_x) for value in core_x_values) > 1.0:
        failures.append("forward torso anchor drift exceeds 1px")
    if bottom_values and any(value != GROUND_Y for value in bottom_values):
        failures.append("ground contact is not locked")

    return {
        "animation": animation,
        "path": str(path),
        "size": list(image.size),
        "frames": expected_frames,
        "source_frames": FRAME_SELECTIONS[animation],
        "core_x_range": [min(core_x_values), max(core_x_values)] if core_x_values else None,
        "bottom_y_range": [min(bottom_values), max(bottom_values)] if bottom_values else None,
        "failures": failures,
    }


def main() -> None:
    REVIEW.mkdir(parents=True, exist_ok=True)
    report: list[dict[str, object]] = []
    for animation in FRAME_SELECTIONS:
        images = prepare_frames(animation)
        placements = grounded_placements(animation, images)
        path, fringe_pixels = assemble(animation, images, placements)
        with Image.open(path) as opened:
            sheet = opened.convert("RGBA")
        make_contact_sheet(animation, sheet)
        make_edge_preview(animation, sheet)
        result = validate_output(animation, path)
        result["fringe_pixels_recolored"] = fringe_pixels
        result["placements"] = placements
        report.append(result)
        print(
            f"{path.name}: {sheet.width}x{sheet.height}, "
            f"{len(FRAME_SELECTIONS[animation])} frames, "
            f"{fringe_pixels} fringe pixels recolored"
        )

    report_path = REVIEW / "vaelorin-validation.json"
    report_path.write_text(json.dumps(report, indent=2), encoding="ascii")
    failures = [item for item in report if item["failures"]]
    if failures:
        raise RuntimeError(f"Vaelorin validation failed; see {report_path}")
    print(f"Validation passed: {report_path}")


if __name__ == "__main__":
    main()
