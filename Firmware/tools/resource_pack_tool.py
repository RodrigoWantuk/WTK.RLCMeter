#!/usr/bin/env python3
"""WTK.RLCMeter PC-side Resource Pack utility."""

from __future__ import annotations

import argparse
import hashlib
import json
from pathlib import Path

from pc_link_resource_pack import iter_resource_frames, send_serial, write_stream
from resource_pack_format import build_pack, inspect_pack


def pack_summary(pack: bytes) -> dict[str, object]:
    info = inspect_pack(pack)
    return {
        "sha256": hashlib.sha256(pack).hexdigest(),
        "size_bytes": len(pack),
        "schema_version": info["schema_version"],
        "resource_api_version": info["resource_api_version"],
        "entry_count": info["entry_count"],
        "entries": [
            {
                "resource_id": f"0x{entry['resource_id']:08X}",
                "resource_type": entry["resource_type"],
                "format": entry["format"],
                "payload_size": entry["payload_size"],
                "payload_crc32": f"0x{entry['payload_crc32']:08X}",
                "language_id": entry.get("language_id"),
                "font": entry.get("font"),
                "image": entry.get("image"),
            }
            for entry in info["entries"]
        ],
    }


def write_summary(path: Path | None, summary: dict[str, object]) -> None:
    if path is None:
        return
    path.parent.mkdir(parents=True, exist_ok=True)
    path.write_text(json.dumps(summary, indent=2, sort_keys=True) + "\n", encoding="utf-8")


def cmd_build(args: argparse.Namespace) -> int:
    pack = build_pack(args.manifest)
    args.output.parent.mkdir(parents=True, exist_ok=True)
    args.output.write_bytes(pack)
    summary = pack_summary(pack)
    write_summary(args.summary, summary)
    print(f"pack={args.output} bytes={summary['size_bytes']} sha256={summary['sha256']}")
    return 0


def cmd_inspect(args: argparse.Namespace) -> int:
    summary = pack_summary(args.pack.read_bytes())
    if args.json:
        print(json.dumps(summary, indent=2, sort_keys=True))
    else:
        print(f"bytes={summary['size_bytes']}")
        print(f"sha256={summary['sha256']}")
        print(f"schema_version={summary['schema_version']}")
        print(f"resource_api_version={summary['resource_api_version']}")
        print(f"entry_count={summary['entry_count']}")
        for entry in summary["entries"]:
            print(
                "entry "
                f"id={entry['resource_id']} "
                f"type={entry['resource_type']} "
                f"format={entry['format']} "
                f"size={entry['payload_size']} "
                f"crc={entry['payload_crc32']}"
            )
    return 0


def cmd_frame(args: argparse.Namespace) -> int:
    pack = args.pack.read_bytes()
    frames = iter_resource_frames(pack, args.chunk_size)
    write_stream(frames, args.output)
    total = sum(len(frame) for frame in frames)
    print(f"stream={args.output} frames={len(frames)} bytes={total}")
    return 0


def cmd_bundle(args: argparse.Namespace) -> int:
    pack = build_pack(args.manifest)
    args.output.parent.mkdir(parents=True, exist_ok=True)
    args.output.write_bytes(pack)
    summary = pack_summary(pack)
    write_summary(args.summary, summary)
    if args.stream is not None:
        frames = iter_resource_frames(pack, args.chunk_size)
        write_stream(frames, args.stream)
        print(f"stream={args.stream} frames={len(frames)} bytes={sum(len(frame) for frame in frames)}")
    print(f"pack={args.output} bytes={summary['size_bytes']} sha256={summary['sha256']}")
    return 0


def cmd_upload(args: argparse.Namespace) -> int:
    pack = args.pack.read_bytes()
    frames = iter_resource_frames(pack, args.chunk_size)
    send_serial(frames, args.port, args.baud, args.inter_frame_delay_ms / 1000.0)
    print(f"uploaded frames={len(frames)} pack_bytes={len(pack)} port={args.port}")
    return 0


def build_parser() -> argparse.ArgumentParser:
    parser = argparse.ArgumentParser(description=__doc__)
    sub = parser.add_subparsers(dest="command", required=True)

    build = sub.add_parser("build", help="build a Resource Pack from a source manifest")
    build.add_argument("manifest", type=Path)
    build.add_argument("-o", "--output", type=Path, required=True)
    build.add_argument("--summary", type=Path, help="write a deterministic JSON summary")
    build.set_defaults(func=cmd_build)

    inspect = sub.add_parser("inspect", help="inspect and validate a Resource Pack")
    inspect.add_argument("pack", type=Path)
    inspect.add_argument("--json", action="store_true")
    inspect.set_defaults(func=cmd_inspect)

    frame = sub.add_parser("frame", help="write a deterministic PC-link framed stream")
    frame.add_argument("pack", type=Path)
    frame.add_argument("-o", "--output", type=Path, required=True)
    frame.add_argument("--chunk-size", type=int, default=120)
    frame.set_defaults(func=cmd_frame)

    bundle = sub.add_parser("bundle", help="build, validate, summarize, and optionally frame a Resource Pack")
    bundle.add_argument("manifest", type=Path)
    bundle.add_argument("-o", "--output", type=Path, required=True)
    bundle.add_argument("--summary", type=Path, required=True)
    bundle.add_argument("--stream", type=Path, help="optional deterministic PC-link framed stream output")
    bundle.add_argument("--chunk-size", type=int, default=120)
    bundle.set_defaults(func=cmd_bundle)

    upload = sub.add_parser("upload", help="upload a Resource Pack over serial using PC-link ACKs")
    upload.add_argument("pack", type=Path)
    upload.add_argument("--port", required=True, help="serial port, for example COM5")
    upload.add_argument("--baud", type=int, default=115200)
    upload.add_argument("--chunk-size", type=int, default=120)
    upload.add_argument("--inter-frame-delay-ms", type=float, default=0.0)
    upload.set_defaults(func=cmd_upload)
    return parser


def main(argv: list[str] | None = None) -> int:
    args = build_parser().parse_args(argv)
    return int(args.func(args))


if __name__ == "__main__":
    raise SystemExit(main())
