"""Real C installation parser/service/NOR state machine fault injection."""
import argparse
import json
import os
from pathlib import Path
import struct
import sys
import tempfile
import unittest
import zlib
sys.path.insert(0,str(Path(__file__).resolve().parents[2]/'tools'))
import pc_osl
import pc_osl_synthetic
import pc_osl_install as install
from pc_cal_capture import Client,FakeSerial,CaptureError
import inspect_calibration_record as wire

BRIDGE=os.environ.get('WTK_CAPTURE_BRIDGE')
DOCUMENT=pc_osl_synthetic.campaign()


def frame(sequence):return pc_osl.build_candidate(DOCUMENT,sequence)[0]
def crc_frame(data):
    struct.pack_into('<I',data,56,wire.crc_frame(data,len(data)-64))
    return bytes(data)


@unittest.skipUnless(BRIDGE,'C fixture required')
class ProvisioningTests(unittest.TestCase):
    def setUp(self):self.open()
    def open(self):
        self.serial=FakeSerial(BRIDGE);self.serial.read_ticks=1
        self.client=Client(self.serial,first_id=1);self.client.identify();self.installer=install.Installer(self.client)
    def tearDown(self):self.serial.close()
    def seed(self,slot,data):
        for offset in range(0,len(data),128):
            self.serial.control(5,struct.pack('<BH',slot,offset)+data[offset:offset+128])
    def reset(self,partial=0):
        self.serial.control(4,struct.pack('<H',partial));self.serial.pending.clear()
        self.client=Client(self.serial,first_id=1);self.client.identify();self.installer=install.Installer(self.client)
    def debug(self):return struct.unpack('<4B4I',self.serial.control(6,b''))
    def commit_raw(self,transaction):
        from pc_link_resource_pack import encode_frame
        request=self.client.next_id;self.client.next_id+=1
        payload=struct.pack('<BBHII',1,install.COMMIT,0,request,transaction)
        self.serial.write(encode_frame(install.COMMIT,request&65535,payload))
    def ready(self,data):
        transaction=self.installer.begin(data);self.installer.transfer(transaction,data);self.installer.validate(transaction)
        return transaction
    def assert_old(self,sequence=1):
        self.assertEqual(self.client.status()['calibration_sequence'],sequence)
        self.assertEqual(self.installer.readback(sequence),frame(sequence))

    def test_complete_blank_capture_install_reboot_standalone(self):
        with tempfile.TemporaryDirectory() as directory:
            result=install.provision_simulation(BRIDGE,directory)
            self.assertEqual((result['captures'],result['conditions']),(99,33))
            self.assertTrue(result['reboot_verified']);self.assertFalse(result['physically_qualified'])
            self.assertEqual(result['candidate_sha256'],result['readback_sha256'])

    def test_existing_two_slots_replay_and_rollover(self):
        self.seed(0,frame(1));self.seed(1,frame(2));self.reset()
        self.installer.install(frame(3));self.reset()
        self.assert_old(3)
        with self.assertRaisesRegex(CaptureError,'successor'):self.installer.begin(frame(2))
        self.seed(0,frame(0xffffffff));self.seed(1,b'\xff'*4096);self.reset()
        self.assertEqual(self.installer.status()['next_sequence'],0)
        with self.assertRaisesRegex(CaptureError,'rollover'):self.installer.begin(frame(1))

    def test_corrupt_newest_and_incomplete_newest_preserve_older(self):
        for mode in ('numerical','incomplete','crc','model'):
            with self.subTest(mode=mode):
                damaged=bytearray(frame(2))
                if mode=='numerical':struct.pack_into('<f',damaged,142,float('nan'));damaged=bytearray(crc_frame(damaged))
                elif mode=='incomplete':
                    damaged=damaged[:-80];struct.pack_into('<H',damaged,64,32)
                    struct.pack_into('<H',damaged,10,len(damaged)-64);damaged=bytearray(crc_frame(damaged))
                elif mode=='model':struct.pack_into('<H',damaged,20,99);damaged=bytearray(crc_frame(damaged))
                else:damaged[200]^=1
                self.seed(0,frame(1));self.seed(1,b'\xff'*4096);self.seed(1,damaged);self.reset()
                self.assert_old()
                transaction=self.ready(frame(2));self.commit_raw(transaction)
                self.serial.advance(1)
                self.assertEqual(self.debug()[1],1)  # Never erase usable A.
                self.reset(128);self.assert_old()

    def test_full_transition_and_partial_nor_reset_sweep(self):
        # Probe all cooperative states/offsets using one-millisecond steps, then replay cuts.
        self.seed(0,frame(1));self.reset()
        transaction=self.ready(frame(2));self.commit_raw(transaction)
        points=[]
        for tick in range(140):
            state=self.debug();points.append((tick,state))
            if state[0]==0 and state[-1]==2:break
            self.serial.advance(1)
        self.assertEqual(points[-1][1][-1],2)
        visited={row[0] for _,row in points}
        self.assertTrue({1,2,3,4,5,6,7,8,9,12}.issubset(visited),visited)
        count=0
        for tick,state in points[:-1]:
            amounts={0,state[5]//2,state[5]} if state[2] else {0}
            if state[2]==2 and state[5]==4:amounts={0,1,2,3,4}
            for amount in sorted(amounts):
                self.serial.close();self.open();self.seed(0,frame(1));self.reset()
                transaction=self.ready(frame(2));self.commit_raw(transaction);self.serial.advance(tick)
                self.reset(amount)
                recovered=self.client.status()['calibration_sequence']
                self.assertIn(recovered,(1,2),(tick,state,amount))
                self.assertEqual(self.installer.readback(recovered),frame(recovered))
                count+=1
        print(f'C-backed reset sweep: {len(points)} transition points, {count} partial-NOR cuts; states={sorted(visited)}')

    def test_invalid_candidates_do_not_issue_flash_operations(self):
        original=frame(1)
        mutations=[]
        for offset,value,fmt in ((6,99,'H'),(16,99,'I'),(20,99,'H'),(142,float('nan'),'f'),
                                 (138,0x320,'I'),(138,0x80000220,'I'),(134,0,'I'),(72,0,'f'),(64,32,'H')):
            data=bytearray(original);struct.pack_into('<'+fmt,data,offset,value);mutations.append(crc_frame(data))
        data=bytearray(original);struct.pack_into('<2f',data,174,0,0);mutations.append(crc_frame(data))
        data=bytearray(original);data[200]^=1;mutations.append(bytes(data))
        data=bytearray(original);data[200:280]=data[120:200];mutations.append(crc_frame(data))
        for index,candidate in enumerate(mutations):
            transaction=self.client.command(install.BEGIN,struct.pack('<III',2760,zlib.crc32(candidate),1))[0]
            self.installer.transfer(transaction,candidate)
            with self.assertRaises(CaptureError,msg=f'mutation {index}'):self.installer.validate(transaction)
            self.assertEqual(self.debug()[6],0)

    def test_blank_flash_interruptions_never_activate_partial_frame(self):
        for target in (2, 4, 6, 12, 8, 9):
            for partial in (0, 1, 2, 3, 4):
                self.serial.close();self.open()
                transaction=self.ready(frame(1));self.commit_raw(transaction)
                for _ in range(140):
                    if self.debug()[0]==target:break
                    self.serial.advance(1)
                self.assertEqual(self.debug()[0],target)
                self.reset(partial)
                recovered=self.client.status()['calibration_sequence']
                self.assertIn(recovered,(0,1))
                if recovered:
                    self.assertEqual(self.installer.readback(1),frame(1))
                else:
                    self.assertFalse(self.client.status()['calibrated'])

    def test_transfer_offsets_incomplete_crc_timeout_abort(self):
        data=frame(1);transaction=self.installer.begin(data)
        for offset in (1,2800):
            with self.assertRaisesRegex(CaptureError,'BAD_PAYLOAD'):
                self.client.command(install.CHUNK,struct.pack('<IH',transaction,offset)+data[:96])
        payload=struct.pack('<IH',transaction,0)+data[:96]
        self.client.command(install.CHUNK,payload)
        with self.assertRaisesRegex(CaptureError,'BAD_PAYLOAD'):self.client.command(install.CHUNK,payload)
        with self.assertRaisesRegex(CaptureError,'INVALID_CANDIDATE'):self.installer.validate(transaction)
        self.assertEqual(self.debug()[6],0)
        transaction=self.installer.begin(data);self.installer.abort(transaction)
        self.assertEqual(self.installer.status()['state'],install.ABORTED)
        transaction=self.installer.begin(data);self.serial.advance(21000)
        self.assertEqual(self.installer.status()['error'],10)
        self.assertEqual(self.debug()[3],0)

    def test_capture_install_exclusion_safety_and_flash_failure(self):
        from pc_cal_capture import START
        self.serial.inject(8);capture_id=self.client.begin(pc_osl.KEYS[0],'OPEN')
        with self.assertRaisesRegex(CaptureError,'BUSY'):self.installer.begin(frame(1))
        self.client.cancel(capture_id);self.serial.inject(0)
        transaction=self.installer.begin(frame(1))
        with self.assertRaisesRegex(CaptureError,'BUSY'):self.client.begin(pc_osl.KEYS[0],'OPEN')
        self.installer.abort(transaction)
        for bits in (1,2):
            self.serial.inject(bits)
            with self.assertRaisesRegex(CaptureError,'SAFETY_BLOCKED'):self.installer.begin(frame(1))
        self.serial.inject(0);self.seed(0,frame(1));self.reset()
        for bits in (32,64):
            transaction=self.ready(frame(2));self.serial.inject(bits);self.commit_raw(transaction)
            self.serial.advance(150);self.assertEqual(self.installer.status()['state'],install.FAILED)
            self.reset();self.assert_old()

    def test_abort_during_program_and_after_commit_point(self):
        self.seed(0,frame(1));self.reset()
        transaction=self.ready(frame(2));self.commit_raw(transaction)
        self.serial.advance(2)
        # Discard the already queued COMMIT ACK before issuing ABORT.
        self.serial.advance(20);self.serial.pending.clear()
        self.installer.abort(transaction);self.serial.advance(20);self.reset();self.assert_old()
        transaction=self.ready(frame(2));self.commit_raw(transaction)
        for _ in range(140):
            if self.debug()[0]==8:break
            self.serial.advance(1)
        self.serial.pending.clear()
        with self.assertRaisesRegex(CaptureError,'TOO_LATE'):self.installer.abort(transaction)
        self.serial.advance(10);self.reset();self.assert_old(2)

    def test_disconnection_cooperative_completion_and_flash_timeout(self):
        self.seed(0,frame(1));self.reset()
        transaction=self.ready(frame(2));self.commit_raw(transaction)
        # No further serial requests: writer must progress and finish by itself.
        self.serial.advance(150);self.reset();self.assert_old(2)
        transaction=self.ready(frame(3));self.serial.inject(128);self.commit_raw(transaction)
        self.serial.advance(1500);self.serial.pending.clear()
        self.assertEqual(self.installer.status()['state'],install.FAILED)
        self.assertEqual(self.debug()[3],0)
        self.reset();self.assert_old(2)

    def test_safety_change_during_program_and_oversized_packet(self):
        self.seed(0,frame(1));self.reset()
        transaction=self.ready(frame(2));self.commit_raw(transaction)
        self.serial.advance(1);self.serial.inject(1);self.serial.advance(150)
        self.serial.pending.clear()
        self.assertEqual(self.installer.status()['error'],7)
        self.reset();self.assert_old()
        transaction=self.installer.begin(frame(2))
        # Deliberately bypass the host encoder's payload bound to test the device parser.
        packet=struct.pack('<IBBHHHI',0x31434c50,1,install.CHUNK,0,99,129,0)+bytes(129)
        self.serial.write(packet);self.serial.advance(300)
        self.assertEqual(self.debug()[6],1)  # Only the previous interrupted erase was issued.
        self.installer.abort(transaction);self.assert_old()


class InstallerValidationTests(unittest.TestCase):
    def test_qualified_and_nonfinite_rejected_locally(self):
        for offset,value,fmt in ((138,0x320,'I'),(142,float('inf'),'f')):
            data=bytearray(frame(1));struct.pack_into('<'+fmt,data,offset,value)
            with self.assertRaises(ValueError):install.validate_candidate(crc_frame(data))


if __name__=='__main__':
    parser=argparse.ArgumentParser();parser.add_argument('--bridge');args,rest=parser.parse_known_args()
    BRIDGE=args.bridge or BRIDGE;ProvisioningTests.__unittest_skip__=not bool(BRIDGE)
    unittest.main(argv=[sys.argv[0]]+rest)
