from __future__ import annotations

from pathlib import Path

from PIL import Image

import build_the_choir_large as choir
import build_the_choir_med as medium
import build_the_weaver as pipeline


FRAME_SELECTIONS = {
    # Stop before the last row's bright, faded source frames.
    "walk": [
        15, 17, 20, 22, 24, 26, 29, 31, 33, 35, 38, 40,
        42, 44, 47, 49, 51, 53, 56, 58, 60, 62, 65, 67,
    ],
    # One complete spell build, flare, and return.
    "attack": [2, 6, 11, 15, 19, 24, 28, 33, 37, 41, 46, 50],
    # One complete recoil pulse and return to the original height.
    "damaged": [2, 6, 11, 15, 19, 24, 28, 33, 37, 41, 46, 50],
    # Collapse into the lowest pose; recovery begins later in the source.
    "killed": [2, 4, 7, 9, 11, 14, 16, 19, 21, 23, 26, 28],
}


def robe_anchor(image: Image.Image) -> tuple[float, float]:
    return choir.lower_robe_x(image), choir.lower_robe_y(image)


def animation_anchor_for(
    _animation: str, image: Image.Image
) -> tuple[float, float]:
    return robe_anchor(image)


def configure_pipeline() -> None:
    pipeline.SOURCE_STEM = "theChoir_small"
    pipeline.OUTPUT_STEM = "theChoir_small"
    pipeline.DISPLAY_NAME = "The Choir Small"
    pipeline.REVIEW = (
        Path(r"C:\Users\jarox\OneDrive\Desktop\Tandem Tales\Bayou Bonanza")
        / "tools"
        / "theChoir-small-review"
    )
    pipeline.FRAME_SELECTIONS = FRAME_SELECTIONS
    pipeline.COMPONENT_MIN_AREAS = {
        "walk": 4,
        "attack": 4,
        "damaged": 600,
        "killed": 600,
    }
    pipeline.FRAME_PREPROCESSOR = medium.clean_medium_particles
    pipeline.FRAME_TRANSFORMER = pipeline.default_frame_transformer
    pipeline.repair_sheet = choir.preserve_feather_edges
    pipeline.animation_anchor = robe_anchor
    pipeline.animation_anchor_for = animation_anchor_for
    pipeline.SHARED_SCALE_OVERRIDE = None


if __name__ == "__main__":
    configure_pipeline()
    pipeline.main()
