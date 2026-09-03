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
PACK_API_VERSION = 2
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
TEXT_ID_FIRST = 0x0001
TEXT_ID_LAST = 0x0038
TEXT_MAX_BYTES = 31
REQUIRED_TEXT_IDS = tuple(range(TEXT_ID_FIRST, TEXT_ID_LAST + 1))
REQUIRED_LANGUAGE_RESOURCE_IDS = {
    "en": RESOURCE_IDS["TEXT_EN"],
    "pt-BR": RESOURCE_IDS["TEXT_PT_BR"],
}


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
    extra_ids = sorted(text_id for text_id in present_ids if text_id not in REQUIRED_TEXT_IDS)
    if extra_ids:
        extra = ", ".join(f"0x{text_id:04X}" for text_id in extra_ids)
        raise ValueError(f"text catalog {path} contains unsupported ids: {extra}")
    missing_ids = [text_id for text_id in REQUIRED_TEXT_IDS if text_id not in present_ids]
    if missing_ids:
        missing = ", ".join(f"0x{text_id:04X}" for text_id in missing_ids)
        raise ValueError(f"text catalog {path} is missing required ids: {missing}")
    records: list[tuple[int, bytes, int]] = []
    blob = bytearray()
    for text_id in REQUIRED_TEXT_IDS:
        key = f"0x{text_id:04X}"
        encoded = str(strings[key]).encode("utf-8")
        if not encoded:
            raise ValueError(f"empty string for {key}")
        if len(encoded) > TEXT_MAX_BYTES:
            raise ValueError(f"text {key} exceeds {TEXT_MAX_BYTES} UTF-8 bytes")
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
    resources = manifest["resources"]
    found_languages = {spec.get("language"): RESOURCE_IDS.get(spec.get("id", "")) for spec in resources}
    for language, resource_id in REQUIRED_LANGUAGE_RESOURCE_IDS.items():
        if found_languages.get(language) != resource_id:
            raise ValueError(f"manifest missing required {language} text resource")
    specs = sorted(resources, key=lambda item: RESOURCE_IDS[item["id"]])
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
        if spec.get("language") not in LANGUAGE_IDS:
            raise ValueError("text resource must declare a supported language")
        payload = build_text_payload(base / spec["path"])
        declared_language = spec["language"]
        payload_language = struct.unpack("<IHHBBHIII", payload[:TEXT_HEADER_SIZE])[3]
        if LANGUAGE_IDS[declared_language] != payload_language:
            raise ValueError(f"text resource {spec['id']} language does not match payload")
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
    seen_language_resources: dict[int, int] = {}
    for index in range(count):
        fields = struct.unpack("<IHHIIIIII", table[index * PACK_ENTRY_SIZE : (index + 1) * PACK_ENTRY_SIZE])
        resource_id, resource_type, fmt, flags, payload_offset, payload_size, payload_crc32, aux_offset, aux_size = fields
        if flags != 0 or aux_offset != 0 or aux_size != 0:
            raise ValueError("unsupported entry flags/aux fields")
        if index != 0 and resource_id <= entries[-1]["resource_id"]:
            raise ValueError("resource ids are not strictly increasing")
        if payload_size == 0 or payload_offset < data_off or payload_offset + payload_size > total:
            raise ValueError("entry payload out of bounds")
        payload = data[payload_offset : payload_offset + payload_size]
        if crc32(payload) != payload_crc32:
            raise ValueError("payload CRC mismatch")
        language_id = None
        if resource_type == RESOURCE_TYPES["TEXT_TABLE"]:
            if fmt != RESOURCE_FORMATS["TEXT_TABLE_UTF8_V1"]:
                raise ValueError("unsupported text format")
            language_id = _inspect_text_payload(payload, resource_id)
            seen_language_resources[language_id] = resource_id
        entries.append(
            {
                "resource_id": resource_id,
                "resource_type": resource_type,
                "format": fmt,
                "payload_offset": payload_offset,
                "payload_size": payload_size,
                "payload_crc32": payload_crc32,
                "language_id": language_id,
            }
        )
    for language, resource_id in REQUIRED_LANGUAGE_RESOURCE_IDS.items():
        if seen_language_resources.get(LANGUAGE_IDS[language]) != resource_id:
            raise ValueError(f"missing required {language} text table")
    return {
        "schema_version": schema,
        "resource_api_version": api,
        "total_pack_size": total,
        "entry_count": count,
        "entries": entries,
        "text_id_first": TEXT_ID_FIRST,
        "text_id_last": TEXT_ID_LAST,
        "text_max_bytes": TEXT_MAX_BYTES,
    }


def _inspect_text_payload(payload: bytes, resource_id: int) -> int:
    if len(payload) < TEXT_HEADER_SIZE:
        raise ValueError("text payload too small")
    magic, version, header_size, language_id, reserved0, record_count, index_offset, blob_offset, index_crc = struct.unpack(
        "<IHHBBHIII", payload[:TEXT_HEADER_SIZE]
    )
    if (
        magic != TEXT_MAGIC
        or version != TEXT_VERSION
        or header_size != TEXT_HEADER_SIZE
        or reserved0 != 0
        or language_id not in LANGUAGE_IDS.values()
        or record_count != len(REQUIRED_TEXT_IDS)
        or index_offset != TEXT_HEADER_SIZE
        or blob_offset != TEXT_HEADER_SIZE + record_count * TEXT_RECORD_SIZE
        or blob_offset > len(payload)
    ):
        raise ValueError("invalid text table header")
    expected_resource = {
        LANGUAGE_IDS["en"]: RESOURCE_IDS["TEXT_EN"],
        LANGUAGE_IDS["pt-BR"]: RESOURCE_IDS["TEXT_PT_BR"],
    }[language_id]
    if resource_id != expected_resource:
        raise ValueError("text resource id does not match language")
    index = payload[index_offset:blob_offset]
    if len(index) != record_count * TEXT_RECORD_SIZE or crc32(index) != index_crc:
        raise ValueError("text index CRC mismatch")
    cursor = 0
    for i, text_id in enumerate(REQUIRED_TEXT_IDS):
        start = i * TEXT_RECORD_SIZE
        record_id, byte_length, byte_offset = struct.unpack("<HHI", index[start : start + TEXT_RECORD_SIZE])
        if record_id != text_id or byte_length == 0 or byte_length > TEXT_MAX_BYTES or byte_offset != cursor:
            raise ValueError("invalid text record")
        text_start = blob_offset + byte_offset
        text_end = text_start + byte_length
        if text_end > len(payload):
            raise ValueError("text record escapes blob")
        payload[text_start:text_end].decode("utf-8")
        cursor += byte_length
    if blob_offset + cursor != len(payload):
        raise ValueError("text blob contains unreferenced bytes")
    return language_id
