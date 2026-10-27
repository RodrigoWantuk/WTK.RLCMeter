import sys
from pathlib import Path
import unittest
from unittest.mock import patch
import struct

sys.path.insert(0, str(Path(__file__).resolve().parents[2]/"tools"))
import pc_serial_ui as ui


def reply(command=ui.OK, identity=42, error=0, state=4, session=3):
    payload = ui.REPLY.pack(1, command, error, state, identity, session, 8)
    return ui.HEADER.pack(ui.MAGIC, 1, ui.RESPONSE, 0, identity & 0xFFFF,
                          len(payload), ui.zlib.crc32(payload)) + payload


class Link:
    def __init__(self, data, short=False):
        self.data, self.short, self.writes = data, short, 0

    def write(self, data):
        self.writes += 1
        return len(data)-1 if self.short else len(data)

    def flush(self): pass

    def read(self, count):
        data, self.data = self.data[:count], self.data[count:]
        return data


class SerialUiTests(unittest.TestCase):
    def test_telemetry_wire_values_and_unavailable_data(self):
        payload=bytearray(ui.TELEMETRY_BYTES)
        payload[0:4]=bytes((1,ui.TELEMETRY,0,20))
        for offset,value in ((4,42),(8,3),(12,12345),(16,8),(24,1),(28,31),(36,100),(40,23500),(44,120),(48,155),(56,2),(60,3)):
            struct.pack_into('<I',payload,offset,value)
        struct.pack_into('<H',payload,32,3942)
        payload[64:76]=b'123456789abc'
        payload[76:88]=bytes(range(1,13))
        struct.pack_into('<4f',payload,88,2.0,-16000.0,9.99e-9,float('nan'))
        payload[104]=3
        def framed():
            return ui.HEADER.pack(ui.MAGIC,1,ui.TELEMETRY_RESPONSE,0,42,len(payload),ui.zlib.crc32(payload))+payload
        result=ui.decode(framed(),ui.TELEMETRY,42)
        self.assertEqual(result['battery_mv'],3942)
        self.assertEqual(result['ntc_temperature_c'],23.5)
        self.assertEqual(result['safety_blocker_flags'],8)
        self.assertEqual(result['uid'],bytes(range(1,13)).hex())
        self.assertIsNone(result['last_result']['inductance_h'])
        ui.json.dumps(result,allow_nan=False)
        struct.pack_into('<f',payload,88,float('nan'))
        with self.assertRaises(ValueError): ui.decode(framed(),ui.TELEMETRY,42)
        struct.pack_into('<I',payload,48,0); payload[104]=0
        result=ui.decode(framed(),ui.TELEMETRY,42)
        self.assertIsNone(result['battery_mv'])
        self.assertIsNone(result['ntc_temperature_c'])
        self.assertFalse(result['measurement_completion_observed'])
        ui.json.dumps(result,allow_nan=False)

    def test_paced_transmission_still_never_retries_activation(self):
        link=Link(reply())
        with patch.object(ui.time,'sleep') as sleep:
            ui.exchange(link,ui.OK,42,3,pace_ms=3.0)
        self.assertEqual(link.writes,len(ui.encode(ui.OK,42,3)))
        self.assertEqual(sleep.call_count,link.writes)
        link=Link(reply(),short=True)
        with self.assertRaises(RuntimeError): ui.exchange(link,ui.OK,42,3,pace_ms=3.0)
        self.assertEqual(link.writes,1)
    def test_request_and_reply(self):
        request = ui.encode(ui.OK, 42, 3)
        self.assertEqual(ui.REQUEST.unpack_from(request, ui.HEADER.size), (1,ui.OK,0,42,3))
        result, sent, received = ui.exchange(Link(reply()), ui.OK, 42, 3)
        self.assertEqual(sent, request)
        self.assertEqual(received, reply())
        self.assertEqual(result["status"], "ACCEPTED")
        self.assertFalse(result["measurement_completion_observed"])

    def test_corruption_identity_and_wake(self):
        bad = bytearray(reply()); bad[-1] ^= 1
        for data in [bytes(bad),reply(identity=43),reply(command=ui.STATUS),reply()[:-1],reply(error=99)]:
            with self.assertRaises(ValueError): ui.decode(data,ui.OK,42)
        self.assertEqual(ui.decode(reply(error=6),ui.OK,42)["status"],"WOKE_ONLY")

    def test_never_retry_uncertain_activation(self):
        link = Link(reply(identity=43))
        with self.assertRaises(ValueError): ui.exchange(link,ui.OK,42,3)
        self.assertEqual(link.writes,1)
        link = Link(reply(),short=True)
        with self.assertRaises(RuntimeError): ui.exchange(link,ui.OK,42,3)
        self.assertEqual(link.writes,1)

    def test_request_bounds(self):
        for command, identity in [(ui.OK,0),(ui.OK,0x100000000),(99,42)]:
            with self.assertRaises(ValueError): ui.encode(command,identity,3)


if __name__ == "__main__": unittest.main()
