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


def fixed_center_anchor(image: Image.Image) -> tuple[float, float]:
    """Keep the rigid lantern body fixed while its crystals and glow animate."""
    return image.width * 0.5, image.height * 0.5


def clear_cell_edges(_animation: str, image: Image.Image) -> Image.Image:
    array = np.asarray(image.convert("RGBA")).copy()
    array[0] = 0
    array[-1] = 0
    array[:, 0] = 0
    array[:, -1] = 0
    return Image.fromarray(pipeline.zero_transparent_rgb(array), "RGBA")


def configure_pipeline() -> None:
    # The provided source filename omits the second "o" in echo.
    pipeline.SOURCE_STEM = "echLantern"
    pipeline.OUTPUT_STEM = "echoLantern"
    pipeline.DISPLAY_NAME = "Echo Lantern"
    pipeline.REVIEW = (
        Path(r"C:\Users\jarox\OneDrive\Desktop\Tandem Tales\Bayou Bonanza")
        / "tools"
        / "echo-lantern-review"
    )
    pipeline.SOURCE_NAMES = {"idle": "idle"}
    pipeline.FRAME_SELECTIONS = FRAME_SELECTIONS
    pipeline.COMPONENT_MIN_AREAS = {"idle": 6}
    pipeline.FRAME_PREPROCESSOR = clear_cell_edges
    pipeline.FRAME_TRANSFORMER = pipeline.default_frame_transformer
    pipeline.animation_anchor_for = lambda _animation, image: fixed_center_anchor(image)
    pipeline.SHARED_SCALE_OVERRIDE = None


if __name__ == "__main__":
    configure_pipeline()
    pipeline.main()
