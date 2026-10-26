"""Production C parser/session integration; every result remains synthetic."""
import argparse
import hashlib
import json
import os
from pathlib import Path
import struct
import sys
import tempfile
import unittest
from unittest.mock import patch
import zlib

sys.path.insert(0,str(Path(__file__).resolve().parents[2]/"tools"))
import pc_cal_capture as capture
import pc_osl
from pc_link_resource_pack import encode_frame

BRIDGE = os.environ.get("WTK_CAPTURE_BRIDGE")


@unittest.skipUnless(BRIDGE,"C fixture supplied by CTest --bridge")
class SerialIntegration(unittest.TestCase):
    def setUp(self):
        self.serial=capture.FakeSerial(BRIDGE)
        self.client=capture.Client(self.serial,first_id=1,timeout=1)
        self.key=pc_osl.Key(1000,1000,100)

    def tearDown(self):
        self.serial.close()

    def test_blank_identification(self):
        identity=self.client.identify()
        self.assertTrue(identity["synthetic"])
        self.assertEqual(identity["adc_provenance"],"NOMINAL_3V3_NOT_CALIBRATED")
        self.assertFalse(self.client.status()["calibrated"])

    def test_legacy_status_does_not_export_battery(self):
        self.serial.control(11,struct.pack("<BH",0,4000))
        identity=self.client.identify()
        self.assertFalse(identity["capabilities"] & 32)
        self.assertNotIn("battery_voltage_v",self.client.status())
        _,body=self.client.command(capture.STATUS)
        self.assertEqual(body[30:],b"\x00\x00")

    def test_coherent_capture_physical_counts_and_legacy_rejection(self):
        self.serial.control(12,b'\x01')
        self.client.timeout=10
        identity=self.client.identify()
        self.assertTrue(identity['capabilities'] & 64)
        self.assertEqual(identity['timeout_ms'],90000)
        key=pc_osl.Key(1000000,10000,100)
        row=self.client.capture(key,'LOAD',dict(id='SYNTHETIC_1M',
            impedance_ohms=[1000000,0],tolerance_pct=1))
        self.assertEqual(len(bytes.fromhex(row['artifact_hex'])),196)
        self.assertEqual(row['acquisition']['blocks_per_group'],96)
        self.assertEqual(row['acquisition']['accepted_physical_blocks'],576)
        self.assertEqual(row['acquisition']['physical_attempts'],576)
        self.assertEqual(row['acquisition']['accepted'],6)
        self.assertTrue(row['quality']['hg_observed'])
        corrupt=bytearray.fromhex(row['artifact_hex'])
        struct.pack_into('<H',corrupt,186,575)
        struct.pack_into('<I',corrupt,192,zlib.crc32(corrupt[:192]))
        with self.assertRaisesRegex(capture.CaptureError,'integration provenance'):
            capture.observation(bytes(corrupt),identity,row['capture_id'],key,'LOAD',row['reference'])
        legacy=dict(identity,capabilities=identity['capabilities'] & ~64)
        with self.assertRaises(capture.CaptureError):
            capture.observation(bytes.fromhex(row['artifact_hex']),legacy,row['capture_id'],key,'LOAD',row['reference'])

    def test_coherent_cancel_partial_group_discards_result(self):
        self.serial.control(12,b'\x01')
        self.client.identify()
        identity=self.client.begin(pc_osl.Key(1000000,10000,100),'OPEN')
        self.serial.advance(100)
        pending,physical,accepted,_=struct.unpack('<4H',self.serial.control(13,b''))
        self.assertGreater(pending,0)
        self.assertLess(pending,96)
        self.assertGreater(physical,0)
        self.assertEqual(accepted,0)
        self.client.cancel(identity)
        state=self.client.status()
        self.assertFalse(state['busy'])
        self.assertFalse(state['result_valid'])
        self.assertTrue(state['transfer_safe'])

    def test_coherent_charger_abort_discards_partial_mean(self):
        self.serial.control(12,b'\x01')
        self.client.identify()
        self.client.begin(pc_osl.Key(1000000,10000,100),'OPEN')
        self.serial.advance(100)
        self.serial.inject(1)
        self.serial.advance(100)
        state=self.client.status()
        self.assertFalse(state['busy'])
        self.assertFalse(state['result_valid'])
        self.assertTrue(state['transfer_safe'])

    def test_coherent_timeout_drains_hardware(self):
        self.serial.control(12,b'\x01')
        self.client.identify()
        self.serial.inject(8)
        self.client.begin(pc_osl.Key(1000000,10000,100),'OPEN')
        self.serial.advance(50000)
        self.serial.advance(40100)
        state=self.client.status()
        self.assertFalse(state['busy'])
        self.assertFalse(state['result_valid'])
        self.assertTrue(state['transfer_safe'])
        self.assertEqual(state['capture_status'],10)

    def test_coherent_quiet_windows_are_condition_specific(self):
        client=capture.Client(object(),timeout=25)
        client.identity=dict(capabilities=68,timeout_ms=90000)
        self.assertEqual(client.capture_quiet_seconds(pc_osl.Key(1000000,10000,100)),91.0)
        self.assertEqual(client.capture_quiet_seconds(pc_osl.Key(1000000,10000,500)),91.0)
        self.assertEqual(client.capture_quiet_seconds(pc_osl.Key(100000,10000,100)),91.0)
        self.assertEqual(client.capture_quiet_seconds(pc_osl.Key(100000,10000,500)),21.0)
        self.assertEqual(client.capture_quiet_seconds(pc_osl.Key(1000,1000,100)),21.0)
        with self.assertRaisesRegex(capture.CaptureError,'quiet window'):
            client.capture(pc_osl.Key(1000000,10000,100),'OPEN')

    def test_battery_status_readable_while_charger_blocks_capture(self):
        self.serial.control(11,struct.pack("<BH",1,4050))
        identity=self.client.identify()
        self.assertTrue(identity["capabilities"] & 32)
        self.serial.inject(1)
        state=self.client.status()
        self.assertEqual(state["battery_voltage_v"],4.05)
        self.assertTrue(state["safety_blocks"] & 1)
        self.assertTrue(state["transfer_safe"])
        self.assertFalse(state["busy"])
        self.assertEqual(state["capture_id"],0)

    def test_battery_unavailable_is_not_zero_volts(self):
        self.serial.control(11,struct.pack("<BH",1,0))
        self.client.identify()
        self.assertIsNone(self.client.status()["battery_voltage_v"])

    def test_out_of_range_battery_telemetry_rejected(self):
        self.serial.control(11,struct.pack("<BH",1,65000))
        self.client.identify()
        with self.assertRaisesRegex(capture.CaptureError,"battery telemetry"):
            self.client.status()

    def test_complete_campaign_and_resume(self):
        with tempfile.TemporaryDirectory() as directory:
            campaign=Path(directory)/"captures.json"; candidate=Path(directory)/"candidate.bin"
            result=capture.simulate(BRIDGE,campaign,candidate)
            self.assertEqual((result["captures"],result["conditions"],result["candidate_bytes"]),(99,33,2760))
            pc_osl.validate_frame(candidate.read_bytes())
            before=campaign.read_bytes()
            capture.simulate(BRIDGE,campaign,candidate)
            self.assertEqual(before,campaign.read_bytes())
            for row in json.loads(before)["captures"]:
                self.assertFalse(row["physically_qualified"])
                self.assertEqual(hashlib.sha256(bytes.fromhex(row["artifact_hex"])).hexdigest(),row["sha256"])

    def test_invalid_command_payload_condition_and_sequence(self):
        for command,body,error in ((0x5f,b"","BAD_COMMAND"),(capture.IDENTIFY,b"x","BAD_PAYLOAD"),
                                   (capture.START,struct.pack("<4B2f",0,0,1,0,0,0),"UNSUPPORTED")):
            with self.assertRaisesRegex(capture.CaptureError,error):self.client.command(command,body)
        self.client.next_id=1
        with self.assertRaisesRegex(capture.CaptureError,"STALE_REQUEST"):self.client.identify()
        request=encode_frame(capture.IDENTIFY,7,struct.pack("<BBHI",1,capture.IDENTIFY,0,8))
        self.serial.write(request)
        _,_,response=self.client.frame(__import__('time').monotonic()+1)
        self.assertEqual(struct.unpack_from("<H",response,8)[0],1)

    def test_partial_crc_and_receiver_timeout(self):
        packet=encode_frame(capture.START,1,struct.pack("<BBHI4B2f",1,capture.START,0,1,2,0,0,0,0,0))
        self.serial.write(packet[:20]);self.serial.advance(300);self.serial.write(packet[20:])
        self.assertFalse(self.client.status()["busy"])
        corrupt=bytearray(packet);corrupt[-1]^=1;self.serial.write(corrupt)
        self.assertFalse(self.client.status()["busy"])
        self.assertGreaterEqual(self.client.status()["protocol_errors"],1)

    def test_charger_and_residual_rejection(self):
        for injection in (1,2):
            self.serial.inject(injection)
            with self.assertRaisesRegex(capture.CaptureError,"SAFETY_BLOCKED"):
                self.client.begin(self.key,"OPEN")
            self.assertFalse(self.client.status()["result_valid"])

    def test_acquisition_error(self):
        self.serial.inject(4)
        with self.assertRaisesRegex(capture.CaptureError,"ACQUISITION_ERROR"):
            self.client.capture(self.key,"OPEN")
        self.assertFalse(self.client.status()["result_valid"])

    def test_post_capture_refresh_timeout_stays_safe_and_invalid(self):
        self.serial.control(10, struct.pack('<H', 25000))
        with self.assertRaisesRegex(capture.CaptureError, 'TIMEOUT'):
            self.client.capture(self.key, 'OPEN')
        state = self.client.status()
        self.assertFalse(state['busy'])
        self.assertFalse(state['result_valid'])
        self.assertTrue(state['transfer_safe'])
        self.assertFalse(state['calibrated'])

    def test_cancel_during_post_capture_refresh(self):
        self.serial.control(10, struct.pack('<H', 5000))
        identity = self.client.begin(self.key, 'OPEN')
        self.serial.advance(100)
        self.client.cancel(identity)
        state = self.client.status()
        self.assertFalse(state['busy'])
        self.assertFalse(state['result_valid'])
        self.assertTrue(state['transfer_safe'])
        self.assertEqual(state['capture_status'], 9)

    def test_interlock_during_acquisition_and_unsupported_clock(self):
        self.serial.inject(8)
        self.client.begin(self.key,"OPEN")
        self.serial.inject(1)
        self.serial.advance(100)
        self.assertFalse(self.client.status()["result_valid"])
        self.assertTrue(self.client.status()["transfer_safe"])
        self.serial.inject(16)
        with self.assertRaisesRegex(capture.CaptureError,"UNSUPPORTED"):
            self.client.begin(self.key,"OPEN")

    def test_reconnect_negotiates_stale_id_and_tampered_campaign(self):
        self.client.next_id=0x40000001
        self.client.identify()
        reconnect=capture.Client(self.serial)
        reconnect.identify()
        self.assertGreater(reconnect.next_id,0x40000001)
        row=reconnect.capture(self.key,"OPEN")
        with tempfile.TemporaryDirectory() as directory:
            path=Path(directory)/"campaign.json";capture.save_capture(path,row)
            document=json.loads(path.read_text());document["captures"][0]["sha256"]="0"*64
            with self.assertRaisesRegex(capture.CaptureError,"artifact"):
                capture.verify_campaign(document,row["device"])

    def test_cancel_busy_and_timeout(self):
        self.serial.inject(8)
        identity=self.client.begin(self.key,"OPEN")
        with self.assertRaisesRegex(capture.CaptureError,"BUSY"):self.client.begin(self.key,"SHORT")
        self.client.cancel(identity)
        state=self.client.status()
        self.assertTrue(state["transfer_safe"]);self.assertFalse(state["result_valid"])
        self.assertEqual(state["capture_status"],9)
        identity=self.client.begin(self.key,"OPEN")
        self.serial.advance(21000)
        state=self.client.status()
        self.assertEqual(state["capture_id"],identity)
        self.assertEqual(state["capture_status"],10);self.assertFalse(state["result_valid"])

    def test_observation_condition_crc_and_duplicate(self):
        row=self.client.capture(self.key,"OPEN")
        data=bytearray.fromhex(row["artifact_hex"])
        data[18]=3;struct.pack_into("<I",data,184,zlib.crc32(data[:184]))
        with self.assertRaisesRegex(capture.CaptureError,"condition"):
            capture.observation(bytes(data),row["device"],row["capture_id"],self.key,"OPEN")
        data[-1]^=1
        with self.assertRaisesRegex(capture.CaptureError,"CRC"):
            capture.observation(bytes(data),row["device"],row["capture_id"],self.key,"OPEN")
        with tempfile.TemporaryDirectory() as directory:
            path=Path(directory)/"campaign.json";capture.save_capture(path,row)
            before=path.read_bytes()
            with self.assertRaisesRegex(capture.CaptureError,"already contains"):capture.save_capture(path,row)
            self.assertEqual(path.read_bytes(),before)


