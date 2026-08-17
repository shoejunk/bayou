from __future__ import annotations

from pathlib import Path

import numpy as np
from PIL import Image, ImageFilter

import build_the_weaver as pipeline
from animation_halo_cleanup import component_labels, suspicious_count


FRAME_SELECTIONS = {
    "walk": [
        7, 8, 10, 11, 12, 14, 15, 16, 18, 19, 20, 22,
        23, 25, 26, 27, 29, 30, 31, 33, 34, 35, 37, 38,
    ],
    # One complete spell build, flare, and return without the long idle tail.
    "attack": [2, 7, 13, 18, 24, 29, 35, 40, 46, 51, 57, 62],
    # One complete damaged pulse around the robe hem.
    "damaged": [2, 6, 9, 13, 17, 20, 24, 27, 31, 35, 38, 42],
    # Collapse through the deepest pose; recovery begins after frame 45.
    "killed": [2, 6, 10, 14, 18, 22, 26, 30, 32, 34, 36, 38],
}

DAMAGED_HEIGHT_SCALES = {
    frame: scale
    for frame, scale in zip(
        FRAME_SELECTIONS["damaged"],
        [1.00, 0.99, 0.98, 0.96, 0.94, 0.92, 0.91, 0.92, 0.94, 0.96, 0.98, 1.00],
        strict=True,
    )
}


def mask_bounds(mask: np.ndarray) -> tuple[int, int, int, int] | None:
    ys, xs = np.nonzero(mask)
    if len(xs) == 0:
        return None
    return int(xs.min()), int(ys.min()), int(xs.max()), int(ys.max())


def torso_x(image: Image.Image) -> float:
    """Follow the head and upper robe, not the swaying outer hem or spell."""
    array = np.asarray(image.convert("RGBA"))
    visible = array[..., 3] > 28
    box = mask_bounds(visible)
    if box is None:
        return image.width / 2.0
    left, top, right, bottom = box
    width = right - left + 1
    height = bottom - top + 1
    solid = np.asarray(
        Image.fromarray(np.where(visible, 255, 0).astype(np.uint8), "L").filter(
            ImageFilter.MinFilter(17)
        )
    ) > 0
    yy, xx = np.indices(solid.shape)
    core = solid & (
        (xx >= left + round(width * 0.14))
        & (xx <= right - round(width * 0.14))
        & (yy >= top + round(height * 0.02))
        & (yy <= top + round(height * 0.64))
    )
    _ys, xs = np.nonzero(core)
    if len(xs) >= 24:
        return float(np.median(xs))

    fallback = visible & (
        (xx >= left + round(width * 0.24))
        & (xx <= right - round(width * 0.24))
        & (yy <= top + round(height * 0.60))
    )
    _ys, xs = np.nonzero(fallback)
    if len(xs) >= 20:
        return float(np.median(xs))
    return (left + right) / 2.0


def hem_y(image: Image.Image) -> float:
    """Lock the true cleaned robe bottom so no feather fringe can dip or clip."""
    array = np.asarray(image.convert("RGBA"))
    visible = array[..., 3] > 28
    box = mask_bounds(visible)
    if box is None:
        return image.height / 2.0
    return float(box[3])


def lower_robe_y(image: Image.Image) -> float:
    """Track the solid central hem without following escaping insect fringe."""
    array = np.asarray(image.convert("RGBA"))
    visible = array[..., 3] > 72
    box = mask_bounds(visible)
    if box is None:
        return image.height / 2.0
    left, _top, right, _bottom = box
    center = torso_x(image)
    half_width = max(8, round((right - left + 1) * 0.12))
    band_left = max(0, round(center) - half_width)
    band_right = min(image.width, round(center) + half_width + 1)
    row_counts = np.count_nonzero(visible[:, band_left:band_right], axis=1)
    rows = np.flatnonzero(row_counts >= 3)
    return float(rows.max()) if len(rows) else hem_y(image)


def lower_robe_x(image: Image.Image) -> float:
    """Track the lower robe center when a bright spell obscures the torso."""
    array = np.asarray(image.convert("RGBA"))
    visible = array[..., 3] > 128
    box = mask_bounds(visible)
    if box is None:
        return image.width / 2.0
    _left, top, _right, bottom = box
    height = bottom - top + 1
    yy, _xx = np.indices(visible.shape)
    lower = visible & (
        (yy >= top + round(height * 0.52))
        & (yy <= top + round(height * 0.93))
    )
    _ys, xs = np.nonzero(lower)
    return float(np.median(xs)) if len(xs) else torso_x(image)


