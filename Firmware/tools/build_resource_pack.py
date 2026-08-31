#!/usr/bin/env python3
"""Build a deterministic WTK.RLCMeter resource pack v2 binary."""

from __future__ import annotations

import argparse
from pathlib import Path

from resource_pack_format import build_pack


def main() -> int:
    parser = argparse.ArgumentParser()
    parser.add_argument("manifest", type=Path)
    parser.add_argument("-o", "--output", type=Path, required=True)
    args = parser.parse_args()
    data = build_pack(args.manifest)
    args.output.parent.mkdir(parents=True, exist_ok=True)
    args.output.write_bytes(data)
    print(f"wrote {args.output} ({len(data)} bytes)")
    return 0


if __name__ == "__main__":
    raise SystemExit(main())
