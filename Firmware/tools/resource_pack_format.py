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
PACK_API_VERSION = 3
PACK_HEADER_SIZE = 44
PACK_ENTRY_SIZE = 32
TEXT_MAGIC = 0x54585457
TEXT_VERSION = 1
TEXT_HEADER_SIZE = 24
TEXT_RECORD_SIZE = 8
FONT_MAGIC = 0x31414657
FONT_VERSION = 1
FONT_HEADER_SIZE = 32
FONT_RECORD_SIZE = 20
FONT_MAX_WIDTH = 32
FONT_MAX_HEIGHT = 32

RESOURCE_IDS = {
    "TEXT_EN": 0x00010001,
    "TEXT_PT_BR": 0x00010002,
    "FONT_UI_SMALL": 0x00020001,
    "FONT_UI_MEDIUM": 0x00020002,
    "FONT_UI_LARGE": 0x00020003,
}
RESOURCE_TYPES = {"TEXT_TABLE": 1, "FONT_BITMAP_A1": 2}
RESOURCE_FORMATS = {"TEXT_TABLE_UTF8_V1": 1, "FONT_BITMAP_A1_V1": 2}
LANGUAGE_IDS = {"en": 1, "pt-BR": 2}
TEXT_ID_FIRST = 0x0001
TEXT_ID_LAST = 0x0038
TEXT_MAX_BYTES = 31
REQUIRED_TEXT_IDS = tuple(range(TEXT_ID_FIRST, TEXT_ID_LAST + 1))
REQUIRED_LANGUAGE_RESOURCE_IDS = {
    "en": RESOURCE_IDS["TEXT_EN"],
    "pt-BR": RESOURCE_IDS["TEXT_PT_BR"],
}
REQUIRED_FONT_RESOURCE_IDS = (
    RESOURCE_IDS["FONT_UI_SMALL"],
    RESOURCE_IDS["FONT_UI_MEDIUM"],
    RESOURCE_IDS["FONT_UI_LARGE"],
)
TECHNICAL_SYMBOLS = "0123456789ABCDEFGHIJKLMNOPQRSTUVWXYZabcdefghijklmnopqrstuvwxyz .,;:-+/()%|=?Ωµ°±·"


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


def _unicode_scalar_valid(codepoint: int) -> bool:
    return 0 <= codepoint <= 0x10FFFF and not (0xD800 <= codepoint <= 0xDFFF)


def _glyph_key_to_codepoint(key: str) -> int:
    if key.startswith("U+"):
        return int(key[2:], 16)
    if len(key) == 1:
        return ord(key)
    raise ValueError(f"invalid glyph key {key!r}")


def _normalize_rows(rows: list[str]) -> tuple[str, ...]:
    if not rows:
        raise ValueError("glyph rows cannot be empty")
    width = len(rows[0])
    if width == 0:
        raise ValueError("glyph width cannot be zero")
    normalized = []
    for row in rows:
        if len(row) != width:
            raise ValueError("glyph rows must have equal width")
        if any(ch not in ".1#" for ch in row):
            raise ValueError("glyph rows must use '.', '1', or '#")
        normalized.append("".join("1" if ch in "1#" else "." for ch in row))
    return tuple(normalized)


def _scale_rows(rows: tuple[str, ...], scale: int) -> tuple[str, ...]:
    if scale <= 0:
        raise ValueError("font scale must be positive")
    scaled: list[str] = []
    for row in rows:
        out = "".join(ch * scale for ch in row)
        for _ in range(scale):
            scaled.append(out)
    return tuple(scaled)


def _pack_a1_rows(rows: tuple[str, ...]) -> bytes:
    if not rows:
        return b""
    width = len(rows[0])
    row_stride = (width + 7) // 8
    packed = bytearray()
    for row in rows:
        for byte_index in range(row_stride):
            value = 0
            for bit in range(8):
                col = (byte_index * 8) + bit
                if col < width and row[col] == "1":
                    value |= 1 << (7 - bit)
            packed.append(value)
    return bytes(packed)


def _load_font_source(path: Path) -> dict[int, tuple[str, ...]]:
    data = json.loads(path.read_text(encoding="utf-8"))
    glyphs: dict[int, tuple[str, ...]] = {}
    for key, rows in data.get("glyphs", {}).items():
        codepoint = _glyph_key_to_codepoint(key)
        if not _unicode_scalar_valid(codepoint) or codepoint in glyphs:
            raise ValueError(f"invalid or duplicate glyph codepoint {key!r}")
        glyphs[codepoint] = _normalize_rows(list(rows))
    for key, target in data.get("aliases", {}).items():
        codepoint = _glyph_key_to_codepoint(key)
        target_codepoint = _glyph_key_to_codepoint(str(target))
        if codepoint in glyphs:
            raise ValueError(f"duplicate glyph/alias {key!r}")
        if target_codepoint not in glyphs:
            raise ValueError(f"alias {key!r} targets missing glyph {target!r}")
        glyphs[codepoint] = glyphs[target_codepoint]
    return glyphs


