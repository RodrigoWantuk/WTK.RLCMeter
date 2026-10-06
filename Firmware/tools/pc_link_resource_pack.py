#!/usr/bin/env python3
"""Prepare or send Resource Pack v2 images using WTK PC-link frames."""

from __future__ import annotations

import argparse
from pathlib import Path
import struct
import sys
import time
import zlib

from resource_pack_format import PACK_API_VERSION


MAGIC = 0x31434C50  # "PLC1" little-endian
VERSION = 1
HEADER_SIZE = 16
MAX_PAYLOAD_BYTES = 128
RESOURCE_CHUNK_DATA_BYTES = 120

FRAME_RESOURCE_BEGIN = 16
FRAME_RESOURCE_CHUNK = 17
FRAME_RESOURCE_END = 18
FRAME_STATUS = 2
STATUS_PAYLOAD_SIZE = 8

PC_STATUS_OK = 0
UPDATE_STATUS_OK = 0
UPDATE_STATUS_BUSY = 1
UPDATE_STATUS_COMPLETE = 2


def crc32(data: bytes) -> int:
    return zlib.crc32(data) & 0xFFFFFFFF


def encode_frame(frame_type: int, sequence: int, payload: bytes, flags: int = 0) -> bytes:
    if not 0 <= frame_type <= 0xFF:
        raise ValueError("frame_type out of range")
    if not 0 <= sequence <= 0xFFFF:
        raise ValueError("sequence out of range")
    if len(payload) > MAX_PAYLOAD_BYTES:
        raise ValueError("payload too large")
    header = struct.pack(
        "<IBBHHHI",
        MAGIC,
        VERSION,
        frame_type,
        flags,
        sequence,
        len(payload),
        crc32(payload),
    )
    return header + payload


def decode_frame(frame: bytes) -> tuple[int, int, bytes]:
    if len(frame) < HEADER_SIZE:
        raise ValueError("truncated frame")
    magic, version, frame_type, _flags, sequence, length, payload_crc = struct.unpack("<IBBHHHI", frame[:HEADER_SIZE])
    if magic != MAGIC:
        raise ValueError("bad magic")
    if version != VERSION:
        raise ValueError("unsupported version")
    if length > MAX_PAYLOAD_BYTES:
        raise ValueError("payload too large")
    if len(frame) != HEADER_SIZE + length:
        raise ValueError("truncated payload")
    payload = frame[HEADER_SIZE:]
    if crc32(payload) != payload_crc:
        raise ValueError("payload crc mismatch")
    return frame_type, sequence, payload


def iter_resource_frames(pack: bytes, chunk_size: int = RESOURCE_CHUNK_DATA_BYTES) -> list[bytes]:
    if not pack:
        raise ValueError("resource pack is empty")
    if not 1 <= chunk_size <= RESOURCE_CHUNK_DATA_BYTES:
        raise ValueError(f"chunk size must be 1..{RESOURCE_CHUNK_DATA_BYTES}")

    frames: list[bytes] = []
    sequence = 1
    pack_crc = crc32(pack)
    begin = struct.pack("<IIHH", len(pack), pack_crc, PACK_API_VERSION, 0)
    frames.append(encode_frame(FRAME_RESOURCE_BEGIN, sequence, begin))
    sequence += 1

    for offset in range(0, len(pack), chunk_size):
        chunk = pack[offset : offset + chunk_size]
        payload = struct.pack("<IHH", offset, len(chunk), 0) + chunk
        frames.append(encode_frame(FRAME_RESOURCE_CHUNK, sequence, payload))
        sequence = (sequence + 1) & 0xFFFF
        if sequence == 0:
            sequence = 1

    end = struct.pack("<II", len(pack), pack_crc)
    frames.append(encode_frame(FRAME_RESOURCE_END, sequence, end))
    return frames


def write_stream(frames: list[bytes], output: Path) -> None:
    output.parent.mkdir(parents=True, exist_ok=True)
    output.write_bytes(b"".join(frames))


def _read_exact(link, size: int) -> bytes:
    data = link.read(size)
    if len(data) != size:
        raise TimeoutError(f"timed out waiting for {size} bytes, received {len(data)}")
    return data


def read_status_frame(link) -> tuple[int, int, int, int]:
    header = _read_exact(link, HEADER_SIZE)
    _magic, _version, _frame_type, _flags, _sequence, length, _payload_crc = struct.unpack("<IBBHHHI", header)
    payload = _read_exact(link, length)
    frame_type, sequence, decoded_payload = decode_frame(header + payload)
    if frame_type != FRAME_STATUS:
        raise ValueError(f"unexpected response frame type {frame_type}")
    if len(decoded_payload) != STATUS_PAYLOAD_SIZE:
        raise ValueError("bad status payload length")
    status_sequence, pc_status, update_status, update_state = struct.unpack("<HHHH", decoded_payload)
    if status_sequence != sequence:
        raise ValueError("status payload sequence mismatch")
    return status_sequence, pc_status, update_status, update_state


def send_serial(frames: list[bytes], port: str, baud: int, inter_frame_delay_s: float) -> None:
    try:
        import serial  # type: ignore[import-not-found]
    except ImportError as exc:
        raise SystemExit("pyserial is required for --port transmission") from exc

    with serial.Serial(port=port, baudrate=baud, timeout=2.0, write_timeout=2.0) as link:
        for frame in frames:
            _frame_type, sequence, _payload = decode_frame(frame)
            link.write(frame)
            link.flush()
            status_sequence, pc_status, update_status, _update_state = read_status_frame(link)
            if status_sequence != sequence:
                raise RuntimeError(f"status sequence {status_sequence} does not match frame {sequence}")
            if (pc_status != PC_STATUS_OK) or (
                update_status not in (UPDATE_STATUS_OK, UPDATE_STATUS_COMPLETE)
            ):
                raise RuntimeError(
                    f"device rejected frame {sequence}: pc_status={pc_status} update_status={update_status}"
                )
            if inter_frame_delay_s > 0.0:
                time.sleep(inter_frame_delay_s)


def main(argv: list[str] | None = None) -> int:
    parser = argparse.ArgumentParser(description=__doc__)
    parser.add_argument("resource_pack", type=Path, help="Resource Pack v2 .wrp2 image")
    parser.add_argument("--out", type=Path, help="write a deterministic framed .wpc stream")
    parser.add_argument("--port", help="optional serial port, for example COM5")
    parser.add_argument("--baud", type=int, default=115200)
    parser.add_argument("--chunk-size", type=int, default=RESOURCE_CHUNK_DATA_BYTES)
    parser.add_argument("--inter-frame-delay-ms", type=float, default=2.0)
    args = parser.parse_args(argv)

    pack = args.resource_pack.read_bytes()
    frames = iter_resource_frames(pack, args.chunk_size)

    if args.out is not None:
        write_stream(frames, args.out)

    if args.port:
        send_serial(frames, args.port, args.baud, args.inter_frame_delay_ms / 1000.0)

    if args.out is None and not args.port:
        total = sum(len(frame) for frame in frames)
        print(f"frames={len(frames)} bytes={total} pack_bytes={len(pack)} crc32=0x{crc32(pack):08X}")

    return 0


if __name__ == "__main__":
    raise SystemExit(main())