def animation_anchor(image: Image.Image) -> tuple[float, float]:
    return torso_x(image), hem_y(image)


def animation_anchor_for(
    animation: str, image: Image.Image
) -> tuple[float, float]:
    if animation == "damaged":
        return torso_x(image), lower_robe_y(image)
    if animation == "killed":
        return lower_robe_x(image), lower_robe_y(image)
    if animation == "attack":
        return lower_robe_x(image), hem_y(image)
    return animation_anchor(image)


def transform_frame(
    animation: str, frame_number: int, image: Image.Image
) -> Image.Image:
    if animation != "damaged":
        return image
    scale = DAMAGED_HEIGHT_SCALES[frame_number]
    if scale == 1.0:
        return image

    anchor_y = lower_robe_y(image)
    new_height = round(image.height * scale)
    resized = pipeline.premultiplied_resize(image, (image.width, new_height))
    resized_anchor_y = anchor_y * new_height / image.height
    offset_y = round(anchor_y - resized_anchor_y)
    canvas = Image.new("RGBA", image.size, (0, 0, 0, 0))
    canvas.alpha_composite(resized, (0, offset_y))
    return Image.fromarray(
        pipeline.zero_transparent_rgb(np.asarray(canvas)), "RGBA"
    )


def clean_robe_particles(animation: str, image: Image.Image) -> Image.Image:
    """Remove unsupported outer haze while retaining feather antialiasing."""
    if animation not in {"damaged", "killed"}:
        return image
    array = np.asarray(image.convert("RGBA")).copy()
    labels, sizes = component_labels(array[..., 3] > 72)
    if len(sizes) <= 1:
        return image
    largest = max(range(1, len(sizes)), key=lambda label: sizes[label])
    core = labels == largest
    support = np.asarray(
        Image.fromarray(np.where(core, 255, 0).astype(np.uint8), "L").filter(
            ImageFilter.MaxFilter(3)
        )
    ) > 0
    array[~support] = 0
    return Image.fromarray(pipeline.zero_transparent_rgb(array), "RGBA")


def preserve_feather_edges(
    image: Image.Image, frame_count: int
) -> tuple[Image.Image, dict[str, int]]:
    """Zero hidden RGB without recoloring legitimate pale feather boundaries."""
    original = np.asarray(image.convert("RGBA"))
    cleaned = pipeline.zero_transparent_rgb(original)
    suspicious = sum(
        suspicious_count(cleaned[:, index * pipeline.CELL : (index + 1) * pipeline.CELL])
        for index in range(frame_count)
    )
    changed = int(np.count_nonzero(np.any(cleaned != original, axis=2)))
    return Image.fromarray(cleaned, "RGBA"), {
        "pixels_recolored": changed,
        "suspicious_before": suspicious,
        "suspicious_after": suspicious,
    }


def configure_pipeline() -> None:
    pipeline.SOURCE_STEM = "theChoir_large"
    pipeline.OUTPUT_STEM = "theChoir_large"
    pipeline.DISPLAY_NAME = "The Choir Large"
    pipeline.SHARED_SCALE_OVERRIDE = 0.5125
    pipeline.REVIEW = (
        Path(r"C:\Users\jarox\OneDrive\Desktop\Tandem Tales\Bayou Bonanza")
        / "tools"
        / "theChoir-large-review"
    )
    pipeline.FRAME_SELECTIONS = FRAME_SELECTIONS
    pipeline.COMPONENT_MIN_AREAS = {
        "walk": 4,
        "attack": 4,
        "damaged": 1000,
        "killed": 1000,
    }
    pipeline.FRAME_PREPROCESSOR = clean_robe_particles
    pipeline.FRAME_TRANSFORMER = transform_frame
    pipeline.repair_sheet = preserve_feather_edges
    pipeline.animation_anchor = animation_anchor
    pipeline.animation_anchor_for = animation_anchor_for


if __name__ == "__main__":
    configure_pipeline()
    pipeline.main()