def required_font_codepoints(manifest_path: Path) -> set[int]:
    manifest = json.loads(manifest_path.read_text(encoding="utf-8"))
    base = manifest_path.parent
    codepoints = {ord(ch) for ch in TECHNICAL_SYMBOLS}
    for spec in manifest["resources"]:
        if spec.get("type") != "TEXT_TABLE":
            continue
        data = json.loads((base / spec["path"]).read_text(encoding="utf-8"))
        for text in data["strings"].values():
            codepoints.update(ord(ch) for ch in str(text))
    return codepoints


def build_font_payload(path: Path, scale: int, required_codepoints: set[int]) -> bytes:
    glyphs = _load_font_source(path)
    required = set(required_codepoints)
    required.update((ord(" "), ord("?")))
    missing = sorted(codepoint for codepoint in required if codepoint not in glyphs)
    if missing:
        missing_text = ", ".join(f"U+{codepoint:04X}" for codepoint in missing[:16])
        raise ValueError(f"font {path} missing required glyphs: {missing_text}")

    records = []
    bitmap = bytearray()
    for codepoint in sorted(glyphs):
        rows = glyphs[codepoint]
        scaled = _scale_rows(rows, scale)
        width = len(scaled[0])
        height = len(scaled)
        if width > FONT_MAX_WIDTH or height > FONT_MAX_HEIGHT:
            raise ValueError(f"glyph U+{codepoint:04X} exceeds {FONT_MAX_WIDTH}x{FONT_MAX_HEIGHT}")
        if codepoint == ord(" "):
            width = 0
            height = 0
            row_stride = 0
            glyph_bytes = b""
            bitmap_offset = 0
        else:
            row_stride = (width + 7) // 8
            glyph_bytes = _pack_a1_rows(scaled)
            if len(glyph_bytes) != row_stride * height:
                raise ValueError("internal font bitmap size mismatch")
            bitmap_offset = len(bitmap)
            bitmap.extend(glyph_bytes)
        advance = 6 * scale
        if advance > 127:
            raise ValueError("font advance exceeds int8")
        records.append((codepoint, bitmap_offset, len(glyph_bytes), width, height, advance, 0, height, row_stride))

    index = bytearray()
    previous = -1
    for record in records:
        codepoint = record[0]
        if codepoint <= previous:
            raise ValueError("font codepoints must be strictly increasing")
        previous = codepoint
        index.extend(struct.pack("<IIHBBbbbBH", *record, 0))
        index.extend(b"\x00\x00")
    header = struct.pack(
        "<IHHHHbbBBIIII",
        FONT_MAGIC,
        FONT_VERSION,
        FONT_HEADER_SIZE,
        len(records),
        FONT_RECORD_SIZE,
        7 * scale,
        1 * scale,
        8 * scale,
        0,
        FONT_HEADER_SIZE,
        FONT_HEADER_SIZE + len(index),
        crc32(bytes(index)),
        0,
    )
    return header + bytes(index) + bytes(bitmap)


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
    found_fonts = {RESOURCE_IDS.get(spec.get("id", "")) for spec in resources if spec.get("type") == "FONT_BITMAP_A1"}
    for resource_id in REQUIRED_FONT_RESOURCE_IDS:
        if resource_id not in found_fonts:
            raise ValueError(f"manifest missing required font resource 0x{resource_id:08X}")
    required_codepoints = required_font_codepoints(manifest_path)
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
        if spec["type"] == "TEXT_TABLE":
            if spec.get("language") not in LANGUAGE_IDS:
                raise ValueError("text resource must declare a supported language")
            payload = build_text_payload(base / spec["path"])
            declared_language = spec["language"]
            payload_language = struct.unpack("<IHHBBHIII", payload[:TEXT_HEADER_SIZE])[3]
            if LANGUAGE_IDS[declared_language] != payload_language:
                raise ValueError(f"text resource {spec['id']} language does not match payload")
        elif spec["type"] == "FONT_BITMAP_A1":
            if spec["format"] != "FONT_BITMAP_A1_V1":
                raise ValueError("unsupported font format")
            payload = build_font_payload(base / spec["path"], int(spec["scale"]), required_codepoints)
        else:
            raise ValueError(f"unsupported resource type {spec['type']!r}")
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
    seen_font_resources: set[int] = set()
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
        elif resource_type == RESOURCE_TYPES["FONT_BITMAP_A1"]:
            if fmt != RESOURCE_FORMATS["FONT_BITMAP_A1_V1"]:
                raise ValueError("unsupported font format")
            font_info = _inspect_font_payload(payload)
            seen_font_resources.add(resource_id)
        else:
            raise ValueError("unsupported resource type")
        entries.append(
            {
                "resource_id": resource_id,
                "resource_type": resource_type,
                "format": fmt,
                "payload_offset": payload_offset,
                "payload_size": payload_size,
                "payload_crc32": payload_crc32,
                "language_id": language_id,
                "font": font_info if resource_type == RESOURCE_TYPES["FONT_BITMAP_A1"] else None,
            }
        )
    for language, resource_id in REQUIRED_LANGUAGE_RESOURCE_IDS.items():
        if seen_language_resources.get(LANGUAGE_IDS[language]) != resource_id:
            raise ValueError(f"missing required {language} text table")
    for resource_id in REQUIRED_FONT_RESOURCE_IDS:
        if resource_id not in seen_font_resources:
            raise ValueError(f"missing required font resource 0x{resource_id:08X}")
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


