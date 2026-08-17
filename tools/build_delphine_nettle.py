from __future__ import annotations

import json
from pathlib import Path

import numpy as np
from PIL import Image, ImageDraw, ImageFilter

from animation_halo_cleanup import suspicious_count
from build_glimmer_stag import (
    bounds,
    harden_interior_alpha,
    premultiplied_resize,
    remove_tiny_components,
    zero_transparent_rgb,
)
from build_maggie_mudroot_mounted import repair_sheet


CELL = 256
GRID = 9
GROUND_Y = 244
MAX_CONTENT_WIDTH = 246
MAX_CONTENT_HEIGHT = 238

ROOT = Path(r"C:\Users\jarox\OneDrive\Desktop\Tandem Tales\Bayou Bonanza")
SOURCE = Path(r"C:\Users\jarox\OneDrive\Desktop\TT Temp")
OUTPUT = ROOT / "assets" / "animations"
REVIEW = ROOT / "tools" / "delphineNettle-review"

SOURCE_NAMES = {
    "attack": "attack",
    "damaged": "damaged",
    "killed": "killed",
    "walk": "walking",
}

# Attack includes the complete magical burst. Damage and walk sample their
# complete middle motion. Death stops at frame 50 while Delphine is still prone.
FRAME_SELECTIONS = {
    "walk": [
        2, 5, 9, 12, 16, 19, 22, 26, 29, 33, 36, 39,
        43, 46, 49, 53, 56, 60, 63, 66, 70, 73, 77, 80,
    ],
    "attack": [2, 8, 13, 19, 24, 30, 35, 41, 46, 52, 57, 63],
    "damaged": [2, 9, 16, 23, 30, 37, 44, 51, 58, 65, 72, 80],
    "killed": [2, 6, 11, 15, 19, 24, 28, 33, 37, 41, 46, 50],
}


