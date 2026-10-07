#!/usr/bin/env python3
"""Rasterize pinned IBM Plex TTFs into deterministic A1 source JSON.

Regeneration requires Pillow; normal pack builds use only Python's standard library.
"""

from __future__ import annotations

import argparse
import json
from pathlib import Path

from PIL import Image, ImageDraw, ImageFont

from resource_pack_format import required_font_codepoints


FACES = (
    ("small", "IBMPlexSans-Regular.ttf", 12),
    ("medium", "IBMPlexSans-Regular.ttf", 16),
    ("large", "IBMPlexMono-Regular.ttf", 28),
)
ICON_CODEPOINTS = range(0xE001, 0xE009)


def icon_rows(codepoint: int, size: int) -> tuple[list[str], int]:
    side = max(10, min(24, size))
    image = Image.new("1", (side, side))
    draw = ImageDraw.Draw(image)
    m = side // 5
    c = side // 2
    if codepoint == 0xE001:  # calibration target
        draw.ellipse((m, m, side - m - 1, side - m - 1), outline=1)
        draw.line((c, m - 1, c, side - m), fill=1)
        draw.line((m - 1, c, side - m, c), fill=1)
    elif codepoint == 0xE002:  # display
        draw.rectangle((m, m, side - m - 1, side - m - 3), outline=1)
        draw.line((c, side - m - 2, c, side - m), fill=1)
    elif codepoint == 0xE003:  # sound
        draw.polygon(((m, c - 2), (m + 3, c - 2), (c + 1, m),
                      (c + 1, side - m), (m + 3, c + 2), (m, c + 2)), outline=1)
        draw.arc((c - 1, m, side - m, side - m), -70, 70, fill=1)
    elif codepoint == 0xE004:  # language
        draw.ellipse((m, m, side - m - 1, side - m - 1), outline=1)
        draw.line((c, m, c, side - m - 1), fill=1)
        draw.line((m, c, side - m - 1, c), fill=1)
    elif codepoint == 0xE005:  # diagnostics
        draw.line((m, c, c - 3, c, c - 1, m + 2, c + 2, side - m - 2,
                   c + 4, c, side - m - 1, c), fill=1, width=2)
    elif codepoint == 0xE006:  # maintenance
        draw.ellipse((m, m, c + 2, c + 2), outline=1)
        draw.line((c, c, side - m - 1, side - m - 1), fill=1, width=2)
    elif codepoint == 0xE007:  # information
        draw.ellipse((m, m, side - m - 1, side - m - 1), outline=1)
        draw.line((c, c - 1, c, side - m - 2), fill=1)
        draw.point((c, m + 2), fill=1)
    else:  # back arrow
        draw.line((side - m - 1, c, m, c), fill=1, width=2)
        draw.line((m, c, c - 1, m), fill=1, width=2)
        draw.line((m, c, c - 1, side - m - 1), fill=1, width=2)
    return ["".join("1" if image.getpixel((x, y)) else "." for x in range(side))
            for y in range(side)], side + 2


def rasterize(font_path: Path, size: int, codepoints: set[int]) -> dict:
    font = ImageFont.truetype(str(font_path), size)
    ascent, descent = font.getmetrics()
    glyphs = {}
    for codepoint in sorted(codepoints | set(ICON_CODEPOINTS)):
        key = f"U+{codepoint:04X}"
        if codepoint in ICON_CODEPOINTS:
            rows, advance = icon_rows(codepoint, size)
            glyphs[key] = {"rows": rows, "advance": advance, "bearing_x": 0,
                           "bearing_y": min(ascent, len(rows))}
            continue
        char = chr(codepoint)
        bbox = font.getbbox(char, anchor="ls")
        if bbox is None:
            raise ValueError(f"missing glyph {key} in {font_path}")
        left, top, right, bottom = bbox
        width, height = max(1, right - left), max(1, bottom - top)
        if width > 32 or height > 32:
            raise ValueError(f"{key} exceeds 32x32 at {size}px")
        image = Image.new("L", (width, height))
        ImageDraw.Draw(image).text((-left, -top), char, font=font, fill=255, anchor="ls")
        rows = ["".join("1" if image.getpixel((x, y)) >= 96 else "."
                        for x in range(width)) for y in range(height)]
        glyphs[key] = {"rows": rows, "advance": max(1, round(font.getlength(char))),
                       "bearing_x": left, "bearing_y": max(0, -top)}
    return {"family": font_path.stem, "pixel_size": size, "ascent": ascent,
            "descent": descent, "line_height": ascent + descent, "glyphs": glyphs}


def main() -> int:
    parser = argparse.ArgumentParser(description=__doc__)
    parser.add_argument("--manifest", type=Path, default=Path(__file__).resolve().parents[1] /
                        "assets/resource_manifest.json")
    args = parser.parse_args()
    root = args.manifest.parent
    codepoints = required_font_codepoints(args.manifest)
    source = root / "source/fonts"
    for role, filename, size in FACES:
        target = root / "font" / f"plex-{role}-a1.json"
        target.write_text(json.dumps(rasterize(source / filename, size, codepoints),
                                     ensure_ascii=False, separators=(",", ":")) + "\n",
                          encoding="utf-8")
        print(f"wrote {target}")
    return 0


if __name__ == "__main__":
    raise SystemExit(main())
