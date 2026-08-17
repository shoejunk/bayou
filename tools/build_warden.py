from __future__ import annotations

from pathlib import Path

import numpy as np
from PIL import Image

import build_the_weaver as pipeline
from animation_halo_cleanup import suspicious_count
from build_glimmer_stag import repair_extreme_white_fringe, zero_transparent_rgb


FRAME_SELECTIONS = {
    "walk": [
        2, 5, 9, 12, 16, 19, 22, 26, 29, 33, 36, 39,
        43, 46, 49, 53, 56, 60, 63, 66, 70, 73, 77, 80,
    ],
    # One complete sword raise, overhead arc, and return.
    "attack": [2, 7, 11, 16, 20, 25, 29, 34, 39, 43, 48, 52],
    # Begin and end on the exact original stance around one mirrored recoil.
    "damaged": [2, 4, 7, 10, 13, 16, 19, 16, 13, 10, 6, 2],
    # Stop before the subtle late head and sword lift in the downed hold.
    "killed": [2, 5, 7, 10, 12, 15, 17, 20, 22, 25, 27, 30],
}


# Source-cell landmarks for the same higher, rear boot in every selected pose.
UPPER_FOOT_ANCHORS = {
    "attack": {
        2: (155, 260),
        7: (153, 258),
        11: (155, 258),
        16: (158, 260),
        20: (145, 266),
        25: (150, 270),
        29: (147, 238),
        34: (166, 234),
        39: (165, 235),
        43: (166, 242),
        48: (173, 258),
        52: (166, 256),
    },
}

DAMAGED_FOOT_PAIRS = {
    2: ((88.0, 286.0), (166.0, 231.0)),
    4: ((82.0, 286.0), (177.0, 231.0)),
    6: ((76.0, 287.0), (185.0, 232.0)),
    7: ((73.0, 286.0), (190.0, 232.0)),
    10: ((62.0, 280.0), (199.0, 238.0)),
    13: ((61.0, 278.0), (204.0, 242.0)),
    16: ((60.0, 272.0), (207.0, 247.0)),
    19: ((56.0, 267.0), (207.0, 249.0)),
}
DAMAGED_TARGET_FEET = DAMAGED_FOOT_PAIRS[2]

# Two restrained one-pixel rises and falls across the 24-frame stride.
WALK_BOB = (0, -1, -1, -1, -1, 0, 0, 1, 1, 1, 1, 0) * 2
BASE_PREMULTIPLIED_RESIZE = pipeline.premultiplied_resize


def visible_bounds(mask: np.ndarray) -> tuple[int, int, int, int] | None:
    ys, xs = np.nonzero(mask)
    if len(xs) == 0:
        return None
    return int(xs.min()), int(ys.min()), int(xs.max()), int(ys.max())


def torso_anchor(image: Image.Image) -> tuple[float, float]:
    """Track the central armor, excluding the moving shield and sword."""
    alpha = np.asarray(image.convert("RGBA"))[..., 3]
    visible = alpha > 28
    box = visible_bounds(visible)
    if box is None:
        return image.width / 2.0, image.height / 2.0
    _left, top, _right, bottom = box
    yy, xx = np.indices(visible.shape)
    central = visible & (
        (xx >= image.width * 0.28)
        & (xx <= image.width * 0.72)
        & (yy >= top + (bottom - top) * 0.16)
        & (yy <= top + (bottom - top) * 0.72)
    )
    ys, xs = np.nonzero(central)
    if len(xs) >= 30:
        weights = alpha[ys, xs].astype(np.float64) ** 2
        def weighted_median(values: np.ndarray) -> float:
            order = np.argsort(values)
            ordered = values[order]
            ordered_weights = weights[order]
            midpoint = ordered_weights.sum() * 0.5
            index = int(np.searchsorted(np.cumsum(ordered_weights), midpoint))
            return float(ordered[min(index, len(ordered) - 1)])

        return weighted_median(xs), weighted_median(ys)
    ys, xs = np.nonzero(visible)
    return float(np.median(xs)), float(np.median(ys))


def torso_x(image: Image.Image) -> float:
    return torso_anchor(image)[0]


def foot_anchor(image: Image.Image) -> tuple[float, float]:
    """Find the lowest substantial foot close to the torso, not the sword tip."""
    alpha = np.asarray(image.convert("RGBA"))[..., 3]
    visible = alpha > 28
    box = visible_bounds(visible)
    if box is None:
        return image.width / 2.0, image.height / 2.0
    torso = torso_x(image)
    _left, top, _right, bottom = box
    half_width = max(18.0, (bottom - top + 1) * 0.22)
    yy, xx = np.indices(visible.shape)
    lower_body = visible & (
        (np.abs(xx - torso) <= half_width)
        & (yy >= top + (bottom - top) * 0.52)
    )
    row_counts = np.count_nonzero(lower_body, axis=1)
    rows = np.flatnonzero(row_counts >= 3)
    foot_y = int(rows[-1]) if len(rows) else bottom
    foot_band = lower_body & (yy >= foot_y - 14) & (yy <= foot_y)
    _ys, xs = np.nonzero(foot_band)
    foot_x = float(np.median(xs)) if len(xs) else torso
    return foot_x, float(foot_y)


