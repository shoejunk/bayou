from __future__ import annotations

from pathlib import Path

import numpy as np
from PIL import Image

import build_the_weaver as pipeline


FRAME_SELECTIONS = {
    "walk": [
        2, 5, 9, 12, 16, 19, 22, 26, 29, 33, 36, 39,
        43, 46, 49, 53, 56, 60, 63, 66, 70, 73, 77, 80,
    ],
    "attack": [2, 9, 16, 23, 30, 37, 45, 52, 59, 66, 73, 80],
    "damaged": [2, 10, 18, 26, 34, 42, 50, 58, 61, 64, 70, 80],
    # Recovery begins later in the source; frame 46 remains fully prone.
    "killed": [2, 6, 10, 14, 18, 22, 26, 30, 34, 38, 42, 46],
}


def animation_anchor_for(
    animation: str, image: Image.Image
) -> tuple[float, float]:
    if animation == "attack":
        # The spell occupies the right half of later frames. Keep the original
        # character axis fixed instead of allowing that effect to recenter him.
        return image.width * (184.0 / 366.0), pipeline.ground_y(image)
    return pipeline.abdomen_x(image), pipeline.ground_y(image)


def clear_cell_edges(_animation: str, image: Image.Image) -> Image.Image:
    array = np.asarray(image.convert("RGBA")).copy()
    array[0] = 0
    array[-1] = 0
    array[:, 0] = 0
    array[:, -1] = 0
    return Image.fromarray(pipeline.zero_transparent_rgb(array), "RGBA")


def configure_pipeline() -> None:
    pipeline.SOURCE_STEM = "lordPallidThread"
    pipeline.OUTPUT_STEM = "lordPallidThread"
    pipeline.DISPLAY_NAME = "Lord Pallid Thread"
    pipeline.REVIEW = (
        Path(r"C:\Users\jarox\OneDrive\Desktop\Tandem Tales\Bayou Bonanza")
        / "tools"
        / "lord-pallid-thread-review"
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
    pipeline.FRAME_TRANSFORMER = pipeline.default_frame_transformer
    pipeline.animation_anchor_for = animation_anchor_for
    pipeline.SHARED_SCALE_OVERRIDE = None


if __name__ == "__main__":
    configure_pipeline()
    pipeline.main()
