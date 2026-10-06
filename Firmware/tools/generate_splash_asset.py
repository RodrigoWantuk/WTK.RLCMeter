#!/usr/bin/env python3
"""Generate the deterministic PRODUCT splash image resource."""

from __future__ import annotations

import argparse
import struct
import json
import math
from pathlib import Path
import zlib


WIDTH = 64
HEIGHT = 32

PALETTE = {
    "bg0": (4, 10, 19),
    "bg1": (5, 18, 27),
    "panel": (7, 24, 32),
    "panel2": (9, 34, 40),
    "cyan": (50, 224, 224),
    "cyan_dim": (20, 104, 116),
    "green": (132, 244, 92),
    "green_dim": (47, 126, 65),
    "white": (226, 252, 245),
    "amber": (255, 191, 91),
    "shadow": (0, 5, 10),
}

FONT_5X7 = {
    "C": ("01110", "10001", "10000", "10000", "10000", "10001", "01110"),
    "K": ("10001", "10010", "10100", "11000", "10100", "10010", "10001"),
    "L": ("10000", "10000", "10000", "10000", "10000", "10000", "11111"),
    "R": ("11110", "10001", "10001", "11110", "10100", "10010", "10001"),
    "T": ("11111", "00100", "00100", "00100", "00100", "00100", "00100"),
    "W": ("10001", "10001", "10001", "10101", "10101", "11011", "10001"),
}


def rgb565(color: tuple[int, int, int]) -> int:
    r, g, b = color
    return ((r >> 3) << 11) | ((g >> 2) << 5) | (b >> 3)


def blend(a: tuple[int, int, int], b: tuple[int, int, int], amount: float) -> tuple[int, int, int]:
    return tuple(int(round((ca * (1.0 - amount)) + (cb * amount))) for ca, cb in zip(a, b))


def set_px(canvas: list[list[tuple[int, int, int]]], x: int, y: int, color: tuple[int, int, int]) -> None:
    if 0 <= x < WIDTH and 0 <= y < HEIGHT:
        canvas[y][x] = color


def line(
    canvas: list[list[tuple[int, int, int]]],
    x0: int,
    y0: int,
    x1: int,
    y1: int,
    color: tuple[int, int, int],
) -> None:
    dx = abs(x1 - x0)
    sx = 1 if x0 < x1 else -1
    dy = -abs(y1 - y0)
    sy = 1 if y0 < y1 else -1
    err = dx + dy
    while True:
        set_px(canvas, x0, y0, color)
        if x0 == x1 and y0 == y1:
            break
        e2 = 2 * err
        if e2 >= dy:
            err += dy
            x0 += sx
        if e2 <= dx:
            err += dx
            y0 += sy


def rect(
    canvas: list[list[tuple[int, int, int]]],
    x: int,
    y: int,
    w: int,
    h: int,
    color: tuple[int, int, int],
) -> None:
    for yy in range(y, y + h):
        for xx in range(x, x + w):
            set_px(canvas, xx, yy, color)


def draw_text(
    canvas: list[list[tuple[int, int, int]]],
    text: str,
    x: int,
    y: int,
    color: tuple[int, int, int],
    scale: int = 1,
) -> None:
    cursor = x
    for ch in text:
        glyph = FONT_5X7[ch]
        for gy, row in enumerate(glyph):
            for gx, bit in enumerate(row):
                if bit == "1":
                    rect(canvas, cursor + (gx * scale), y + (gy * scale), scale, scale, color)
        cursor += (6 * scale)


def draw_resistor(canvas: list[list[tuple[int, int, int]]]) -> None:
    y = 15
    color = PALETTE["amber"]
    dim = PALETTE["green_dim"]
    line(canvas, 36, y, 39, y, dim)
    points = [(39, y), (41, 13), (43, 17), (45, 13), (47, 17), (49, 13), (51, 17), (53, y)]
    for (x0, y0), (x1, y1) in zip(points, points[1:]):
        line(canvas, x0, y0, x1, y1, color)
    line(canvas, 53, y, 59, y, dim)
    set_px(canvas, 35, y, PALETTE["cyan"])
    set_px(canvas, 60, y, PALETTE["cyan"])


