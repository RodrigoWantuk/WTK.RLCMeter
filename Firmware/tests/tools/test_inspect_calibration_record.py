import importlib.util
import io
import binascii
import struct
import tempfile
import unittest
from contextlib import redirect_stdout
from pathlib import Path


TOOL_PATH = Path(__file__).resolve().parents[2] / "tools" / "inspect_calibration_record.py"
spec = importlib.util.spec_from_file_location("inspect_calibration_record", TOOL_PATH)
inspect_cal = importlib.util.module_from_spec(spec)
assert spec.loader is not None
spec.loader.exec_module(inspect_cal)


def put_u16(buf, off, value):
    struct.pack_into("<H", buf, off, value)


def put_u32(buf, off, value):
    struct.pack_into("<I", buf, off, value)


def valid_frame(full=False):
    keys = ([(r, f, a) for r in range(6) for f in range(3) for a in range(2)
             if not (r == 0 and a == 1)] if full else [(2, 1, 0)])
    payload_len = inspect_cal.SET_PAYLOAD_HEADER_BYTES + len(keys) * inspect_cal.RECORD_BYTES
    total = inspect_cal.HEADER_BYTES + payload_len
    frame = bytearray(total)
    put_u32(frame, 0, inspect_cal.MAGIC)
    put_u16(frame, 4, 1)
    put_u16(frame, 6, inspect_cal.SCHEMA_VERSION)
    put_u16(frame, 8, inspect_cal.HEADER_BYTES)
    put_u16(frame, 10, payload_len)
    put_u32(frame, 12, 7)
    put_u32(frame, 16, 0x00010001)
    put_u16(frame, 20, inspect_cal.MODEL_CURRENT)
    put_u32(frame, inspect_cal.COMMIT_OFFSET, 0xFFFFFFFF)
    payload = inspect_cal.HEADER_BYTES
    put_u16(frame, payload, len(keys))
    put_u32(frame, payload + 4, 0x00000001)
    struct.pack_into("<" + "f" * 12, frame, payload + 8, *([3.3 / 4095.0, 0.0] * 6))
    for index, (range_id, frequency, amplitude) in enumerate(keys):
        rec = payload + inspect_cal.SET_PAYLOAD_HEADER_BYTES + index * inspect_cal.RECORD_BYTES
        rref = inspect_cal.RANGES[range_id]
        key_bytes = struct.pack("<IHBBB3x", 0x00010001, inspect_cal.MODEL_CURRENT,
                                range_id, frequency, amplitude)
        struct.pack_into("<IHBBBBiII", frame, rec, 0x00010001, inspect_cal.MODEL_CURRENT,
                         range_id, frequency, amplitude, 2, 0,
                         binascii.crc32(key_bytes),
                         inspect_cal.FLAG_OSL_MODEL | inspect_cal.FLAG_LOAD_REFERENCE)
        floats = [15.468085, 0.0, float(rref), 0.0, 0.0, 0.0,
                  1.0, 0.0, -float(rref), 0.0, 0.0, 0.0]
        struct.pack_into("<" + "f" * 12, frame, rec + 22, *floats)
    crc = inspect_cal.crc_frame(frame, payload_len)
    put_u32(frame, inspect_cal.CRC_OFFSET, crc)
    put_u32(frame, inspect_cal.COMMIT_OFFSET, inspect_cal.COMMIT_MARKER)
    return bytes(frame)


