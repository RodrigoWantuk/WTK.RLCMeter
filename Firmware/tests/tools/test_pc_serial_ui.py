import sys
from pathlib import Path
import unittest

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
