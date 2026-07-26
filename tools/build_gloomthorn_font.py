#!/usr/bin/env python3
"""Build the static Gloomthorn Display TTF from the OFL Cormorant source."""

from __future__ import annotations

import argparse
import unicodedata
from pathlib import Path

from fontTools.ttLib import TTFont
from fontTools.varLib.instancer import instantiateVariableFont


FAMILY = "Gloomthorn Display"
SUBFAMILY = "Regular"
FULL_NAME = f"{FAMILY} {SUBFAMILY}"
POSTSCRIPT_NAME = "GloomthornDisplay-Regular"
VERSION = "Version 1.000"
COPYRIGHT = (
    "Copyright 2015 The Cormorant Project Authors "
    "(github.com/CatharsisFonts/Cormorant). "
    "Modified in 2026 as Gloomthorn Display."
)
LICENSE = (
    "This Font Software is licensed under the SIL Open Font License, "
    "Version 1.1. This license is available with a FAQ at: "
    "https://openfontlicense.org"
)
LICENSE_URL = "https://openfontlicense.org"


def set_name(font: TTFont, name_id: int, value: str) -> None:
    name = font["name"]
    name.setName(value, name_id, 3, 1, 0x409)
    name.setName(value, name_id, 1, 0, 0)


def add_display_spacing(font: TTFont, units: int) -> None:
    """Add restrained tracking after letters and numerals.

    Keeping the glyph origin unchanged preserves composite placement and
    TrueType hints. The added advance becomes inter-letter space for all but
    the final glyph in a run.
    """

    cmap = font.getBestCmap()
    spaced_glyphs = {
        glyph_name
        for codepoint, glyph_name in cmap.items()
        if unicodedata.category(chr(codepoint))[0] in {"L", "N"}
    }
    metrics = font["hmtx"].metrics
    for glyph_name in spaced_glyphs:
        advance, left_side_bearing = metrics[glyph_name]
        metrics[glyph_name] = (advance + units, left_side_bearing)


def build(source: Path, output: Path, weight: float, spacing: int) -> None:
    font = TTFont(source, recalcBBoxes=True, recalcTimestamp=False)
    if "fvar" not in font:
        raise ValueError(f"{source} is not a variable font")

    axes = {axis.axisTag for axis in font["fvar"].axes}
    if "wght" not in axes:
        raise ValueError(f"{source} does not provide a wght axis")

    instantiateVariableFont(font, {"wght": weight}, inplace=True, optimize=True)
    add_display_spacing(font, spacing)

    set_name(font, 0, COPYRIGHT)
    set_name(font, 1, FAMILY)
    set_name(font, 2, SUBFAMILY)
    set_name(font, 3, "1.000;GLOOMTHORN;GloomthornDisplay-Regular")
    set_name(font, 4, FULL_NAME)
    set_name(font, 5, VERSION)
    set_name(font, 6, POSTSCRIPT_NAME)
    set_name(font, 9, "Gloomthorn Display derivative")
    set_name(font, 13, LICENSE)
    set_name(font, 14, LICENSE_URL)
    set_name(font, 16, FAMILY)
    set_name(font, 17, SUBFAMILY)
    set_name(font, 25, "GloomthornDisplay")

    font["head"].fontRevision = 1.0
    font["OS/2"].usWeightClass = 500
    font["OS/2"].fsSelection |= 1 << 6  # REGULAR
    font["OS/2"].fsSelection &= ~(1 << 0)  # not ITALIC
    font["OS/2"].fsSelection &= ~(1 << 5)  # not BOLD
    font["post"].italicAngle = 0

    output.parent.mkdir(parents=True, exist_ok=True)
    font.save(output, reorderTables=True)


def main() -> None:
    parser = argparse.ArgumentParser()
    parser.add_argument("source", type=Path, help="CormorantGaramond[wght].ttf")
    parser.add_argument("output", type=Path)
    parser.add_argument("--weight", type=float, default=500)
    parser.add_argument("--spacing", type=int, default=24)
    args = parser.parse_args()
    build(args.source, args.output, args.weight, args.spacing)


if __name__ == "__main__":
    main()