def upper_stationary_foot_anchor(image: Image.Image) -> tuple[float, float]:
    """Track the raised planted foot while excluding the lower foot and weapons."""
    alpha = np.asarray(image.convert("RGBA"))[..., 3]
    visible = alpha > 28
    box = visible_bounds(visible)
    if box is None:
        return image.width / 2.0, image.height / 2.0
    torso = torso_x(image)
    _left, top, _right, bottom = box
    height = bottom - top + 1
    yy, xx = np.indices(visible.shape)
    foot_region = visible & (
        (xx >= torso - height * 0.02)
        & (xx <= torso + height * 0.23)
        & (yy >= top + height * 0.46)
        & (yy <= top + height * 0.88)
    )
    row_counts = np.count_nonzero(foot_region, axis=1)
    rows = np.flatnonzero(row_counts >= 3)
    if not len(rows):
        return foot_anchor(image)
    foot_y = int(rows[-1])
    foot_band = foot_region & (yy >= foot_y - 10) & (yy <= foot_y)
    _ys, xs = np.nonzero(foot_band)
    foot_x = float(np.median(xs)) if len(xs) else torso
    return foot_x, float(foot_y)


def substantial_bottom(image: Image.Image) -> float:
    alpha = np.asarray(image.convert("RGBA"))[..., 3]
    rows = np.flatnonzero(np.count_nonzero(alpha > 28, axis=1) >= 4)
    return float(rows[-1]) if len(rows) else image.height / 2.0


def substantial_top(image: Image.Image) -> float:
    alpha = np.asarray(image.convert("RGBA"))[..., 3]
    rows = np.flatnonzero(np.count_nonzero(alpha > 28, axis=1) >= 4)
    return float(rows[0]) if len(rows) else image.height / 2.0


def animation_anchor_for(
    animation: str, image: Image.Image
) -> tuple[float, float]:
    damage_anchor = image.info.get("damaged_anchor")
    if animation == "damaged" and damage_anchor is not None:
        return (
            float(damage_anchor[0]) * image.width,
            float(damage_anchor[1]) * image.height,
        )
    manual_foot = image.info.get("upper_foot_anchor")
    if animation == "attack" and manual_foot is not None:
        foot_y = float(manual_foot[1]) * image.height
        return (
            float(manual_foot[0]) * image.width,
            foot_y,
        )
    if animation == "killed":
        return torso_x(image), substantial_bottom(image)
    if animation == "walk":
        bob = float(image.info.get("walk_bob", 0))
        return torso_x(image), substantial_top(image) - bob
    if animation == "attack":
        return upper_stationary_foot_anchor(image)
    return torso_anchor(image)


def premultiplied_affine(
    image: Image.Image,
    source_feet: tuple[tuple[float, float], tuple[float, float]],
) -> Image.Image:
    """Map both feet to fixed targets without straight-alpha interpolation."""
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
    if animation == "damaged":
        left, right = DAMAGED_FOOT_PAIRS[frame_number]
        image.info["damaged_anchor"] = (
            ((left[0] + right[0]) * 0.5) / image.width,
            ((left[1] + right[1]) * 0.5) / image.height,
        )
    image.info["source_frame"] = frame_number
    foot = UPPER_FOOT_ANCHORS.get(animation, {}).get(frame_number)
    if foot is not None:
        image.info["upper_foot_anchor"] = (
            foot[0] / image.width,
            foot[1] / image.height,
        )
    if animation == "walk":
        index = FRAME_SELECTIONS["walk"].index(frame_number)
        image.info["walk_bob"] = WALK_BOB[index]
    return image


def resize_with_metadata(
    image: Image.Image, size: tuple[int, int]
) -> Image.Image:
    resized = BASE_PREMULTIPLIED_RESIZE(image, size)
    resized.info.update(image.info)
    return resized


def repair_pale_armor(
    image: Image.Image, frame_count: int
) -> tuple[Image.Image, dict[str, int]]:
    """Repair extreme white contamination without recoloring pale armor edges."""
    source = zero_transparent_rgb(np.asarray(image.convert("RGBA")))
    suspicious_before = sum(
        suspicious_count(source[:, index * pipeline.CELL : (index + 1) * pipeline.CELL])
        for index in range(frame_count)
    )
    repaired, changed = repair_extreme_white_fringe(
        Image.fromarray(source, "RGBA"), frame_count
    )
    output = zero_transparent_rgb(np.asarray(repaired.convert("RGBA")))
    suspicious_after = sum(
        suspicious_count(output[:, index * pipeline.CELL : (index + 1) * pipeline.CELL])
        for index in range(frame_count)
    )
    return Image.fromarray(output, "RGBA"), {
        "pixels_recolored": changed,
        "suspicious_before": suspicious_before,
        "suspicious_after": suspicious_after,
    }


def configure_pipeline() -> None:
    pipeline.SOURCE_STEM = "warden"
    pipeline.OUTPUT_STEM = "warden"
    pipeline.DISPLAY_NAME = "Warden"
    pipeline.REVIEW = (
        Path(r"C:\Users\jarox\OneDrive\Desktop\Tandem Tales\Bayou Bonanza")
        / "tools"
        / "warden-review"
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
    pipeline.FRAME_PREPROCESSOR = pipeline.default_frame_preprocessor
    pipeline.FRAME_TRANSFORMER = tag_frame
    pipeline.premultiplied_resize = resize_with_metadata
    pipeline.repair_sheet = repair_pale_armor
    pipeline.animation_anchor = torso_anchor
    pipeline.animation_anchor_for = animation_anchor_for
    pipeline.SHARED_SCALE_OVERRIDE = None


if __name__ == "__main__":
    configure_pipeline()
    pipeline.main()
