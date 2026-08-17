from __future__ import annotations

from pathlib import Path

import numpy as np
from PIL import Image

import build_the_weaver as pipeline


FRAME_SELECTIONS = {
    "idle": [
        2, 5, 9, 12, 16, 19, 22, 26, 29, 33, 36, 39,
        43, 46, 49, 53, 56, 60, 63, 66, 70, 73, 77, 80,
    ],
}


def camp_base_anchor(image: Image.Image) -> tuple[float, float]:
    """Use the source's fixed base coordinate, independent of leaf motion."""
    return image.width * 0.5, image.height * (365.0 / 378.0)


def clear_cell_edges(_animation: str, image: Image.Image) -> Image.Image:
    array = np.asarray(image.convert("RGBA")).copy()
    array[0] = 0
    array[-1] = 0
    array[:, 0] = 0
    array[:, -1] = 0
    return Image.fromarray(pipeline.zero_transparent_rgb(array), "RGBA")


def configure_pipeline() -> None:
    pipeline.SOURCE_STEM = "hiddenCamp"
    pipeline.OUTPUT_STEM = "hiddenCamp"
    pipeline.DISPLAY_NAME = "Hidden Camp"
    pipeline.REVIEW = (
        Path(r"C:\Users\jarox\OneDrive\Desktop\Tandem Tales\Bayou Bonanza")
        / "tools"
        / "hidden-camp-review"
    )
    # The supplied idle artwork currently uses the attack source suffix.
    pipeline.SOURCE_NAMES = {"idle": "attack"}
    pipeline.FRAME_SELECTIONS = FRAME_SELECTIONS
    pipeline.COMPONENT_MIN_AREAS = {"idle": 8}
    pipeline.FRAME_PREPROCESSOR = clear_cell_edges
    pipeline.FRAME_TRANSFORMER = pipeline.default_frame_transformer
    pipeline.animation_anchor_for = lambda _animation, image: camp_base_anchor(image)
    pipeline.SHARED_SCALE_OVERRIDE = None


if __name__ == "__main__":
    configure_pipeline()
    pipeline.main()
