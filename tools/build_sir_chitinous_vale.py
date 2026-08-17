from __future__ import annotations

from pathlib import Path

import numpy as np
from PIL import Image, ImageFilter

import build_the_weaver as pipeline


FRAME_SELECTIONS = {
    "walk": [
        2, 5, 9, 12, 16, 19, 22, 26, 29, 33, 36, 39,
        43, 46, 49, 53, 56, 60, 63, 66, 70, 73, 77, 80,
    ],
    # One complete turn, sword sweep, and controlled return to stance.
    "attack": [2, 7, 12, 17, 22, 27, 32, 37, 42, 47, 52, 57],
    # One planted recoil followed by the same compatible poses in reverse.
    "damaged": [19, 25, 31, 37, 43, 49, 55, 49, 43, 37, 31, 25],
    # End on the last fully settled prone frame, before head/arm recovery starts.
    "killed": [2, 6, 10, 14, 18, 22, 26, 30, 34, 38, 42, 45],
}


DAMAGED_FOOT_PAIRS = {
    19: ((72.0, 236.0), (204.0, 237.0)),
    25: ((72.0, 233.0), (204.0, 237.0)),
    31: ((72.0, 233.0), (204.0, 237.0)),
    37: ((74.0, 239.0), (203.0, 244.0)),
    43: ((72.0, 244.0), (202.0, 249.0)),
    49: ((70.0, 246.0), (202.0, 250.0)),
    55: ((71.0, 246.0), (204.0, 250.0)),
}
DAMAGED_TARGET_FEET = ((72.0, 239.0), (204.0, 244.0))
BASE_PREMULTIPLIED_RESIZE = pipeline.premultiplied_resize


def visible_bounds(mask: np.ndarray) -> tuple[int, int, int, int] | None:
    ys, xs = np.nonzero(mask)
    if len(xs) == 0:
        return None
    return int(xs.min()), int(ys.min()), int(xs.max()), int(ys.max())


def torso_x(image: Image.Image) -> float:
    """Track the armored thorax while ignoring the shield, sword, and cape."""
    alpha = np.asarray(image.convert("RGBA"))[..., 3]
    visible = alpha > 28
    box = visible_bounds(visible)
    if box is None:
        return image.width / 2.0
    left, top, right, bottom = box
    width = right - left + 1
    height = bottom - top + 1
    yy, xx = np.indices(visible.shape)
    solid = np.asarray(
        Image.fromarray(np.where(visible, 255, 0).astype(np.uint8), "L").filter(
            ImageFilter.MinFilter(9)
        )
    ) > 0
    core = solid & (
        (xx >= left + width * 0.18)
        & (xx <= right - width * 0.18)
        & (yy >= top + height * 0.12)
        & (yy <= top + height * 0.68)
    )
    _ys, xs = np.nonzero(core)
    if len(xs) >= 20:
        return float(np.median(xs))

    central = visible & (
        (xx >= left + width * 0.24)
        & (xx <= right - width * 0.24)
        & (yy <= top + height * 0.70)
    )
    _ys, xs = np.nonzero(central)
    return float(np.median(xs)) if len(xs) else (left + right) / 2.0


def support_foot(image: Image.Image) -> tuple[float, float]:
    """Find the lowest central support appendage without latching onto weapons."""
    alpha = np.asarray(image.convert("RGBA"))[..., 3]
    visible = alpha > 28
    box = visible_bounds(visible)
    if box is None:
        return image.width / 2.0, image.height / 2.0
    _left, top, _right, bottom = box
    torso = torso_x(image)
    height = bottom - top + 1
    yy, xx = np.indices(visible.shape)
    lower = visible & (
        (np.abs(xx - torso) <= max(16.0, height * 0.26))
        & (yy >= top + height * 0.52)
    )
    row_counts = np.count_nonzero(lower, axis=1)
    rows = np.flatnonzero(row_counts >= 3)
    foot_y = int(rows[-1]) if len(rows) else bottom
    foot_band = lower & (yy >= foot_y - 14) & (yy <= foot_y)
    _ys, xs = np.nonzero(foot_band)
    foot_x = float(np.median(xs)) if len(xs) else torso
    return foot_x, float(foot_y)


def substantial_bottom(image: Image.Image) -> float:
    alpha = np.asarray(image.convert("RGBA"))[..., 3]
    rows = np.flatnonzero(np.count_nonzero(alpha > 28, axis=1) >= 4)
    return float(rows[-1]) if len(rows) else image.height / 2.0


def animation_anchor_for(
    animation: str, image: Image.Image
) -> tuple[float, float]:
    damage_anchor = image.info.get("damaged_anchor")
    if animation == "damaged" and damage_anchor is not None:
        return (
            float(damage_anchor[0]) * image.width,
            float(damage_anchor[1]) * image.height,
        )
    if animation == "killed":
        return torso_x(image), substantial_bottom(image)
    foot_x, foot_y = support_foot(image)
    if animation == "walk":
        return torso_x(image), foot_y
    return foot_x, foot_y


