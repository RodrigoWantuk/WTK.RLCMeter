import importlib.util
import json
from pathlib import Path
import sys
import tempfile
import unittest


TOOLS = Path(__file__).resolve().parents[2] / "tools"
SPEC = importlib.util.spec_from_file_location("resource_pack_format", TOOLS / "resource_pack_format.py")
resource_pack_format = importlib.util.module_from_spec(SPEC)
assert SPEC.loader is not None
sys.modules[SPEC.name] = resource_pack_format
SPEC.loader.exec_module(resource_pack_format)


class ResourcePackTests(unittest.TestCase):
    def test_repo_manifest_builds_deterministically_and_inspects(self):
        manifest = Path(__file__).resolve().parents[2] / "assets" / "resource_manifest.json"
        first = resource_pack_format.build_pack(manifest)
        second = resource_pack_format.build_pack(manifest)
        self.assertEqual(first, second)
        info = resource_pack_format.inspect_pack(first)
        self.assertEqual(info["schema_version"], 2)
        self.assertEqual(info["resource_api_version"], 1)
        self.assertEqual(info["entry_count"], 2)
        self.assertEqual([entry["resource_id"] for entry in info["entries"]], [0x00010001, 0x00010002])

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


if __name__ == "__main__":
    unittest.main()
