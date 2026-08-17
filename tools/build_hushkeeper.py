from __future__ import annotations

from pathlib import Path

import numpy as np
from PIL import Image

import build_the_weaver as pipeline
from animation_halo_cleanup import suspicious_count
from build_glimmer_stag import (
    repair_extreme_white_fringe,
    zero_transparent_rgb,
)


FRAME_SELECTIONS = {
    # One complete wing cycle from the source's idle sheet.
    "walk": [
        2, 5, 9, 12, 16, 19, 22, 26, 29, 33, 36, 39,
        43, 46, 49, 53, 56, 60, 63, 66, 70, 73, 77, 80,
    ],
    # Clean later cast; omit the source's broken 52-55 and 63-65 intervals.
    "attack": [46, 48, 49, 50, 51, 56, 57, 58, 59, 61, 66, 70],
    # Full recoil/spin and recovery from the updated source.
    "damaged": [2, 9, 16, 23, 30, 37, 45, 52, 59, 66, 73, 80],
    # End on the fully down pose before the later head and wing lift begins.
    "killed": [2, 5, 8, 10, 13, 16, 19, 22, 25, 27, 30, 33],
}


def weighted_median(values: np.ndarray, weights: np.ndarray) -> float:
    order = np.argsort(values)
    ordered_values = values[order]
    ordered_weights = weights[order]
    midpoint = float(ordered_weights.sum()) * 0.5
    index = int(np.searchsorted(np.cumsum(ordered_weights), midpoint))
    return float(ordered_values[min(index, len(ordered_values) - 1)])


def body_core(image: Image.Image) -> tuple[float, float]:
    """Track the dark, colored body without letting pale wings pull the anchor."""
    array = np.asarray(image.convert("RGBA"), dtype=np.float32)
    alpha = array[..., 3] / 255.0
    rgb = array[..., :3]
    lightness = (
        rgb[..., 0] * 0.2126 + rgb[..., 1] * 0.7152 + rgb[..., 2] * 0.0722
    )
    chroma = rgb.max(axis=2) - rgb.min(axis=2)
    visible = alpha > 0.09
    body_emphasis = np.clip((205.0 - lightness) / 125.0, 0.08, 1.0)
    color_emphasis = 0.35 + np.clip(chroma / 90.0, 0.0, 1.0)
    weights = alpha * alpha * body_emphasis * color_emphasis
    weights[~visible] = 0.0
    # Bright source magic should not pull the body anchor away from the thorax.
    magic_color = (
        ((rgb[..., 2] > rgb[..., 0] + 28) & (rgb[..., 1] > rgb[..., 0] + 8))
        | ((rgb[..., 0] > rgb[..., 1] + 35) & (rgb[..., 2] > rgb[..., 1] + 18))
        | ((lightness > 220) & (chroma < 28))
    )
    weights[magic_color] = 0.0
    ys, xs = np.nonzero(weights > 0.015)
    if len(xs) < 12:
        ys, xs = np.nonzero(visible)
        if len(xs) == 0:
            return image.width / 2.0, image.height / 2.0
        return float(np.median(xs)), float(np.median(ys))
    selected = weights[ys, xs]
    return weighted_median(xs, selected), weighted_median(ys, selected)


def substantial_bottom(image: Image.Image) -> float:
    alpha = np.asarray(image.convert("RGBA"))[..., 3]
    visible = alpha > 24
    rows = np.flatnonzero(np.count_nonzero(visible, axis=1) >= 3)
    if len(rows):
        return float(rows[-1])
    ys, _xs = np.nonzero(visible)
    return float(ys[-1]) if len(ys) else image.height / 2.0


def animation_anchor_for(
    animation: str, image: Image.Image
) -> tuple[float, float]:
    core_x, core_y = body_core(image)
    if animation == "killed":
        return core_x, substantial_bottom(image)
    return core_x, core_y


def repair_wing_safe(
    image: Image.Image, frame_count: int
) -> tuple[Image.Image, dict[str, int]]:
    """Repair only extreme fringe without recoloring translucent wing texture."""
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
    pipeline.SOURCE_STEM = "hushkeeper"
    pipeline.OUTPUT_STEM = "hushkeeper"
    pipeline.DISPLAY_NAME = "Hushkeeper"
    pipeline.REVIEW = (
        Path(r"C:\Users\jarox\OneDrive\Desktop\Tandem Tales\Bayou Bonanza")
        / "tools"
        / "hushkeeper-review"
    )
    pipeline.SOURCE_NAMES = {
        "walk": "idle",
        "attack": "attack",
        "damaged": "damaged",
        "killed": "killed",
    }
    pipeline.FRAME_SELECTIONS = FRAME_SELECTIONS
    pipeline.COMPONENT_MIN_AREAS = {
        "walk": 5,
        "attack": 3,
        "damaged": 5,
        "killed": 5,
    }
    pipeline.FRAME_PREPROCESSOR = pipeline.default_frame_preprocessor
    pipeline.FRAME_TRANSFORMER = pipeline.default_frame_transformer
    pipeline.repair_sheet = repair_wing_safe
    pipeline.animation_anchor = body_core
    pipeline.animation_anchor_for = animation_anchor_for
    pipeline.SHARED_SCALE_OVERRIDE = None


if __name__ == "__main__":
    configure_pipeline()
    pipeline.main()