def premultiplied_affine(
    image: Image.Image,
    source_feet: tuple[tuple[float, float], tuple[float, float]],
) -> Image.Image:
    """Normalize both support feet without straight-alpha interpolation."""
    (source_left_x, source_left_y), (source_right_x, source_right_y) = source_feet
    (target_left_x, target_left_y), (target_right_x, target_right_y) = (
        DAMAGED_TARGET_FEET
    )
    source_span = source_right_x - source_left_x
    scale_x = (target_right_x - target_left_x) / source_span
    translate_x = target_left_x - scale_x * source_left_x
    shear_y = (
        (target_right_y - target_left_y)
        - (source_right_y - source_left_y)
    ) / source_span
    translate_y = target_left_y - source_left_y - shear_y * source_left_x
    inverse = (
        1.0 / scale_x,
        0.0,
        -translate_x / scale_x,
        -shear_y / scale_x,
        1.0,
        shear_y * translate_x / scale_x - translate_y,
    )

    rgba = np.asarray(image.convert("RGBA"), dtype=np.float32)
    alpha = rgba[..., 3] / 255.0
    premultiplied = rgba[..., :3] * alpha[..., None]

    def transform(channel: np.ndarray) -> np.ndarray:
        source = Image.fromarray(channel.astype(np.float32), "F")
        result = source.transform(
            image.size,
            Image.Transform.AFFINE,
            inverse,
            resample=Image.Resampling.BICUBIC,
            fillcolor=0.0,
        )
        return np.asarray(result, dtype=np.float32)

    transformed_alpha = np.clip(transform(alpha), 0.0, 1.0)
    transformed_rgb = np.stack(
        [transform(premultiplied[..., channel]) for channel in range(3)],
        axis=2,
    )
    straight_rgb = np.zeros_like(transformed_rgb)
    visible = transformed_alpha > 1e-6
    straight_rgb[visible] = (
        transformed_rgb[visible] / transformed_alpha[visible, None]
    )
    output = np.empty_like(rgba, dtype=np.uint8)
    output[..., :3] = np.clip(np.rint(straight_rgb), 0, 255).astype(np.uint8)
    output[..., 3] = np.clip(
        np.rint(transformed_alpha * 255.0), 0, 255
    ).astype(np.uint8)
    output[output[..., 3] == 0, :3] = 0
    return Image.fromarray(output, "RGBA")


def tag_frame(
    animation: str, frame_number: int, image: Image.Image
) -> Image.Image:
    if animation != "damaged":
        return image
    image = premultiplied_affine(image, DAMAGED_FOOT_PAIRS[frame_number])
    left, right = DAMAGED_TARGET_FEET
    image.info["damaged_anchor"] = (
        ((left[0] + right[0]) * 0.5) / image.width,
        ((left[1] + right[1]) * 0.5) / image.height,
    )
    return image


def resize_with_metadata(
    image: Image.Image, size: tuple[int, int]
) -> Image.Image:
    resized = BASE_PREMULTIPLIED_RESIZE(image, size)
    resized.info.update(image.info)
    return resized


def clear_cell_edges(_animation: str, image: Image.Image) -> Image.Image:
    array = np.asarray(image.convert("RGBA")).copy()
    array[0] = 0
    array[-1] = 0
    array[:, 0] = 0
    array[:, -1] = 0
    return Image.fromarray(pipeline.zero_transparent_rgb(array), "RGBA")


def configure_pipeline() -> None:
    pipeline.SOURCE_STEM = "sirChitinousVale"
    pipeline.OUTPUT_STEM = "sirChitinousVale"
    pipeline.DISPLAY_NAME = "Sir Chitinous Vale"
    pipeline.REVIEW = (
        Path(r"C:\Users\jarox\OneDrive\Desktop\Tandem Tales\Bayou Bonanza")
        / "tools"
        / "sir-chitinous-vale-review"
    )
    pipeline.SOURCE_NAMES = {
        "walk": "walking",
        "attack": "attack",
        "damaged": "damaged",
        "killed": "killed",
    }
    pipeline.FRAME_SELECTIONS = FRAME_SELECTIONS
    pipeline.COMPONENT_MIN_AREAS = {
        "walk": 8,
        "attack": 8,
        "damaged": 8,
        "killed": 8,
    }
    pipeline.FRAME_PREPROCESSOR = clear_cell_edges
    pipeline.FRAME_TRANSFORMER = tag_frame
    pipeline.premultiplied_resize = resize_with_metadata
    pipeline.animation_anchor_for = animation_anchor_for
    pipeline.SHARED_SCALE_OVERRIDE = None


if __name__ == "__main__":
    configure_pipeline()
    pipeline.main()
