#!/usr/bin/env python3
"""Inspect and validate a WTK.RLCMeter resource pack v2 binary."""

from __future__ import annotations

import argparse
import json
from pathlib import Path

from resource_pack_format import inspect_pack


def main() -> int:
    parser = argparse.ArgumentParser()
    parser.add_argument("pack", type=Path)
    parser.add_argument("--json", action="store_true")
    args = parser.parse_args()
    info = inspect_pack(args.pack.read_bytes())
    if args.json:
        print(json.dumps(info, indent=2, sort_keys=True))
    else:
        print(f"schema_version={info['schema_version']}")
        print(f"resource_api_version={info['resource_api_version']}")
        print(f"total_pack_size={info['total_pack_size']}")
        print(f"entry_count={info['entry_count']}")
        for entry in info["entries"]:
            print(
                "entry "
                f"id=0x{entry['resource_id']:08X} "
                f"type={entry['resource_type']} "
                f"format={entry['format']} "
                f"offset={entry['payload_offset']} "
                f"size={entry['payload_size']} "
                f"crc=0x{entry['payload_crc32']:08X}"
            )
    return 0


if __name__ == "__main__":
    raise SystemExit(main())
