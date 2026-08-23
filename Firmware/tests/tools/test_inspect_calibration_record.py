import importlib.util
import io
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


def valid_frame():
    payload_len = inspect_cal.SET_PAYLOAD_HEADER_BYTES + inspect_cal.RECORD_BYTES
    total = inspect_cal.HEADER_BYTES + payload_len
    frame = bytearray(total)
    put_u32(frame, 0, inspect_cal.MAGIC)
    put_u16(frame, 4, 1)
    put_u16(frame, 6, inspect_cal.SCHEMA_VERSION)
    put_u16(frame, 8, inspect_cal.HEADER_BYTES)
    put_u16(frame, 10, payload_len)
    put_u32(frame, 12, 7)
    put_u32(frame, 16, 0x00010001)
    put_u16(frame, 20, 2)
    put_u32(frame, inspect_cal.COMMIT_OFFSET, 0xFFFFFFFF)
    payload = inspect_cal.HEADER_BYTES
    put_u16(frame, payload, 1)
    put_u32(frame, payload + 4, 0x00000001)
    struct.pack_into("<" + "f" * 12, frame, payload + 8, *([3.3 / 4095.0, 0.0] * 6))
    rec = payload + inspect_cal.SET_PAYLOAD_HEADER_BYTES
    struct.pack_into("<IHBBBBiII", frame, rec, 0x00010001, 2, 2, 1, 0, 2, 25000, 0x12345678, 0x0000011E)
    floats = [15.468085, 0.0, 1000.0, 0.0, 1.0, 0.0, 0.0, 0.0, 1.0, 0.0, 0.0, 0.0]
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


if __name__ == "__main__":
    unittest.main()
