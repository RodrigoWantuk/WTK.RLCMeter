#!/usr/bin/env python3
"""Deterministic WTK.RLCMeter resource-pack v2 builder/reader helpers."""

from __future__ import annotations

from dataclasses import dataclass
import json
from pathlib import Path
import struct
import zlib


PACK_MAGIC = 0x32505257
PACK_SCHEMA_VERSION = 2
PACK_API_VERSION = 1
PACK_HEADER_SIZE = 44
PACK_ENTRY_SIZE = 32
TEXT_MAGIC = 0x54585457
TEXT_VERSION = 1
TEXT_HEADER_SIZE = 24
TEXT_RECORD_SIZE = 8

RESOURCE_IDS = {"TEXT_EN": 0x00010001, "TEXT_PT_BR": 0x00010002}
RESOURCE_TYPES = {"TEXT_TABLE": 1}
RESOURCE_FORMATS = {"TEXT_TABLE_UTF8_V1": 1}
LANGUAGE_IDS = {"en": 1, "pt-BR": 2}
REQUIRED_TEXT_IDS = tuple(range(0x0001, 0x0038))


@dataclass(frozen=True)
class ResourceEntry:
    resource_id: int
    resource_type: int
    fmt: int
    flags: int
    payload_offset: int
    payload_size: int
    payload_crc32: int
    aux_offset: int = 0
    aux_size: int = 0

    def encode(self) -> bytes:
        return struct.pack(
            "<IHHIIIIII",
            self.resource_id,
            self.resource_type,
            self.fmt,
            self.flags,
            self.payload_offset,
            self.payload_size,
            self.payload_crc32,
            self.aux_offset,
            self.aux_size,
        )


def crc32(data: bytes) -> int:
    return zlib.crc32(data) & 0xFFFFFFFF


def encode_header(total_size: int, entries: list[ResourceEntry], table_crc: int, header_crc: int = 0) -> bytes:
    return struct.pack(
        "<IHHHHIHHIIIIII",
        PACK_MAGIC,
        PACK_SCHEMA_VERSION,
        PACK_HEADER_SIZE,
        PACK_API_VERSION,
        0,
        total_size,
        len(entries),
        PACK_ENTRY_SIZE,
        PACK_HEADER_SIZE,
        PACK_HEADER_SIZE + (len(entries) * PACK_ENTRY_SIZE),
        table_crc,
        header_crc,
        0,
        0,
    )


def build_text_payload(path: Path) -> bytes:
    data = json.loads(path.read_text(encoding="utf-8"))
    language = data["language"]
    if language not in LANGUAGE_IDS:
        raise ValueError(f"unsupported language {language!r}")
    strings = data["strings"]
    present_ids = {int(key, 16) for key in strings}
    missing_ids = [text_id for text_id in REQUIRED_TEXT_IDS if text_id not in present_ids]
    if missing_ids:
        missing = ", ".join(f"0x{text_id:04X}" for text_id in missing_ids)
        raise ValueError(f"text catalog {path} is missing required ids: {missing}")
    records: list[tuple[int, bytes, int]] = []
    blob = bytearray()
    for key in sorted(strings, key=lambda value: int(value, 16)):
        text_id = int(key, 16)
        encoded = str(strings[key]).encode("utf-8")
        if not encoded:
            raise ValueError(f"empty string for {key}")
        encoded.decode("utf-8")
        records.append((text_id, encoded, len(blob)))
        blob.extend(encoded)
    index = bytearray()
    previous = 0
    for text_id, encoded, offset in records:
        if text_id <= previous:
            raise ValueError("text ids must be strictly increasing")
        previous = text_id
        index.extend(struct.pack("<HHI", text_id, len(encoded), offset))
    header = struct.pack(
        "<IHHBBHIII",
        TEXT_MAGIC,
        TEXT_VERSION,
        TEXT_HEADER_SIZE,
        LANGUAGE_IDS[language],
        0,
        len(records),
        TEXT_HEADER_SIZE,
        TEXT_HEADER_SIZE + len(index),
        crc32(bytes(index)),
    )
    return header + bytes(index) + bytes(blob)


def build_pack(manifest_path: Path) -> bytes:
    manifest = json.loads(manifest_path.read_text(encoding="utf-8"))
    if manifest.get("schema_version") != PACK_SCHEMA_VERSION:
        raise ValueError("manifest schema_version must be 2")
    base = manifest_path.parent
    specs = sorted(manifest["resources"], key=lambda item: RESOURCE_IDS[item["id"]])
    data_offset = PACK_HEADER_SIZE + (len(specs) * PACK_ENTRY_SIZE)
    payloads: list[bytes] = []
    entries: list[ResourceEntry] = []
    cursor = data_offset
    previous_id = 0
    for spec in specs:
        resource_id = RESOURCE_IDS[spec["id"]]
        if resource_id <= previous_id:
            raise ValueError("resource ids must be unique and sorted")
        previous_id = resource_id
        if spec["type"] != "TEXT_TABLE":
            raise ValueError("Stage 3A builder only supports text tables")
        payload = build_text_payload(base / spec["path"])
        entries.append(
            ResourceEntry(
                resource_id=resource_id,
                resource_type=RESOURCE_TYPES[spec["type"]],
                fmt=RESOURCE_FORMATS[spec["format"]],
                flags=0,
                payload_offset=cursor,
                payload_size=len(payload),
                payload_crc32=crc32(payload),
            )
        )
        payloads.append(payload)
        cursor += len(payload)
    table = b"".join(entry.encode() for entry in entries)
    header_without_crc = encode_header(cursor, entries, crc32(table), 0)
    header = encode_header(cursor, entries, crc32(table), crc32(header_without_crc))
    return header + table + b"".join(payloads)


def inspect_pack(data: bytes) -> dict[str, object]:
    if len(data) < PACK_HEADER_SIZE:
        raise ValueError("pack too small")
    unpacked = struct.unpack("<IHHHHIHHIIIIII", data[:PACK_HEADER_SIZE])
    magic, schema, header_size, api, flags, total, count, entry_size, table_off, data_off, table_crc, header_crc, r0, r1 = unpacked
    if magic != PACK_MAGIC or schema != PACK_SCHEMA_VERSION or header_size != PACK_HEADER_SIZE:
        raise ValueError("invalid pack header")
    if api != PACK_API_VERSION or flags != 0 or r0 != 0 or r1 != 0:
        raise ValueError("unsupported pack header")
    if total != len(data) or entry_size != PACK_ENTRY_SIZE:
        raise ValueError("invalid pack bounds")
    check_header = bytearray(data[:PACK_HEADER_SIZE])
    struct.pack_into("<I", check_header, 32, 0)
    if crc32(bytes(check_header)) != header_crc:
        raise ValueError("header CRC mismatch")
    table = data[table_off : table_off + (count * PACK_ENTRY_SIZE)]
    if crc32(table) != table_crc:
        raise ValueError("entry table CRC mismatch")
    entries = []
    for index in range(count):
        fields = struct.unpack("<IHHIIIIII", table[index * PACK_ENTRY_SIZE : (index + 1) * PACK_ENTRY_SIZE])
        entries.append(
            {
                "resource_id": fields[0],
                "resource_type": fields[1],
                "format": fields[2],
                "payload_offset": fields[4],
                "payload_size": fields[5],
                "payload_crc32": fields[6],
            }
        )
    return {"schema_version": schema, "resource_api_version": api, "total_pack_size": total, "entry_count": count, "entries": entries}
