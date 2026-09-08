import importlib.util
import json
from pathlib import Path
import shutil
import sys
import tempfile
import unittest
import struct
from typing import Optional
import re


TOOLS = Path(__file__).resolve().parents[2] / "tools"
SPEC = importlib.util.spec_from_file_location("resource_pack_format", TOOLS / "resource_pack_format.py")
resource_pack_format = importlib.util.module_from_spec(SPEC)
assert SPEC.loader is not None
sys.modules[SPEC.name] = resource_pack_format
SPEC.loader.exec_module(resource_pack_format)


class ResourcePackTests(unittest.TestCase):
    def _write_catalog(self, root: Path, language: str, filename: str, override: Optional[dict[str, str]] = None) -> None:
        strings = {f"0x{text_id:04X}": f"{language[:1].upper()}{text_id:04X}" for text_id in resource_pack_format.REQUIRED_TEXT_IDS}
        if override:
            strings.update(override)
        (root / "text" / filename).write_text(
            json.dumps({"language": language, "strings": strings}),
            encoding="utf-8",
        )

    def _write_font_source(self, root: Path) -> None:
        (root / "font").mkdir(exist_ok=True)
        source = Path(__file__).resolve().parents[2] / "assets" / "font" / "wtk-pixel-base.json"
        shutil.copyfile(source, root / "font" / "wtk-pixel-base.json")

    def _write_manifest(self, root: Path, resources: Optional[list[dict[str, str]]] = None) -> Path:
        manifest = root / "manifest.json"
        manifest.write_text(
            json.dumps(
                {
                    "schema_version": 2,
                    "resources": resources
                    if resources is not None
                    else [
                        {
                            "id": "TEXT_EN",
                            "type": "TEXT_TABLE",
                            "format": "TEXT_TABLE_UTF8_V1",
                            "language": "en",
                            "path": "text/en.json",
                        },
                        {
                            "id": "TEXT_PT_BR",
                            "type": "TEXT_TABLE",
                            "format": "TEXT_TABLE_UTF8_V1",
                            "language": "pt-BR",
                            "path": "text/pt-BR.json",
                        },
                        {
                            "id": "FONT_UI_SMALL",
                            "type": "FONT_BITMAP_A1",
                            "format": "FONT_BITMAP_A1_V1",
                            "path": "font/wtk-pixel-base.json",
                            "scale": 1,
                        },
                        {
                            "id": "FONT_UI_MEDIUM",
                            "type": "FONT_BITMAP_A1",
                            "format": "FONT_BITMAP_A1_V1",
                            "path": "font/wtk-pixel-base.json",
                            "scale": 2,
                        },
                        {
                            "id": "FONT_UI_LARGE",
                            "type": "FONT_BITMAP_A1",
                            "format": "FONT_BITMAP_A1_V1",
                            "path": "font/wtk-pixel-base.json",
                            "scale": 3,
                        },
                    ],
                }
            ),
            encoding="utf-8",
        )
        return manifest

    def _synthetic_pack(self) -> bytes:
        with tempfile.TemporaryDirectory() as tmpdir:
            root = Path(tmpdir)
            (root / "text").mkdir()
            self._write_font_source(root)
            self._write_catalog(root, "en", "en.json")
            self._write_catalog(root, "pt-BR", "pt-BR.json")
            return resource_pack_format.build_pack(self._write_manifest(root))

    def test_python_constants_match_firmware_headers(self):
        src = Path(__file__).resolve().parents[2] / "src"
        resource_header = (src / "storage" / "resource_store.h").read_text(encoding="utf-8")
        text_header = (src / "ui" / "ui_text.h").read_text(encoding="utf-8")
        self.assertRegex(resource_header, r"RESOURCE_PACK_API_VERSION\s*=\s*3u")
        self.assertRegex(text_header, r"UI_TEXT_ID_LAST\s*=\s*UI_TEXT_ID_RANGE")
        self.assertRegex(text_header, r"UI_TEXT_MAX_BYTES\s*=\s*31u")
        last_match = re.search(r"UI_TEXT_ID_RANGE\s*=\s*0x([0-9A-Fa-f]+)u", text_header)
        self.assertIsNotNone(last_match)
        self.assertEqual(int(last_match.group(1), 16), resource_pack_format.TEXT_ID_LAST)

    def test_repo_manifest_builds_deterministically_and_inspects(self):
        manifest = Path(__file__).resolve().parents[2] / "assets" / "resource_manifest.json"
        first = resource_pack_format.build_pack(manifest)
        second = resource_pack_format.build_pack(manifest)
        self.assertEqual(first, second)
        info = resource_pack_format.inspect_pack(first)
        self.assertEqual(info["schema_version"], 2)
        self.assertEqual(info["resource_api_version"], 3)
        self.assertEqual(info["entry_count"], 5)
        self.assertEqual(info["text_id_first"], 0x0001)
        self.assertEqual(info["text_id_last"], 0x0038)
        self.assertEqual(info["text_max_bytes"], 31)
        self.assertEqual(
            [entry["resource_id"] for entry in info["entries"]],
            [0x00010001, 0x00010002, 0x00020001, 0x00020002, 0x00020003],
        )
        font_entries = [entry for entry in info["entries"] if entry["font"] is not None]
        self.assertEqual(len(font_entries), 3)
        for entry in font_entries:
            font = entry["font"]
            self.assertGreaterEqual(font["glyph_count"], 100)
            self.assertEqual(font["index_bytes"], font["glyph_count"] * resource_pack_format.FONT_RECORD_SIZE)
            self.assertLessEqual(font["max_width"], resource_pack_format.FONT_MAX_WIDTH)
            self.assertLessEqual(font["max_height"], resource_pack_format.FONT_MAX_HEIGHT)

    def test_corrupt_header_crc_fails_inspection(self):
        manifest = Path(__file__).resolve().parents[2] / "assets" / "resource_manifest.json"
        data = bytearray(resource_pack_format.build_pack(manifest))
        data[12] ^= 0x01
        with self.assertRaises(ValueError):
            resource_pack_format.inspect_pack(bytes(data))

    def test_empty_string_is_rejected(self):
        with tempfile.TemporaryDirectory() as tmpdir:
            root = Path(tmpdir)
            (root / "text").mkdir()
            self._write_font_source(root)
            (root / "text" / "en.json").write_text(
                json.dumps({"language": "en", "strings": {"0x0001": ""}}),
                encoding="utf-8",
            )
            manifest = root / "manifest.json"
            manifest.write_text(
                json.dumps(
                    {
                        "schema_version": 2,
                        "resources": [
                            {
                                "id": "TEXT_EN",
                                "type": "TEXT_TABLE",
                                "format": "TEXT_TABLE_UTF8_V1",
                                "language": "en",
                                "path": "text/en.json",
                            }
                        ],
                    }
                ),
                encoding="utf-8",
            )
            with self.assertRaises(ValueError):
                resource_pack_format.build_pack(manifest)

    def test_text_must_be_complete_dense_and_bounded(self):
        with tempfile.TemporaryDirectory() as tmpdir:
            root = Path(tmpdir)
            (root / "text").mkdir()
            self._write_font_source(root)
            self._write_catalog(root, "en", "en.json")
            self._write_catalog(root, "pt-BR", "pt-BR.json", {"0x0038": "X" * 32})
            with self.assertRaisesRegex(ValueError, "exceeds"):
                resource_pack_format.build_pack(self._write_manifest(root))

        with tempfile.TemporaryDirectory() as tmpdir:
            root = Path(tmpdir)
            (root / "text").mkdir()
            self._write_font_source(root)
            self._write_catalog(root, "en", "en.json")
            missing = {f"0x{text_id:04X}": "X" for text_id in resource_pack_format.REQUIRED_TEXT_IDS}
            del missing["0x0038"]
            (root / "text" / "pt-BR.json").write_text(
                json.dumps({"language": "pt-BR", "strings": missing}),
                encoding="utf-8",
            )
            with self.assertRaisesRegex(ValueError, "missing required"):
                resource_pack_format.build_pack(self._write_manifest(root))

    def test_manifest_requires_both_languages(self):
        with tempfile.TemporaryDirectory() as tmpdir:
            root = Path(tmpdir)
            (root / "text").mkdir()
            self._write_font_source(root)
            self._write_catalog(root, "en", "en.json")
            manifest = self._write_manifest(
                root,
                [
                    {
                        "id": "TEXT_EN",
                        "type": "TEXT_TABLE",
                        "format": "TEXT_TABLE_UTF8_V1",
                        "language": "en",
                        "path": "text/en.json",
                    }
                ],
            )
            with self.assertRaisesRegex(ValueError, "missing required pt-BR"):
                resource_pack_format.build_pack(manifest)

    def test_payload_and_text_index_corruption_fail_inspection(self):
        data = bytearray(self._synthetic_pack())
        data[-1] ^= 0x01
        with self.assertRaisesRegex(ValueError, "payload CRC"):
            resource_pack_format.inspect_pack(bytes(data))

        data = bytearray(self._synthetic_pack())
        first_payload = struct.unpack_from("<IHHIIIIII", data, resource_pack_format.PACK_HEADER_SIZE)
        payload_offset = first_payload[4]
        index_crc_offset = payload_offset + 20
        struct.pack_into("<I", data, index_crc_offset, 0)
        payload_size = first_payload[5]
        payload_crc = resource_pack_format.crc32(bytes(data[payload_offset : payload_offset + payload_size]))
        struct.pack_into("<I", data, resource_pack_format.PACK_HEADER_SIZE + 20, payload_crc)
        count = struct.unpack_from("<H", data, 16)[0]
        table = bytes(
            data[
                resource_pack_format.PACK_HEADER_SIZE :
                resource_pack_format.PACK_HEADER_SIZE + (count * resource_pack_format.PACK_ENTRY_SIZE)
            ]
        )
        struct.pack_into("<I", data, 28, resource_pack_format.crc32(table))
        check_header = bytearray(data[: resource_pack_format.PACK_HEADER_SIZE])
        struct.pack_into("<I", check_header, 32, 0)
        struct.pack_into("<I", data, 32, resource_pack_format.crc32(bytes(check_header)))
        with self.assertRaisesRegex(ValueError, "text index CRC"):
            resource_pack_format.inspect_pack(bytes(data))

    def test_text_record_escape_and_wrong_language_fail_inspection(self):
        data = bytearray(self._synthetic_pack())
        first_payload = struct.unpack_from("<IHHIIIIII", data, resource_pack_format.PACK_HEADER_SIZE)
        payload_offset = first_payload[4]
        record_offset = payload_offset + resource_pack_format.TEXT_HEADER_SIZE
        struct.pack_into("<HHI", data, record_offset, 0x0001, 4, 0xFFFFFFF0)
        payload_size = first_payload[5]
        payload_crc = resource_pack_format.crc32(bytes(data[payload_offset : payload_offset + payload_size]))
        struct.pack_into("<I", data, resource_pack_format.PACK_HEADER_SIZE + 20, payload_crc)
        count = struct.unpack_from("<H", data, 16)[0]
        table = bytes(
            data[
                resource_pack_format.PACK_HEADER_SIZE :
                resource_pack_format.PACK_HEADER_SIZE + (count * resource_pack_format.PACK_ENTRY_SIZE)
            ]
        )
        struct.pack_into("<I", data, 28, resource_pack_format.crc32(table))
        check_header = bytearray(data[: resource_pack_format.PACK_HEADER_SIZE])
        struct.pack_into("<I", check_header, 32, 0)
        struct.pack_into("<I", data, 32, resource_pack_format.crc32(bytes(check_header)))
        with self.assertRaises(ValueError):
            resource_pack_format.inspect_pack(bytes(data))

        with tempfile.TemporaryDirectory() as tmpdir:
            root = Path(tmpdir)
            (root / "text").mkdir()
            self._write_font_source(root)
            self._write_catalog(root, "pt-BR", "en.json")
            self._write_catalog(root, "pt-BR", "pt-BR.json")
            with self.assertRaisesRegex(ValueError, "language does not match"):
                resource_pack_format.build_pack(self._write_manifest(root))

    def test_font_coverage_and_semantic_corruption_fail(self):
        manifest = Path(__file__).resolve().parents[2] / "assets" / "resource_manifest.json"
        required = resource_pack_format.required_font_codepoints(manifest)
        self.assertIn(ord("Ç"), required)
        self.assertIn(ord("Ã"), required)
        self.assertIn(ord("Ê"), required)
        self.assertIn(ord("Ω"), required)
        self.assertIn(ord("µ"), required)
        self.assertIn(ord("°"), required)

        with tempfile.TemporaryDirectory() as tmpdir:
            root = Path(tmpdir)
            (root / "text").mkdir()
            self._write_font_source(root)
            self._write_catalog(root, "en", "en.json", {"0x0001": "Ω"})
            self._write_catalog(root, "pt-BR", "pt-BR.json")
            source = root / "font" / "wtk-pixel-base.json"
            data = json.loads(source.read_text(encoding="utf-8"))
            del data["glyphs"]["Ω"]
            source.write_text(json.dumps(data), encoding="utf-8")
            with self.assertRaisesRegex(ValueError, "missing required glyphs"):
                resource_pack_format.build_pack(self._write_manifest(root))

        with tempfile.TemporaryDirectory() as tmpdir:
            root = Path(tmpdir)
            (root / "text").mkdir()
            self._write_font_source(root)
            self._write_catalog(root, "en", "en.json")
            self._write_catalog(root, "pt-BR", "pt-BR.json")
            source = root / "font" / "wtk-pixel-base.json"
            data = json.loads(source.read_text(encoding="utf-8"))
            data["glyphs"]["A"] = ["1" * 33]
            source.write_text(json.dumps(data), encoding="utf-8")
            with self.assertRaisesRegex(ValueError, "exceeds"):
                resource_pack_format.build_pack(self._write_manifest(root))

        data = bytearray(resource_pack_format.build_pack(manifest))
        font_entry_offset = resource_pack_format.PACK_HEADER_SIZE + (2 * resource_pack_format.PACK_ENTRY_SIZE)
        font_entry = struct.unpack_from("<IHHIIIIII", data, font_entry_offset)
        payload_offset = font_entry[4]
        index_crc_offset = payload_offset + 24
        struct.pack_into("<I", data, index_crc_offset, 0)
        payload_size = font_entry[5]
        payload_crc = resource_pack_format.crc32(bytes(data[payload_offset : payload_offset + payload_size]))
        struct.pack_into("<I", data, font_entry_offset + 20, payload_crc)
        count = struct.unpack_from("<H", data, 16)[0]
        table = bytes(
            data[
                resource_pack_format.PACK_HEADER_SIZE :
                resource_pack_format.PACK_HEADER_SIZE + (count * resource_pack_format.PACK_ENTRY_SIZE)
            ]
        )
        struct.pack_into("<I", data, 28, resource_pack_format.crc32(table))
        check_header = bytearray(data[: resource_pack_format.PACK_HEADER_SIZE])
        struct.pack_into("<I", check_header, 32, 0)
        struct.pack_into("<I", data, 32, resource_pack_format.crc32(bytes(check_header)))
        with self.assertRaisesRegex(ValueError, "font index CRC"):
            resource_pack_format.inspect_pack(bytes(data))


if __name__ == "__main__":
    unittest.main()