def _inspect_font_payload(payload: bytes) -> dict[str, object]:
    if len(payload) < FONT_HEADER_SIZE:
        raise ValueError("font payload too small")
    magic, version, header_size, glyph_count, record_size, ascent, descent, line_height, reserved0, index_offset, bitmap_offset, index_crc, flags = struct.unpack(
        "<IHHHHbbBBIIII", payload[:FONT_HEADER_SIZE]
    )
    index_size = glyph_count * FONT_RECORD_SIZE
    if (
        magic != FONT_MAGIC
        or version != FONT_VERSION
        or header_size != FONT_HEADER_SIZE
        or record_size != FONT_RECORD_SIZE
        or glyph_count == 0
        or ascent <= 0
        or descent < 0
        or line_height <= 0
        or reserved0 != 0
        or flags != 0
        or index_offset != FONT_HEADER_SIZE
        or bitmap_offset != FONT_HEADER_SIZE + index_size
        or bitmap_offset > len(payload)
    ):
        raise ValueError("invalid font header")
    index = payload[index_offset:bitmap_offset]
    if len(index) != index_size or crc32(index) != index_crc:
        raise ValueError("font index CRC mismatch")
    previous = -1
    bitmap_cursor = 0
    required = {ord(" "), ord("?")}
    max_width = 0
    max_height = 0
    first_codepoint = None
    last_codepoint = None
    for i in range(glyph_count):
        record = index[i * FONT_RECORD_SIZE : (i + 1) * FONT_RECORD_SIZE]
        codepoint, bitmap_rel, bitmap_size, width, height, advance, bearing_x, bearing_y, row_stride, reserved = struct.unpack(
            "<IIHBBbbbBH", record[:18]
        )
        if record[18:] != b"\x00\x00":
            raise ValueError("font record padding is nonzero")
        if (
            not _unicode_scalar_valid(codepoint)
            or codepoint <= previous
            or reserved != 0
            or advance <= 0
            or bearing_y < 0
            or bearing_y > ascent
            or width > FONT_MAX_WIDTH
            or height > FONT_MAX_HEIGHT
        ):
            raise ValueError("invalid font glyph record")
        previous = codepoint
        first_codepoint = codepoint if first_codepoint is None else first_codepoint
        last_codepoint = codepoint
        required.discard(codepoint)
        if width == 0 or height == 0:
            if width != 0 or height != 0 or row_stride != 0 or bitmap_size != 0 or bitmap_rel != 0:
                raise ValueError("invalid blank font glyph")
            continue
        expected_stride = (width + 7) // 8
        if row_stride != expected_stride or bitmap_size != expected_stride * height:
            raise ValueError("invalid font bitmap size")
        if bitmap_rel != bitmap_cursor or bitmap_offset + bitmap_rel + bitmap_size > len(payload):
            raise ValueError("font glyph bitmap out of bounds")
        bitmap_cursor += bitmap_size
        max_width = max(max_width, width)
        max_height = max(max_height, height)
    if required:
        raise ValueError("font missing SPACE or '?'")
    if bitmap_offset + bitmap_cursor != len(payload):
        raise ValueError("font bitmap contains unreferenced bytes")
    return {
        "glyph_count": glyph_count,
        "ascent": ascent,
        "descent": descent,
        "line_height": line_height,
        "first_codepoint": first_codepoint,
        "last_codepoint": last_codepoint,
        "max_width": max_width,
        "max_height": max_height,
        "index_bytes": index_size,
        "bitmap_bytes": bitmap_cursor,
        "index_crc32": index_crc,
    }