def build_canvas() -> list[list[tuple[int, int, int]]]:
    canvas: list[list[tuple[int, int, int]]] = []
    for y in range(HEIGHT):
        band = y / (HEIGHT - 1)
        base = blend(PALETTE["bg0"], PALETTE["bg1"], band)
        canvas.append([base for _ in range(WIDTH)])

    rect(canvas, 1, 1, 62, 30, PALETTE["shadow"])
    rect(canvas, 2, 2, 60, 28, PALETTE["panel"])
    rect(canvas, 3, 3, 58, 3, PALETTE["panel2"])
    line(canvas, 3, 29, 60, 29, PALETTE["cyan_dim"])
    line(canvas, 3, 2, 60, 2, PALETTE["cyan"])
    line(canvas, 2, 4, 2, 28, PALETTE["cyan_dim"])
    line(canvas, 61, 4, 61, 28, PALETTE["green_dim"])

    draw_text(canvas, "WTK", 5, 6, PALETTE["white"], 2)
    draw_text(canvas, "RLC", 43, 5, PALETTE["cyan"], 1)
    draw_resistor(canvas)

    previous: tuple[int, int] | None = None
    for x in range(4, 61):
        phase = ((x - 4) / 57.0) * math.tau * 1.55
        y = int(round(23 + (math.sin(phase) * 4)))
        if previous is not None:
            line(canvas, previous[0], previous[1], x, y, PALETTE["green"])
        previous = (x, y)

    for x in (7, 24, 41, 58):
        set_px(canvas, x, 25, PALETTE["cyan"])
        set_px(canvas, x, 26, PALETTE["cyan_dim"])

    return canvas


def rle_encode(canvas: list[list[tuple[int, int, int]]]) -> list[list[object]]:
    encoded: list[list[object]] = []
    last: int | None = None
    count = 0
    for row in canvas:
        for color in row:
            value = rgb565(color)
            if last is None:
                last = value
                count = 1
            elif value == last and count < 0xFFFF:
                count += 1
            else:
                encoded.append([count, f"0x{last:04X}"])
                last = value
                count = 1
    if last is not None:
        encoded.append([count, f"0x{last:04X}"])
    return encoded


def write_preview_ppm(canvas: list[list[tuple[int, int, int]]], path: Path) -> None:
    path.parent.mkdir(parents=True, exist_ok=True)
    with path.open("wb") as stream:
        stream.write(f"P6\n{WIDTH} {HEIGHT}\n255\n".encode("ascii"))
        for row in canvas:
            for r, g, b in row:
                stream.write(bytes((r, g, b)))


def _png_chunk(kind: bytes, payload: bytes) -> bytes:
    return (
        struct.pack(">I", len(payload))
        + kind
        + payload
        + struct.pack(">I", zlib.crc32(kind + payload) & 0xFFFFFFFF)
    )


def write_preview_png(canvas: list[list[tuple[int, int, int]]], path: Path) -> None:
    path.parent.mkdir(parents=True, exist_ok=True)
    rows = bytearray()
    for row in canvas:
        rows.append(0)
        for r, g, b in row:
            rows.extend((r, g, b))
    ihdr = struct.pack(">IIBBBBB", WIDTH, HEIGHT, 8, 2, 0, 0, 0)
    png = (
        b"\x89PNG\r\n\x1a\n"
        + _png_chunk(b"IHDR", ihdr)
        + _png_chunk(b"IDAT", zlib.compress(bytes(rows), level=9))
        + _png_chunk(b"IEND", b"")
    )
    path.write_bytes(png)


def write_preview(canvas: list[list[tuple[int, int, int]]], path: Path) -> None:
    if path.suffix.lower() == ".png":
        write_preview_png(canvas, path)
    else:
        write_preview_ppm(canvas, path)


def main() -> int:
    parser = argparse.ArgumentParser(description=__doc__)
    parser.add_argument(
        "--output",
        type=Path,
        default=Path(__file__).resolve().parents[1] / "assets" / "image" / "wtk-splash-rle.json",
    )
    parser.add_argument("--preview", type=Path, help="optional PPM preview output")
    args = parser.parse_args()

    canvas = build_canvas()
    data = {
        "format": "IMAGE_RGB565_RLE_V1",
        "width": WIDTH,
        "height": HEIGHT,
        "description": "WTK.RLCMeter external PRODUCT splash, deterministic RGB565 RLE source.",
        "runs": rle_encode(canvas),
    }
    args.output.parent.mkdir(parents=True, exist_ok=True)
    args.output.write_text(json.dumps(data, indent=2) + "\n", encoding="utf-8")
    if args.preview is not None:
        write_preview(canvas, args.preview)
    print(f"wrote {args.output} runs={len(data['runs'])}")
    return 0


if __name__ == "__main__":
    raise SystemExit(main())
