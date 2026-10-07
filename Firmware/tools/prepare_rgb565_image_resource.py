#!/usr/bin/env python3
"""Convert generated PNG artwork into deterministic RGB565 RLE image resources."""

from __future__ import annotations

import argparse
import json
from pathlib import Path
import struct
import zlib


def _paeth(a: int, b: int, c: int) -> int:
    p = a + b - c
    pa = abs(p - a)
    pb = abs(p - b)
    pc = abs(p - c)
    if pa <= pb and pa <= pc:
        return a
    if pb <= pc:
        return b
    return c


def read_png_rgb(path: Path) -> tuple[int, int, list[tuple[int, int, int]]]:
    data = path.read_bytes()
    if data[:8] != b"\x89PNG\r\n\x1a\n":
        raise ValueError(f"{path} is not a PNG")
    pos = 8
    width = height = bit_depth = color_type = interlace = None
    payload = bytearray()
    while pos < len(data):
        if pos + 8 > len(data):
            raise ValueError("truncated PNG chunk")
        size = struct.unpack(">I", data[pos : pos + 4])[0]
        kind = data[pos + 4 : pos + 8]
        chunk = data[pos + 8 : pos + 8 + size]
        pos += 12 + size
        if kind == b"IHDR":
            width, height, bit_depth, color_type, _, _, interlace = struct.unpack(">IIBBBBB", chunk)
        elif kind == b"IDAT":
            payload.extend(chunk)
        elif kind == b"IEND":
            break
    if None in (width, height, bit_depth, color_type, interlace):
        raise ValueError("missing PNG IHDR")
    if bit_depth != 8 or color_type not in (2, 6) or interlace != 0:
        raise ValueError("only non-interlaced 8-bit RGB/RGBA PNG input is supported")

    channels = 3 if color_type == 2 else 4
    stride = int(width) * channels
    raw = zlib.decompress(bytes(payload))
    rows: list[bytes] = []
    previous = bytearray(stride)
    src = 0
    bpp = channels
    for _ in range(int(height)):
        filt = raw[src]
        src += 1
        scan = bytearray(raw[src : src + stride])
        src += stride
        for i in range(stride):
            left = scan[i - bpp] if i >= bpp else 0
            up = previous[i]
            up_left = previous[i - bpp] if i >= bpp else 0
            if filt == 1:
                scan[i] = (scan[i] + left) & 0xFF
            elif filt == 2:
                scan[i] = (scan[i] + up) & 0xFF
            elif filt == 3:
                scan[i] = (scan[i] + ((left + up) >> 1)) & 0xFF
            elif filt == 4:
                scan[i] = (scan[i] + _paeth(left, up, up_left)) & 0xFF
            elif filt != 0:
                raise ValueError(f"unsupported PNG filter {filt}")
        rows.append(bytes(scan))
        previous = scan

    pixels: list[tuple[int, int, int]] = []
    for row in rows:
        for x in range(int(width)):
            offset = x * channels
            r, g, b = row[offset], row[offset + 1], row[offset + 2]
            if channels == 4:
                alpha = row[offset + 3]
                r = ((r * alpha) + (4 * (255 - alpha))) // 255
                g = ((g * alpha) + (10 * (255 - alpha))) // 255
                b = ((b * alpha) + (19 * (255 - alpha))) // 255
            pixels.append((r, g, b))
    return int(width), int(height), pixels


def resize_cover(
    src_w: int,
    src_h: int,
    pixels: list[tuple[int, int, int]],
    dst_w: int,
    dst_h: int,
) -> list[tuple[int, int, int]]:
    scale = max(dst_w / src_w, dst_h / src_h)
    sample_w = dst_w / scale
    sample_h = dst_h / scale
    x0 = (src_w - sample_w) * 0.5
    y0 = (src_h - sample_h) * 0.5
    out: list[tuple[int, int, int]] = []
    for y in range(dst_h):
        sy = y0 + ((y + 0.5) / scale) - 0.5
        y_base = max(0, min(src_h - 2, int(sy)))
        fy = sy - y_base
        for x in range(dst_w):
            sx = x0 + ((x + 0.5) / scale) - 0.5
            x_base = max(0, min(src_w - 2, int(sx)))
            fx = sx - x_base
            samples = [
                pixels[(y_base * src_w) + x_base],
                pixels[(y_base * src_w) + x_base + 1],
                pixels[((y_base + 1) * src_w) + x_base],
                pixels[((y_base + 1) * src_w) + x_base + 1],
            ]
            rgb = []
            for channel in range(3):
                top = (samples[0][channel] * (1.0 - fx)) + (samples[1][channel] * fx)
                bottom = (samples[2][channel] * (1.0 - fx)) + (samples[3][channel] * fx)
                rgb.append(int(round((top * (1.0 - fy)) + (bottom * fy))))
            out.append((rgb[0], rgb[1], rgb[2]))
    return out


def rgb565(color: tuple[int, int, int]) -> int:
    r, g, b = color
    return ((r >> 3) << 11) | ((g >> 2) << 5) | (b >> 3)


def rle_encode(pixels: list[tuple[int, int, int]]) -> list[list[object]]:
    runs: list[list[object]] = []
    last: int | None = None
    count = 0
    for color in pixels:
        value = rgb565(color)
        if last is None:
            last = value
            count = 1
        elif value == last and count < 0xFFFF:
            count += 1
        else:
            runs.append([count, f"0x{last:04X}"])
            last = value
            count = 1
    if last is not None:
        runs.append([count, f"0x{last:04X}"])
    return runs


def main() -> int:
    parser = argparse.ArgumentParser(description=__doc__)
    parser.add_argument("input", type=Path)
    parser.add_argument("-o", "--output", type=Path, required=True)
    parser.add_argument("--width", type=int, default=240)
    parser.add_argument("--height", type=int, default=320)
    parser.add_argument("--description", default="")
    args = parser.parse_args()

    src_w, src_h, pixels = read_png_rgb(args.input)
    resized = resize_cover(src_w, src_h, pixels, args.width, args.height)
    data = {
        "format": "IMAGE_RGB565_RLE_V1",
        "width": args.width,
        "height": args.height,
        "source": str(args.input).replace("\\", "/"),
        "description": args.description,
        "runs": rle_encode(resized),
    }
    args.output.parent.mkdir(parents=True, exist_ok=True)
    args.output.write_text(json.dumps(data, indent=2) + "\n", encoding="utf-8")
    print(f"wrote {args.output} {args.width}x{args.height} runs={len(data['runs'])}")
    return 0


if __name__ == "__main__":
    raise SystemExit(main())