class ClientTransportTests(unittest.TestCase):
    def test_real_capture_quiet_window_and_cancellation_checkpoint(self):
        clock = [0.0]
        sleeps = []
        class QuietClient(capture.Client):
            checkpoints = 0
            def checkpoint(self):
                self.checkpoints += 1
        client = QuietClient(object())
        client.identity = dict(timeout_ms=20000)
        self.assertEqual(client.capture_quiet_seconds(), 21.0)
        def sleep(seconds):
            sleeps.append(seconds)
            clock[0] += seconds
        with patch.object(capture.time, 'monotonic', side_effect=lambda: clock[0]), \
                patch.object(capture.time, 'sleep', side_effect=sleep):
            client.wait_capture_quiet(client.capture_quiet_seconds())
        self.assertAlmostEqual(sum(sleeps), 21.0)
        self.assertLessEqual(max(sleeps), 0.05)
        self.assertGreater(client.checkpoints, 1)
        def cancel():
            raise KeyboardInterrupt()
        client.checkpoint = cancel
        with self.assertRaises(KeyboardInterrupt):
            client.wait_capture_quiet(21.0)

    def test_short_timeout_rejects_before_start_and_fixture_has_no_wall_wait(self):
        client = capture.Client(object(), timeout=5)
        client.identity = dict(timeout_ms=20000, capabilities=4)
        with self.assertRaisesRegex(capture.CaptureError, 'quiet window'):
            client.capture(pc_osl.Key(1000, 1000, 100), 'OPEN')
        fixture = object.__new__(capture.FakeSerial)
        self.assertEqual(capture.Client(fixture).capture_quiet_seconds(), 0.0)

    def test_interrupted_write(self):
        class Transport:
            def write(self,data):return len(data)-1
        with self.assertRaisesRegex(capture.CaptureError,"interrupted"):
            capture.Client(Transport(),first_id=1).identify()

    def test_stale_response_discard_and_identity_mismatch(self):
        class Transport:
            def write(self,data):return len(data)
            def read(self,size):
                stale=encode_frame(capture.IDENTIFY|0x80,1,struct.pack("<BBHIHH",1,capture.IDENTIFY,0,1,0,0))
                wrong=encode_frame(capture.IDENTIFY|0x80,3,struct.pack("<BBHIHH",1,capture.IDENTIFY,0,3,0,0))
                return stale+wrong
        with self.assertRaisesRegex(capture.CaptureError,"identity"):
            capture.Client(Transport(),first_id=2).identify()


if __name__=="__main__":
    parser=argparse.ArgumentParser();parser.add_argument("--bridge",type=Path)
    args,remaining=parser.parse_known_args();BRIDGE=args.bridge or BRIDGE
    # unittest decorators are evaluated before command-line parsing.
    SerialIntegration.__unittest_skip__=not bool(BRIDGE)
    unittest.main(argv=[sys.argv[0]]+remaining)