def crop_frame(sheet: Image.Image, frame_number: int) -> Image.Image:
    if sheet.width % GRID or sheet.height % GRID:
        raise ValueError(f"{sheet.width}x{sheet.height} is not an exact 9x9 sheet")
    frame_width = sheet.width // GRID
    frame_height = sheet.height // GRID
    index = frame_number - 1
    left = (index % GRID) * frame_width
    top = (index // GRID) * frame_height
    frame = sheet.crop((left, top, left + frame_width, top + frame_height))
    return Image.fromarray(
        zero_transparent_rgb(np.asarray(frame.convert("RGBA"))), "RGBA"
    )


def character_mask(image: Image.Image, erode: bool = False) -> np.ndarray:
    """Exclude Delphine's bright neutral spell while retaining colored artwork."""
    array = np.asarray(image.convert("RGBA"))
    rgb = array[..., :3]
    alpha = array[..., 3]
    brightness = np.mean(rgb, axis=2)
    chroma = np.max(rgb, axis=2) - np.min(rgb, axis=2)
    mask = (alpha > 28) & ((brightness < 215) | (chroma > 42))
    if erode:
        mask = np.asarray(
            Image.fromarray(np.where(mask, 255, 0).astype(np.uint8), "L").filter(
                ImageFilter.MinFilter(7)
            )
        ) > 0
    return mask


def mask_bounds(mask: np.ndarray) -> tuple[int, int, int, int] | None:
    ys, xs = np.nonzero(mask)
    if len(xs) == 0:
        return None
    return int(xs.min()), int(ys.min()), int(xs.max()), int(ys.max())


def body_core_x(image: Image.Image) -> float:
    """Track the torso without staff tips, limbs, or the white spell burst."""
    core = character_mask(image, erode=True)
    box = mask_bounds(core)
    if box is None:
        visible = bounds(image)
        return image.width / 2.0 if visible is None else (visible[0] + visible[2]) / 2.0
    left, top, right, bottom = box
    height = bottom - top + 1
    yy, _xx = np.indices(core.shape)
    core &= (yy >= top + round(height * 0.08)) & (
        yy <= top + round(height * 0.72)
    )
    _ys, xs = np.nonzero(core)
    if len(xs) < 10:
        return (left + right) / 2.0
    return float(np.median(xs))


def ground_contact(image: Image.Image) -> tuple[float, float]:
    """Find the lower planted foot while ignoring spell loops and staff tips."""
    mask = character_mask(image)
    box = mask_bounds(mask)
    if box is None:
        return image.width / 2.0, image.height - 1.0
    left, top, right, bottom = box
    width = right - left + 1
    height = bottom - top + 1
    center_x = body_core_x(image)
    half_width = max(28, round(width * 0.42))
    yy, xx = np.indices(mask.shape)
    contact = (
        mask
        & (xx >= center_x - half_width)
        & (xx <= center_x + half_width)
        & (yy >= top + round(height * 0.50))
    )
    ys, xs = np.nonzero(contact)
    if len(xs) < 5:
        return center_x, float(bottom)
    low = int(ys.max())
    use = ys >= low - 5
    return float(np.median(xs[use])), float(low)


def animation_anchor(animation: str, image: Image.Image) -> tuple[float, float]:
    if animation in {"walk", "killed"}:
        return body_core_x(image), ground_contact(image)[1]
    return ground_contact(image)


def load_raw_frames() -> dict[str, list[Image.Image]]:
    output: dict[str, list[Image.Image]] = {}
    for animation, frame_numbers in FRAME_SELECTIONS.items():
        source_path = SOURCE / f"SS_delphineNettle_{SOURCE_NAMES[animation]}.png"
        with Image.open(source_path) as opened:
            sheet = opened.convert("RGBA")
        if sheet.width % GRID or sheet.height % GRID:
            raise ValueError(f"{source_path.name} is not an exact 9x9 sheet")
        output[animation] = [
            harden_interior_alpha(
                remove_tiny_components(crop_frame(sheet, frame_number), 4)
            )
            for frame_number in frame_numbers
        ]
    return output


def shared_scale(raw: dict[str, list[Image.Image]]) -> float:
    boxes = [
        box
        for images in raw.values()
        for image in images
        if (box := bounds(image)) is not None
    ]
    widest = max(box[2] - box[0] + 1 for box in boxes)
    tallest = max(box[3] - box[1] + 1 for box in boxes)
    scale = min(MAX_CONTENT_WIDTH / widest, MAX_CONTENT_HEIGHT / tallest, 1.0)
    for animation, images in raw.items():
        lefts: list[float] = []
        rights: list[float] = []
        tops: list[float] = []
        bottoms: list[float] = []
        for image in images:
            box = bounds(image)
            if box is None:
                continue
            anchor_x, anchor_y = animation_anchor(animation, image)
            lefts.append(box[0] - anchor_x)
            rights.append(box[2] - anchor_x)
            tops.append(box[1] - anchor_y)
            bottoms.append(box[3] - anchor_y)
        scale = min(
            scale,
            MAX_CONTENT_WIDTH / (max(rights) - min(lefts) + 1),
            MAX_CONTENT_HEIGHT / (max(bottoms) - min(tops) + 1),
        )
    return scale


def fitted_target_x(animation: str, images: list[Image.Image]) -> int:
    lower = -10_000.0
    upper = 10_000.0
    centered: list[float] = []
    for image in images:
        box = bounds(image)
        if box is None:
            continue
        anchor_x, _anchor_y = animation_anchor(animation, image)
        left, _top, right, _bottom = box
        lower = max(lower, anchor_x + 3 - left)
        upper = min(upper, anchor_x + CELL - 4 - right)
        centered.append(128.0 + anchor_x - (left + right) / 2.0)
    if lower > upper:
        raise RuntimeError(f"anchored frames cannot fit: {lower:.2f}>{upper:.2f}")
    return round(min(max(float(np.median(centered)), lower), upper))


def grounded_placements(
    animation: str, images: list[Image.Image]
) -> tuple[list[tuple[int, int]], tuple[int, int]]:
    target_x = fitted_target_x(animation, images)
    placements = []
    for image in images:
        anchor_x, anchor_y = animation_anchor(animation, image)
        placements.append((round(target_x - anchor_x), round(GROUND_Y - anchor_y)))
    return placements, (target_x, GROUND_Y)


def assemble(
    animation: str,
    images: list[Image.Image],
    placements: list[tuple[int, int]],
) -> tuple[Path, dict[str, int]]:
    sheet = Image.new("RGBA", (CELL * len(images), CELL), (0, 0, 0, 0))
    for index, (image, (local_x, local_y)) in enumerate(zip(images, placements)):
        box = bounds(image)
        if box is not None:
            output_box = (
                local_x + box[0], local_y + box[1],
                local_x + box[2], local_y + box[3],
            )
            if (
                output_box[0] < 3 or output_box[1] < 3
                or output_box[2] > CELL - 4 or output_box[3] > CELL - 4
            ):
                source_frame = FRAME_SELECTIONS[animation][index]
                raise RuntimeError(
                    f"{animation} source frame {source_frame} clips at {output_box}"
                )
        sheet.alpha_composite(image, (index * CELL + local_x, local_y))
    array = zero_transparent_rgb(np.asarray(sheet))
    for index in range(len(images)):
        left = index * CELL
        array[:, left] = 0
        array[:, left + CELL - 1] = 0
        array[0, left : left + CELL] = 0
        array[-1, left : left + CELL] = 0
    repaired, cleanup = repair_sheet(Image.fromarray(array, "RGBA"), len(images))
    OUTPUT.mkdir(parents=True, exist_ok=True)
    path = OUTPUT / f"delphineNettle-{animation}.png"
    repaired.save(path, format="PNG", optimize=False)
    return path, cleanup


def make_contact_sheet(animation: str, sheet: Image.Image) -> Path:
    frame_count = len(FRAME_SELECTIONS[animation])
    rows = (frame_count + 5) // 6
    contact = Image.new("RGBA", (1152, rows * 192), (28, 24, 35, 255))
    draw = ImageDraw.Draw(contact)
    for index, source_number in enumerate(FRAME_SELECTIONS[animation]):
        frame = sheet.crop((index * CELL, 0, (index + 1) * CELL, CELL))
        frame.thumbnail((192, 192), Image.Resampling.LANCZOS)
        x = (index % 6) * 192
        y = (index // 6) * 192
        for py in range(y, y + 192, 16):
            for px in range(x, x + 192, 16):
                color = (55, 50, 64, 255) if ((px // 16 + py // 16) % 2) else (38, 34, 46, 255)
                draw.rectangle((px, py, px + 15, py + 15), fill=color)
        contact.alpha_composite(frame, (x, y))
        draw.rectangle((x, y, x + 61, y + 17), fill=(0, 0, 0, 210))
        draw.text((x + 4, y + 2), f"{index + 1}: {source_number}", fill="white")
    REVIEW.mkdir(parents=True, exist_ok=True)
    path = REVIEW / f"delphineNettle-{animation}-contact.png"
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
    path = REVIEW / f"delphineNettle-{animation}-edges.png"
    canvas.save(path, format="PNG")
    return path


def validate_output(
    animation: str,
    path: Path,
    placements: list[tuple[int, int]],
    target: tuple[int, int],
) -> dict[str, object]:
    frame_count = len(FRAME_SELECTIONS[animation])
    with Image.open(path) as opened:
        source_mode = opened.mode
        image = opened.convert("RGBA")
    array = np.asarray(image)
    failures: list[str] = []
    if source_mode != "RGBA":
        failures.append(f"mode is {source_mode}, not RGBA")
    if image.size != (frame_count * CELL, CELL):
        failures.append(f"wrong dimensions {image.size}")
    if np.any(array[array[..., 3] == 0, :3]):
        failures.append("nonzero RGB under zero alpha")
    anchor_x_values: list[float] = []
    anchor_y_values: list[float] = []
    frame_bounds: list[list[int]] = []
    for index in range(frame_count):
        frame = image.crop((index * CELL, 0, (index + 1) * CELL, CELL))
        frame_array = np.asarray(frame)
        if np.any(frame_array[0]) or np.any(frame_array[-1]):
            failures.append(f"frame {index + 1} touches a horizontal border")
        if np.any(frame_array[:, 0]) or np.any(frame_array[:, -1]):
            failures.append(f"frame {index + 1} touches a vertical border")
        box = bounds(frame)
        if box is None:
            failures.append(f"frame {index + 1} is empty")
            continue
        frame_bounds.append(list(box))
        anchor_x, anchor_y = animation_anchor(animation, frame)
        anchor_x_values.append(anchor_x)
        anchor_y_values.append(anchor_y)
    if anchor_x_values and max(abs(value - target[0]) for value in anchor_x_values) > 1.5:
        failures.append("horizontal anchor drift exceeds 1.5px")
    if anchor_y_values and max(abs(value - target[1]) for value in anchor_y_values) > 1.5:
        failures.append("ground anchor drift exceeds 1.5px")
    return {
        "animation": animation,
        "path": str(path),
        "size": list(image.size),
        "mode": source_mode,
        "frames": frame_count,
        "source_grid": [9, 9],
        "source_frames": FRAME_SELECTIONS[animation],
        "target_anchor": list(target),
        "anchor_x_range": [min(anchor_x_values), max(anchor_x_values)] if anchor_x_values else None,
        "anchor_y_range": [min(anchor_y_values), max(anchor_y_values)] if anchor_y_values else None,
        "frame_bounds": frame_bounds,
        "placements": placements,
        "suspicious_final": suspicious_count(array),
        "failures": failures,
    }


def main() -> None:
    raw = load_raw_frames()
    scale = shared_scale(raw)
    resized = {
        animation: [
            premultiplied_resize(image, (round(image.width * scale), round(image.height * scale)))
            for image in images
        ]
        for animation, images in raw.items()
    }
    placements: dict[str, list[tuple[int, int]]] = {}
    targets: dict[str, tuple[int, int]] = {}
    for animation, images in resized.items():
        placements[animation], targets[animation] = grounded_placements(animation, images)

    report: list[dict[str, object]] = []
    for animation, images in resized.items():
        path, cleanup = assemble(animation, images, placements[animation])
        with Image.open(path) as opened:
            sheet = opened.convert("RGBA")
        make_contact_sheet(animation, sheet)
        make_edge_preview(animation, sheet)
        result = validate_output(animation, path, placements[animation], targets[animation])
        result["scale"] = scale
        result["halo_cleanup"] = cleanup
        report.append(result)
        print(
            f"{path.name}: {sheet.width}x{sheet.height}, "
            f"{len(FRAME_SELECTIONS[animation])} frames, "
            f"{cleanup['pixels_recolored']} halo pixels recolored, "
            f"suspicious {cleanup['suspicious_before']}->{cleanup['suspicious_after']}"
        )

    REVIEW.mkdir(parents=True, exist_ok=True)
    report_path = REVIEW / "delphineNettle-validation.json"
    report_path.write_text(json.dumps(report, indent=2), encoding="ascii")
    failures = [item for item in report if item["failures"]]
    if failures:
        raise RuntimeError(f"Delphine Nettle validation failed; see {report_path}")
    print(f"Shared scale: {scale:.6f}")
    print(f"Validation passed: {report_path}")


if __name__ == "__main__":
    main()
