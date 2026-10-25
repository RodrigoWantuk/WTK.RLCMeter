import importlib.util
from pathlib import Path
import struct
import sys
import tempfile
import unittest
from unittest.mock import patch
from types import SimpleNamespace


TOOLS = Path(__file__).resolve().parents[2] / "tools"
RESOURCE_SPEC = importlib.util.spec_from_file_location("resource_pack_format", TOOLS / "resource_pack_format.py")
resource_pack_format = importlib.util.module_from_spec(RESOURCE_SPEC)
assert RESOURCE_SPEC.loader is not None
sys.modules[RESOURCE_SPEC.name] = resource_pack_format
RESOURCE_SPEC.loader.exec_module(resource_pack_format)

PC_SPEC = importlib.util.spec_from_file_location("pc_link_resource_pack", TOOLS / "pc_link_resource_pack.py")
pc_link_resource_pack = importlib.util.module_from_spec(PC_SPEC)
assert PC_SPEC.loader is not None
sys.modules[PC_SPEC.name] = pc_link_resource_pack
PC_SPEC.loader.exec_module(pc_link_resource_pack)


class PcLinkResourcePackTests(unittest.TestCase):
    def test_paced_upload_safe_lines_and_stop_on_missing_ack(self):
        frames = pc_link_resource_pack.iter_resource_frames(b"test pack")

        class Link:
            def __init__(self, **kwargs):
                self.kwargs = kwargs
                self.data = bytearray()
                self.tx = bytearray()
                self.closed = False
            def open(self):
                assert self.kwargs['port'] is None and self.dtr is False and self.rts is False
            def close(self): self.closed = True
            def flush(self): pass
            def write(self, data): self.tx.extend(data); return len(data)
            def read(self, size):
                result = bytes(self.data[:size]); del self.data[:size]; return result

        link = Link(port=None)
        for frame in frames:
            kind, sequence, _ = pc_link_resource_pack.decode_frame(frame)
            status = 2 if kind == pc_link_resource_pack.FRAME_RESOURCE_END else 0
            link.data.extend(pc_link_resource_pack.encode_frame(2, sequence, struct.pack('<HHHH',sequence,0,status,0)))
        with patch.dict(sys.modules, {'serial':SimpleNamespace(Serial=lambda **kw: link)}), patch.object(pc_link_resource_pack.time, 'sleep') as delay:
            report = pc_link_resource_pack.send_serial(frames, 'COM4', 115200, 0)
        self.assertEqual(report['outcome'], 'ALL_FRAMES_ACKNOWLEDGED')
        self.assertEqual(link.tx, b''.join(frames))
        self.assertTrue(link.closed)
        self.assertEqual(delay.call_count, sum(map(len,frames))+1)
        self.assertEqual(delay.call_args_list[0].args, (0.010,))
        self.assertIn(((0.002,), {}), delay.call_args_list)
        link = Link(port=None)
        with tempfile.TemporaryDirectory() as tmp, patch.dict(sys.modules, {'serial':SimpleNamespace(Serial=lambda **kw: link)}):
            path=Path(tmp)/'failure.json'
            with self.assertRaises(TimeoutError):
                pc_link_resource_pack.send_serial(frames, 'COM4', 115200, 0, 0, 0, path)
            self.assertEqual(link.tx, frames[0])
            self.assertIn('STOPPED_INSPECT_BEFORE_RETRY', path.read_text())

    def test_status_rejects_oversized_header_before_payload_read(self):
        class Link:
            def read(self, size):
                self.asserted = size
                return struct.pack('<IBBHHHI',pc_link_resource_pack.MAGIC,1,2,0,1,65535,0)
        with self.assertRaises(ValueError):
            pc_link_resource_pack.read_status_frame(Link())

    def _decode_header(self, frame: bytes):
        return struct.unpack("<IBBHHHI", frame[: pc_link_resource_pack.HEADER_SIZE])

    def test_resource_pack_frames_have_expected_shapes(self):
        pack = bytes(range(250))
        frames = pc_link_resource_pack.iter_resource_frames(pack, chunk_size=100)
        self.assertEqual(len(frames), 5)

        magic, version, frame_type, flags, sequence, length, payload_crc = self._decode_header(frames[0])
        self.assertEqual(magic, pc_link_resource_pack.MAGIC)
        self.assertEqual(version, pc_link_resource_pack.VERSION)
        self.assertEqual(frame_type, pc_link_resource_pack.FRAME_RESOURCE_BEGIN)
        self.assertEqual(flags, 0)
        self.assertEqual(sequence, 1)
        self.assertEqual(length, 12)
        self.assertEqual(payload_crc, pc_link_resource_pack.crc32(frames[0][pc_link_resource_pack.HEADER_SIZE :]))
        total_size, pack_crc, api_version, reserved = struct.unpack("<IIHH", frames[0][16:])
        self.assertEqual(total_size, len(pack))
        self.assertEqual(pack_crc, pc_link_resource_pack.crc32(pack))
        self.assertEqual(api_version, resource_pack_format.PACK_API_VERSION)
        self.assertEqual(reserved, 0)

        chunk_header = frames[2][pc_link_resource_pack.HEADER_SIZE : pc_link_resource_pack.HEADER_SIZE + 8]
        offset, byte_count, reserved = struct.unpack("<IHH", chunk_header)
        self.assertEqual(offset, 100)
        self.assertEqual(byte_count, 100)
        self.assertEqual(reserved, 0)
        self.assertEqual(frames[2][pc_link_resource_pack.HEADER_SIZE + 8 :], pack[100:200])

        _, _, frame_type, _, sequence, length, _ = self._decode_header(frames[-1])
        self.assertEqual(frame_type, pc_link_resource_pack.FRAME_RESOURCE_END)
        self.assertEqual(sequence, 5)
        self.assertEqual(length, 8)

    def test_rejects_invalid_chunk_sizes(self):
        with self.assertRaises(ValueError):
            pc_link_resource_pack.iter_resource_frames(b"abc", chunk_size=0)
        with self.assertRaises(ValueError):
            pc_link_resource_pack.iter_resource_frames(
                b"abc", chunk_size=pc_link_resource_pack.RESOURCE_CHUNK_DATA_BYTES + 1
            )

    def test_writes_deterministic_stream(self):
        pack = b"WTK-resource-pack"
        frames = pc_link_resource_pack.iter_resource_frames(pack, chunk_size=8)
        with tempfile.TemporaryDirectory() as tmp:
            output = Path(tmp) / "pack.wpc"
            pc_link_resource_pack.write_stream(frames, output)
            self.assertEqual(output.read_bytes(), b"".join(frames))

    def test_decode_frame_rejects_bad_crc(self):
        frame = bytearray(pc_link_resource_pack.encode_frame(pc_link_resource_pack.FRAME_RESOURCE_BEGIN, 1, b"abc"))
        frame[-1] ^= 0x55
        with self.assertRaises(ValueError):
            pc_link_resource_pack.decode_frame(bytes(frame))

    def test_reads_status_frame(self):
        payload = struct.pack("<HHHH", 7, pc_link_resource_pack.PC_STATUS_OK, pc_link_resource_pack.UPDATE_STATUS_OK, 3)
        frame = pc_link_resource_pack.encode_frame(pc_link_resource_pack.FRAME_STATUS, 7, payload)

        class FakeLink:
            def __init__(self, data):
                self._data = bytearray(data)

            def read(self, size):
                chunk = bytes(self._data[:size])
                del self._data[:size]
                return chunk

        sequence, pc_status, update_status, update_state = pc_link_resource_pack.read_status_frame(FakeLink(frame))
        self.assertEqual(sequence, 7)
        self.assertEqual(pc_status, pc_link_resource_pack.PC_STATUS_OK)
        self.assertEqual(update_status, pc_link_resource_pack.UPDATE_STATUS_OK)
        self.assertEqual(update_state, 3)

    def test_status_frame_requires_status_type(self):
        frame = pc_link_resource_pack.encode_frame(pc_link_resource_pack.FRAME_RESOURCE_END, 2, b"")

        class FakeLink:
            def __init__(self, data):
                self._data = bytearray(data)

            def read(self, size):
                chunk = bytes(self._data[:size])
                del self._data[:size]
                return chunk

        with self.assertRaises(ValueError):
            pc_link_resource_pack.read_status_frame(FakeLink(frame))


if __name__ == "__main__":
    unittest.main()