class InspectCalibrationRecordTests(unittest.TestCase):
    def test_valid_frame_exits_zero(self):
        with tempfile.TemporaryDirectory() as tmp:
            path = Path(tmp) / "cal.bin"
            path.write_bytes(valid_frame())
            with redirect_stdout(io.StringIO()):
                self.assertEqual(inspect_cal.inspect(path), 0)

    def test_crc_failure_exits_nonzero(self):
        blob = bytearray(valid_frame())
        blob[-1] ^= 0x01
        with tempfile.TemporaryDirectory() as tmp:
            path = Path(tmp) / "cal.bin"
            path.write_bytes(blob)
            with redirect_stdout(io.StringIO()):
                self.assertEqual(inspect_cal.inspect(path), 3)

    def test_osl_names_are_exposed(self):
        with tempfile.TemporaryDirectory() as tmp:
            path = Path(tmp) / "cal.bin"
            path.write_bytes(valid_frame())
            output = io.StringIO()
            with redirect_stdout(output):
                self.assertEqual(inspect_cal.inspect(path), 0)
            self.assertIn("osl_t_short=", output.getvalue())
            self.assertIn("osl_k=", output.getvalue())
            self.assertIn("osl_load_reference=", output.getvalue())
            self.assertIn("effective_hg=", output.getvalue())

    def test_strict_full_rev1_frame_and_slot_image(self):
        blob = valid_frame(full=True)
        summary = inspect_cal.decode_full_rev1_frame(blob)
        self.assertEqual(summary.sequence, 7)
        self.assertEqual(len(summary.conditions), 33)
        self.assertEqual(summary, inspect_cal.decode_full_rev1_frame(
            blob + bytes([0xFF]) * (inspect_cal.SLOT_BYTES - len(blob))))

    def test_strict_frame_rejects_partial_tampered_and_invalid_model(self):
        with self.assertRaises(ValueError):
            inspect_cal.decode_full_rev1_frame(valid_frame())
        blob = bytearray(valid_frame(full=True))
        blob[-1] ^= 1
        with self.assertRaises(ValueError):
            inspect_cal.decode_full_rev1_frame(blob)
        blob = bytearray(valid_frame(full=True))
        rec = inspect_cal.HEADER_BYTES + inspect_cal.SET_PAYLOAD_HEADER_BYTES
        put_u32(blob, rec + 18, 0)
        put_u32(blob, inspect_cal.CRC_OFFSET,
                inspect_cal.crc_frame(blob, len(blob) - inspect_cal.HEADER_BYTES))
        with self.assertRaises(ValueError):
            inspect_cal.decode_full_rev1_frame(blob)

    def test_strict_frame_rejects_duplicate_keys_even_with_valid_crc(self):
        blob = bytearray(valid_frame(full=True))
        first = inspect_cal.HEADER_BYTES + inspect_cal.SET_PAYLOAD_HEADER_BYTES
        second = first + inspect_cal.RECORD_BYTES
        blob[second:second + inspect_cal.RECORD_BYTES] = blob[first:first + inspect_cal.RECORD_BYTES]
        put_u32(blob, inspect_cal.CRC_OFFSET,
                inspect_cal.crc_frame(blob, len(blob) - inspect_cal.HEADER_BYTES))
        with self.assertRaises(ValueError):
            inspect_cal.decode_full_rev1_frame(blob)

    def test_strict_frame_rejects_bad_condition_id_and_nonfinite_adc(self):
        blob = bytearray(valid_frame(full=True))
        first = inspect_cal.HEADER_BYTES + inspect_cal.SET_PAYLOAD_HEADER_BYTES
        put_u32(blob, first + 14, 0)
        put_u32(blob, inspect_cal.CRC_OFFSET,
                inspect_cal.crc_frame(blob, len(blob) - inspect_cal.HEADER_BYTES))
        with self.assertRaises(ValueError):
            inspect_cal.decode_full_rev1_frame(blob)
        blob = bytearray(valid_frame(full=True))
        struct.pack_into("<f", blob, inspect_cal.HEADER_BYTES + 8, float("nan"))
        put_u32(blob, inspect_cal.CRC_OFFSET,
                inspect_cal.crc_frame(blob, len(blob) - inspect_cal.HEADER_BYTES))
        with self.assertRaises(ValueError):
            inspect_cal.decode_full_rev1_frame(blob)


if __name__ == "__main__":
    unittest.main()
