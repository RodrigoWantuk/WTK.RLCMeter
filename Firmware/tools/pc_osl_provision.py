"""Offline provisioning adapter. New operation IDs are simulator-only, not firmware APIs."""

from __future__ import annotations

import struct
import zlib

import inspect_calibration_record as wire
from pc_link_resource_pack import encode_frame, decode_frame
from pc_osl import Key, Solution, validate_frame

# Development namespace; deliberately outside existing RESOURCE operation IDs.
OSL_BEGIN, OSL_CHUNK, OSL_END = 0x40, 0x41, 0x42
CHUNK_BYTES = 120


def newer(a, b):
    return a != b and ((a-b)&0xffffffff) < 0x80000000


def transfer_frames(frame):
    validate_frame(frame)
    checksum = zlib.crc32(frame)&0xffffffff
    frames = [encode_frame(OSL_BEGIN, 1, struct.pack("<II", len(frame), checksum))]
    for index, offset in enumerate(range(0, len(frame), CHUNK_BYTES), 2):
        chunk = frame[offset:offset+CHUNK_BYTES]
        frames.append(encode_frame(OSL_CHUNK, index, struct.pack("<IHH", offset, len(chunk), 0)+chunk))
    frames.append(encode_frame(OSL_END, len(frames)+1, struct.pack("<II", len(frame), checksum)))
    return frames


class FakeDevice:
    """Models bounded transfer + NOR A/B; never controls real relays or a COM port.

    The C store remains the authority: header [0,60), payload [64,end), commit [60,64)
    are programmed in that order, then verified. Reset loses staging, not Flash.
    """

    def __init__(self):
        self.slots = [bytearray(b"\xff"*4096), bytearray(b"\xff"*4096)]
        self.safe = True
        self.charger = False
        self.residual_safe = True
        self.reboot()

    def reboot(self):
        self.staging = None
        self.expected_sequence = 1
        self.expected_size = 0
        self.expected_crc = 0
        self.active_slot = None
        self.active_frame = None
        for index, data in enumerate(self.slots):
            try:
                size = 64+struct.unpack_from("<H", data, 10)[0]
                frame = bytes(data[:size])
                summary = validate_frame(frame)
            except (ValueError, struct.error):
                continue
            if self.active_frame is None or newer(summary.sequence, validate_frame(self.active_frame).sequence):
                self.active_slot, self.active_frame = index, frame

    def identify(self):
        return {"device": "SYNTHETIC_FAKE_DEVICE", "hardware": wire.REV1_HARDWARE,
                "schema": 2, "model": 4, "capture_install_dispatch": "SIMULATED_ONLY",
                "active_sequence": validate_frame(self.active_frame).sequence if self.active_frame else 0,
                "qualified": False}

    def capture_standard(self, fixture, key, standard, impedance=None):
        """Simulated factory capture permission, including a blank device.

        This deliberately does not grant normal measurement permission. The real
        firmware still needs a reviewed initial-capture path and ADC provenance.
        """
        if not self.safe or self.charger or not self.residual_safe:
            raise ValueError("simulated safety/idle permissions denied")
        if standard not in ('OPEN', 'SHORT', 'LOAD'):
            raise ValueError("factory capture is limited to OSL standards")
        return fixture.acquire(key, standard, impedance)

    def receive(self, encoded, interrupt_program_at=None):
        frame_type, sequence, payload = decode_frame(encoded)
        # CRC covers the payload in PLC1, not this header. Check semantic fields too.
        if struct.unpack_from("<H", encoded, 6)[0] != 0:
            raise ValueError("unsupported provisioning flags")
        if not self.safe or self.charger or not self.residual_safe:
            raise ValueError("simulated safety/idle permissions denied")
        if sequence != self.expected_sequence:
            raise ValueError("duplicate or out-of-order transfer sequence")
        if frame_type == OSL_BEGIN:
            if self.staging is not None or sequence != 1 or len(payload) != 8:
                raise ValueError("invalid BEGIN")
            size, checksum = struct.unpack("<II", payload)
            if size != 2760:
                raise ValueError("expected complete 33-condition frame")
            self.expected_size, self.expected_crc = size, checksum
            self.staging = bytearray()
        elif frame_type == OSL_CHUNK:
            if self.staging is None or len(payload) < 9:
                raise ValueError("chunk outside active transfer")
            offset, size, reserved = struct.unpack_from("<IHH", payload)
            if (reserved or offset != len(self.staging) or not 1 <= size <= CHUNK_BYTES or
                    len(payload) != 8+size or offset+size > self.expected_size):
                raise ValueError("invalid chunk bounds/offset")
            self.staging.extend(payload[8:])
        elif frame_type == OSL_END:
            if self.staging is None or len(payload) != 8:
                raise ValueError("END outside transfer")
            size, checksum = struct.unpack("<II", payload)
            frame = bytes(self.staging)
            if (size != self.expected_size or len(frame) != size or checksum != self.expected_crc or
                    zlib.crc32(frame)&0xffffffff != checksum):
                raise ValueError("incomplete or corrupt transfer")
            self.install(frame, interrupt_program_at)
            self.staging = None
        else:
            raise ValueError("unsupported simulator operation")
        self.expected_sequence += 1

    def install(self, frame, interrupt_at=None):
        if not self.safe or self.charger or not self.residual_safe:
            raise ValueError("simulated safety/idle permissions denied")
        summary = validate_frame(frame)
        if self.active_frame is not None:
            active_sequence = validate_frame(self.active_frame).sequence
            if active_sequence == 0xffffffff or summary.sequence != active_sequence+1:
                raise ValueError("candidate must use the next nonzero calibration sequence")
        elif summary.sequence != 1:
            raise ValueError("blank device requires sequence 1")
        # Candidate frames cannot self-assert physical qualification.
        if any(struct.unpack_from("<I", frame, 120+i*80+18)[0]&wire.FLAG_QUALIFIED for i in range(33)):
            raise ValueError("ordinary PC candidates cannot assert QUALIFIED")
        target = 0 if self.active_slot is None else 1-self.active_slot
        slot = self.slots[target]
        slot[:] = b"\xff"*4096
        # Include interruption within any individual byte, including the commit marker.
        order = list(range(60))+list(range(64, len(frame)))+list(range(60, 64))
        for operation, offset in enumerate(order):
            if interrupt_at is not None and operation == interrupt_at:
                raise InterruptedError("synthetic power loss during NOR program")
            slot[offset] &= frame[offset]
        readback = bytes(slot[:len(frame)])
        validate_frame(readback)
        if readback != frame:
            raise ValueError("readback mismatch")
        # Activate only after full byte readback + full validation, mirroring the service.
        self.active_slot, self.active_frame = target, readback

    def standalone_measure(self, key: Key, transfer):
        if self.active_frame is None:
            raise ValueError("CALIBRATION_REQUIRED: blank/invalid device cannot publish a result")
        for i in range(33):
            offset = 120+i*80
            if tuple(self.active_frame[offset+6:offset+9]) != key.ids():
                continue
            values = struct.unpack_from("<12f", self.active_frame, offset+22)
            complexes = [complex(values[j], values[j+1]) for j in range(0, 10, 2)]
            solution = Solution(key, *complexes, False, "1X", None, "persisted", 0.0, 0.0)
            return {"z_ohms": solution.apply(transfer), "qualified": False,
                    "evidence": "SYNTHETIC_NOT_PHYSICALLY_QUALIFIED",
                    "sequence": validate_frame(self.active_frame).sequence}
        raise ValueError("missing persisted condition")
