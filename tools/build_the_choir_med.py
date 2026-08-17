from __future__ import annotations

from pathlib import Path

import numpy as np
from PIL import Image, ImageFilter

import build_the_choir_large as choir
import build_the_weaver as pipeline
from animation_halo_cleanup import component_labels


FRAME_SELECTIONS = {
    # Stop before the final row's bright, faded frames.
    "walk": [
        2, 5, 8, 11, 14, 17, 20, 23, 26, 29, 32, 35,
        39, 42, 45, 48, 51, 54, 57, 60, 63, 66, 69, 72,
    ],
    # One complete spell build, flare, and return.
    "attack": [2, 6, 11, 15, 19, 24, 28, 33, 37, 41, 46, 50],
    # One complete bend, low recoil, and recovery.
    "damaged": [2, 7, 13, 18, 24, 29, 35, 40, 46, 51, 57, 62],
    # Collapse into the grounded hold; later source frames only prolong it.
    "killed": [11, 15, 18, 22, 25, 29, 32, 36, 39, 43, 46, 50],
}


def robe_anchor(image: Image.Image) -> tuple[float, float]:
    return choir.lower_robe_x(image), choir.lower_robe_y(image)


def animation_anchor_for(
    _animation: str, image: Image.Image
) -> tuple[float, float]:
    return robe_anchor(image)


def clean_medium_particles(animation: str, image: Image.Image) -> Image.Image:
    cleaned = choir.clean_robe_particles(animation, image)
    if animation not in {"damaged", "killed"}:
        return cleaned

    array = np.asarray(cleaned.convert("RGBA")).copy()
    visible = array[..., 3] > 72
    box = choir.mask_bounds(visible)
    if box is None:
        return cleaned
    _left, top, _right, bottom = box

    dense = np.asarray(
        Image.fromarray(np.where(visible, 255, 0).astype(np.uint8), "L").filter(
            ImageFilter.MinFilter(7)
        )
    ) > 0
    labels, sizes = component_labels(dense)
    if len(sizes) <= 1:
        return cleaned
    largest = max(range(1, len(sizes)), key=lambda label: sizes[label])
    core = labels == largest
    support = np.asarray(
        Image.fromarray(np.where(core, 255, 0).astype(np.uint8), "L").filter(
            ImageFilter.MaxFilter(31)
        )
    ) > 0

    yy, _xx = np.indices(visible.shape)
    lower_region = yy >= top + round((bottom - top + 1) * 0.55)
    array[lower_region & ~support] = 0
    return Image.fromarray(pipeline.zero_transparent_rgb(array), "RGBA")


def configure_pipeline() -> None:
    pipeline.SOURCE_STEM = "theChoir_med"
    pipeline.OUTPUT_STEM = "theChoir_med"
    pipeline.DISPLAY_NAME = "The Choir Medium"
    pipeline.REVIEW = (
        Path(r"C:\Users\jarox\OneDrive\Desktop\Tandem Tales\Bayou Bonanza")
        / "tools"
        / "theChoir-med-review"
    )
    pipeline.FRAME_SELECTIONS = FRAME_SELECTIONS
    pipeline.COMPONENT_MIN_AREAS = {
        "walk": 4,
        "attack": 4,
        "damaged": 1000,
        "killed": 1000,
    }
    pipeline.FRAME_PREPROCESSOR = clean_medium_particles
    pipeline.FRAME_TRANSFORMER = pipeline.default_frame_transformer
    pipeline.repair_sheet = choir.preserve_feather_edges
    pipeline.animation_anchor = robe_anchor
    pipeline.animation_anchor_for = animation_anchor_for
    pipeline.SHARED_SCALE_OVERRIDE = None


if __name__ == "__main__":
    configure_pipeline()
    pipeline.main()
