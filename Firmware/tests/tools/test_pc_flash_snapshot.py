import sys,struct,unittest,zlib
from pathlib import Path
sys.path.insert(0,str(Path(__file__).resolve().parents[2]/"tools"))
import pc_flash_snapshot as p
class SnapshotCodec(unittest.TestCase):
    def test_codec_and_bounds(self):
        raw=p.encode(10,3,0x3f6000,108)
        self.assertEqual(len(raw),36)
        body=bytearray(128);struct.pack_into('<BBBBIIIHBB',body,0,1,p.COMMAND,0,4,10,3,0x3f6000,108,0,0)
        body[20:]=bytes(range(108))
        reply=p.HEADER.pack(p.MAGIC,1,p.RESPONSE,0,10,128,zlib.crc32(body))+body
        self.assertEqual(p.decode(reply,10,3,0x3f6000,108),bytes(range(108)))
        with self.assertRaises(ValueError):p.decode(reply,10,3,0x3f6001,108)
        with self.assertRaises(ValueError):p.decode(reply[:-1],10,3,0x3f6000,108)
        with self.assertRaises(ValueError):p.encode(10,3,0,109)
    def test_layout(self):
        from storage_layout import resource_limit
        self.assertEqual(resource_limit(4194304),4194304-40960)
        self.assertEqual(resource_limit(8388608),8388608-40960)
        with self.assertRaises(ValueError):resource_limit(1024)
    def test_every_resource_sender_guards_tail(self):
        import pc_link_resource_pack as upload
        with self.assertRaises(ValueError):upload.iter_resource_frames(bytes(2*1024*1024-40960+1))
        begin=upload.encode_frame(upload.FRAME_RESOURCE_BEGIN,1,struct.pack('<IIHH',2*1024*1024,0,4,0))
        end=upload.encode_frame(upload.FRAME_RESOURCE_END,2,struct.pack('<II',2*1024*1024,0))
        with self.assertRaises(ValueError):upload.send_serial([begin,end],"DO_NOT_OPEN",115200,0.0)
if __name__=='__main__':unittest.main()
