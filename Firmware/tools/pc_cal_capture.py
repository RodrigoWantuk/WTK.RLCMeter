#!/usr/bin/env python3
"""USART1 calibration capture client. No calibration installation commands."""
from __future__ import annotations

import argparse
import hashlib
import json
import math
from pathlib import Path
import secrets
import struct
import subprocess
import time
import zlib

import pc_osl
import inspect_calibration_record as wire
from pc_link_resource_pack import encode_frame, decode_frame

IDENTIFY, STATUS, START, RESULT, CANCEL = range(0x50, 0x55)
STANDARDS = ("OPEN", "SHORT", "LOAD")
ERRORS = ("OK", "BAD_FRAME", "BAD_COMMAND", "BAD_PAYLOAD", "STALE_REQUEST", "BUSY",
          "UNSUPPORTED", "SAFETY_BLOCKED", "NOT_READY", "CANCELED", "TIMEOUT", "ACQUISITION_ERROR",
          "INVALID_CANDIDATE", "SEQUENCE_ERROR", "STORAGE_ERROR", "TOO_LATE")


class CaptureError(ValueError):
    pass


class FakeSerial:
    """Host test side channel advances time; all serial frames go to the production C parser."""
    def __init__(self, bridge):
        self.process = subprocess.Popen([str(bridge)], stdin=subprocess.PIPE, stdout=subprocess.PIPE)
        self.pending = bytearray()
        self.read_ticks = 100

    def control(self, command, data):
        self.process.stdin.write(struct.pack("<BH", command, len(data)) + data)
        self.process.stdin.flush()
        size = self.process.stdout.read(2)
        if len(size) != 2:
            raise CaptureError("C fixture terminated")
        result=self.process.stdout.read(struct.unpack("<H", size)[0])
        if command<=3: self.pending.extend(result)
        return result

    def write(self, data):
        for offset in range(0, len(data), 144):
            self.control(1, data[offset:offset+144])
        return len(data)

    def advance(self, milliseconds):
        self.control(2, struct.pack("<H", milliseconds))

    def inject(self, bits):
        self.control(3, bytes([bits]))

    def read(self, size=1):
        if not self.pending:
            self.advance(self.read_ticks)
        result = bytes(self.pending[:size])
        del self.pending[:size]
        return result

    def close(self):
        self.process.stdin.close()
        self.process.wait(timeout=5)
        self.process.stdout.close()


class Client:
    def __init__(self, transport, first_id=None, timeout=25.0):
        self.transport = transport
        # ID lifetime is one device boot. Reconnect may require --first-id > last ID.
        self.next_id = first_id if first_id is not None else secrets.randbelow(0x3fffffff)+1
        self.negotiate_id = first_id is None
        self.timeout = timeout
        self.buffer = bytearray()
        self.identity = None

    def frame(self, deadline):
        while time.monotonic() < deadline:
            while len(self.buffer) >= 4 and self.buffer[:4] != b"PLC1":
                del self.buffer[0]
            if len(self.buffer) >= 16:
                length = struct.unpack_from("<H", self.buffer, 10)[0]
                if self.buffer[4] != 1 or length > 128:
                    del self.buffer[0]
                    continue
                if len(self.buffer) >= 16+length:
                    packet = bytes(self.buffer[:16+length])
                    del self.buffer[:16+length]
                    if struct.unpack_from("<H",packet,6)[0] != 0:
                        raise CaptureError("response reserved flags")
                    return decode_frame(packet)
            data = self.transport.read(144)
            if data:
                self.buffer.extend(data)
                if len(self.buffer) > 512:
                    raise CaptureError("receive buffer exceeded")
        raise TimeoutError("capture response timeout; device must complete safe teardown")

    def command(self, command, body=b""):
        request_id = self.next_id
        pc_osl.integer(request_id, 1, 0xffffffff)
        self.next_id += 1
        payload = struct.pack("<BBHI", 1, command, 0, request_id)+body
        frame = encode_frame(command, request_id & 0xffff, payload)
        if self.transport.write(frame) != len(frame):
            raise CaptureError("interrupted request write")
        deadline = time.monotonic()+self.timeout
        for _ in range(16):
            kind, sequence, response = self.frame(deadline)
            if len(response) < 12:
                raise CaptureError("short response")
            api, echo, reserved, identity, status, reserved2 = struct.unpack_from("<BBHIHH", response)
            if identity < request_id:
                continue  # Bounded discard of a stale reply after cancellation/reconnect.
            if (api, echo, reserved, identity, sequence, kind, reserved2) != (
                    1, command, 0, request_id, request_id & 0xffff, command | 0x80, 0):
                raise CaptureError("response identity/version mismatch")
            if status:
                if status == 4 and command == IDENTIFY and self.negotiate_id and len(response) == 16:
                    self.negotiate_id = False
                    self.next_id = struct.unpack_from("<I",response,12)[0]+1
                    return self.command(command,body)
                raise CaptureError(ERRORS[status] if status < len(ERRORS) else "unknown device error")
            return request_id, response[12:]
        raise CaptureError("too many stale responses")

    def identify(self):
        _, data = self.command(IDENTIFY)
        if len(data) != 60:
            raise CaptureError("invalid identity length")
        hw, model, profile, capabilities, conditions, size, timeout, adc = struct.unpack_from("<IHHIHHIH", data)
        if (hw, model, conditions, size, adc) != (0x00010001, 4, 33, 188, 1):
            raise CaptureError("unsupported hardware/model/capture format")
        self.identity = dict(hardware_revision=hw, model=model, profile=profile,
                             capabilities=capabilities, protocol_version=1, timeout_ms=timeout,
                             adc_provenance="NOMINAL_3V3_NOT_CALIBRATED", synthetic=bool(data[22]),
                             uid=data[24:36].hex(), git=data[36:44].rstrip(b"\0").decode("ascii"),
                             firmware_version=data[44:60].rstrip(b"\0").decode("ascii"))
        return self.identity

    def status(self):
        _, data = self.command(STATUS)
        if len(data) != 32:
            raise CaptureError("invalid status length")
        values = struct.unpack_from("<5Ii", data)
        return dict(capture_id=values[0], calibration_sequence=values[1], safety_faults=values[2],
                    safety_blocks=values[3], protocol_errors=values[4], temperature_mC=values[5],
                    capture_status=data[24], busy=bool(data[25]), result_valid=bool(data[26]),
                    calibrated=bool(data[27]), transfer_safe=bool(data[28]))

    def begin(self, key, standard, load=0j):
        if standard not in STANDARDS:
            raise CaptureError("unsupported standard")
        if standard == "LOAD" and (not math.isfinite(abs(load)) or load.real < 0 or abs(load) <= 1e-5):
            raise CaptureError("invalid known LOAD")
        return self.command(START, struct.pack("<4B2f", *key.ids(), STANDARDS.index(standard),
                                               load.real, load.imag))[0]

    def cancel(self, capture_id):
        self.command(CANCEL, struct.pack("<I", capture_id))

    def capture(self, key, standard, reference=None):
        if self.identity is None:
            self.identify()
        if not self.identity["capabilities"] & 4:
            raise CaptureError("capture capability unavailable")
        load = pc_osl.complex_value(reference["impedance_ohms"]) if standard == "LOAD" else 0j
        if standard == "LOAD":
            tolerance = pc_osl.finite(reference["tolerance_pct"])
            if not 0 < tolerance < 100 or not isinstance(reference["id"],str) or not reference["id"].strip():
                raise CaptureError("LOAD needs explicit tolerance and reference identity")
        capture_id = self.begin(key, standard, load)
        try:
            deadline = time.monotonic()+self.timeout
            while True:
                status = self.status()
                if status["capture_id"] != capture_id:
                    raise CaptureError("capture status identity mismatch")
                if not status["busy"]:
                    break
                if time.monotonic() >= deadline:
                    raise TimeoutError("capture completion timeout")
                if not isinstance(self.transport, FakeSerial):
                    time.sleep(0.01)
            if not status["result_valid"] or status["capture_status"]:
                raise CaptureError(ERRORS[status["capture_status"]])
            artifact = bytearray()
            expected_crc = None
            for offset in (0, 96):
                count = min(96, 188-offset)
                _, part = self.command(RESULT, struct.pack("<IHBB", capture_id, offset, count, 0))
                if len(part) != 12+count:
                    raise CaptureError("invalid result chunk length")
                identity, position, size, crc = struct.unpack_from("<IHHI", part)
                if (identity, position, size) != (capture_id, offset, 188) or (
                        expected_crc is not None and expected_crc != crc):
                    raise CaptureError("result chunk identity mismatch")
                expected_crc = crc
                artifact.extend(part[12:])
            if expected_crc != zlib.crc32(artifact[:184]):
                raise CaptureError("result CRC mismatch")
            return observation(bytes(artifact), self.identity, capture_id, key, standard, reference)
        except (ValueError, TimeoutError, OSError):
            try:
                self.cancel(capture_id)
            except (ValueError, TimeoutError, OSError):
                pass  # Device's independent 20 s timeout still drains its safety FSM.
            raise


def observation(data, identity, capture_id, key, standard, reference=None):
    if len(data) != 188 or zlib.crc32(data[:184]) != struct.unpack_from("<I", data, 184)[0]:
        raise CaptureError("observation length/CRC mismatch")
    magic, api, size, request, hw, model = struct.unpack_from("<IHHIIH", data)
    if (magic, api, size, request, hw, model) != (0x314f4343, 1, 188, capture_id, 0x00010001, 4):
        raise CaptureError("observation identity mismatch")
    if tuple(data[18:22]) != (*key.ids(), STANDARDS.index(standard)):
        raise CaptureError("observation condition mismatch")
    if data[152:164].hex() != identity["uid"] or data[144:152].rstrip(b"\0").decode() != identity["git"]:
        raise CaptureError("observation device/firmware mismatch")
    flags = struct.unpack_from("<H", data, 22)[0]
    faults, blocks, timestamp, temperature = struct.unpack_from("<IIIi", data, 24)
    # RANGE is deliberately disabled after safe teardown; every other blocker rejects the result.
    if flags & 3 != 3 or faults or blocks & ~8 or not flags & 64:
        raise CaptureError("observation unsafe/unstable or unknown ADC provenance")
    if struct.unpack_from("<H", data, 44)[0] != 1 or data[40] < 6:
        raise CaptureError("observation lacks ADC provenance/stable repeats")
    values = struct.unpack_from("<24f", data, 48)
    if not all(math.isfinite(value) for value in values) or any(value <= 0 for value in values[12::2]):
        raise CaptureError("nonfinite phasor/invalid ADC scale")
    item = dict(condition=key.as_json(), standard=standard, stable=True, safe_capture=True,
                temperature_mC=temperature if flags & 32 else None,
                phasors={name:list(values[2*i:2*i+2]) for i,name in enumerate(pc_osl.CHANNELS)},
                quality=dict(ret_1x_valid=bool(flags&4), ret_hg_valid=bool(flags&8), hg_observed=bool(flags&16)),
                evidence="SYNTHETIC_NOT_PHYSICALLY_QUALIFIED" if flags & 512 else "REQUIRES_BENCH_VALIDATION",
                physically_qualified=False, adc_calibrated=False, device=identity,
                capture_id=capture_id, timestamp_ms=timestamp,
                adc=dict(values=list(values[12:]), provenance="NOMINAL_3V3_NOT_CALIBRATED"),
                acquisition=dict(accepted=data[40],rejected=data[41],attempts=data[42],flags=flags,
                                 safety_faults=faults,safety_blocks=blocks,
                                 hardware_error=struct.unpack_from("<I",data,164)[0],
                                 reject_flags=struct.unpack_from("<I",data,168)[0],
                                 permit_issue_ms=struct.unpack_from("<I",data,172)[0],
                                 permit_validate_ms=struct.unpack_from("<I",data,176)[0],
                                 clipping_mask=struct.unpack_from("<I",data,180)[0]),
                artifact_hex=data.hex(), sha256=hashlib.sha256(data).hexdigest())
    if standard == "LOAD":
        item["reference"] = reference
    pc_osl.standard_from_capture(item)
    return item


def verify_campaign(document, identity):
    seen=set()
    for row in document["captures"]:
        key=pc_osl.Key.from_json(row["condition"])
        pair=(key,row["standard"])
        if pair in seen: raise CaptureError("duplicate campaign capture")
        seen.add(pair)
        if row["device"] != identity: raise CaptureError("campaign device/firmware mismatch")
        raw=bytes.fromhex(row["artifact_hex"])
        decoded=observation(raw,identity,row["capture_id"],key,row["standard"],row.get("reference"))
        if row != decoded or hashlib.sha256(raw).hexdigest()!=row["sha256"] or row["adc"]!=document["adc"]:
            raise CaptureError("campaign artifact/metadata mismatch")
    return seen


def save_capture(path, item):
    path = Path(path)
    document = json.loads(path.read_text()) if path.exists() else dict(
        format="WTK_PC_OSL_CAPTURE_V1", adc=item["adc"], captures=[])
    verify_campaign(document,item["device"])
    for old in document["captures"]:
        if old["device"] != item["device"] or old["adc"] != item["adc"]:
            raise CaptureError("campaign device/firmware/ADC mismatch")
        if old["condition"] == item["condition"] and old["standard"] == item["standard"]:
            raise CaptureError("campaign already contains this capture")
    document["captures"].append(item)
    path.parent.mkdir(parents=True, exist_ok=True)
    temporary = path.with_suffix(path.suffix+".tmp")
    temporary.write_text(json.dumps(document, indent=2)+"\n", encoding="utf-8")
    temporary.replace(path)
    return document


def simulate(bridge, campaign_path, candidate_path):
    transport = FakeSerial(bridge)
    try:
        client = Client(transport, first_id=1)
        client.identify()
        if client.status()["calibrated"]:
            raise CaptureError("simulation must start blank")
        if Path(campaign_path).exists():
            verify_campaign(json.loads(Path(campaign_path).read_text()),client.identity)
        for key in pc_osl.KEYS:
            for standard in STANDARDS:
                path = Path(campaign_path)
                if path.exists() and any(row["condition"]==key.as_json() and row["standard"]==standard
                                         for row in json.loads(path.read_text())["captures"]):
                    continue
                reference = dict(id=f"SYNTHETIC_{key.rref_ohms}",
                                 impedance_ohms=[key.rref_ohms,0], tolerance_pct=1.0)
                save_capture(path, client.capture(key, standard, reference))
        document = json.loads(Path(campaign_path).read_text())
        frame, solutions = pc_osl.build_candidate(document, 1)
        Path(candidate_path).write_bytes(frame)
        return dict(evidence="SYNTHETIC_NOT_PHYSICALLY_QUALIFIED", captures=len(document["captures"]),
                    conditions=len(solutions), candidate_bytes=len(frame), installed=False)
    finally:
        transport.close()


def main():
    parser = argparse.ArgumentParser(description=__doc__)
    parser.add_argument("action", choices=("ports","identify","status","capture","cancel","simulate"))
    parser.add_argument("--port")
    parser.add_argument("--first-id",type=int)
    parser.add_argument("--bridge",type=Path)
    parser.add_argument("--campaign",type=Path,default=Path("captures.json"))
    parser.add_argument("--candidate",type=Path,default=Path("candidate.bin"))
    parser.add_argument("--rref",type=int);parser.add_argument("--frequency",type=int)
    parser.add_argument("--amplitude",type=int);parser.add_argument("--standard",choices=STANDARDS)
    parser.add_argument("--load-real",type=float);parser.add_argument("--load-imag",type=float,default=0)
    parser.add_argument("--tolerance-pct",type=float);parser.add_argument("--reference-id")
    parser.add_argument("--capture-id",type=int)
    args=parser.parse_args()
    if args.action=="simulate":
        if not args.bridge: parser.error("--bridge required")
        print(json.dumps(simulate(args.bridge,args.campaign,args.candidate),indent=2));return
    import serial
    from serial.tools import list_ports
    if args.action=="ports":
        print(json.dumps([port.device for port in list_ports.comports()]));return
    ports=[port.device for port in list_ports.comports()]
    port=args.port or (ports[0] if len(ports)==1 else None)
    if not port: parser.error("select --port; automatic selection requires exactly one port")
    with serial.Serial(port,115200,timeout=0.05,write_timeout=1) as transport:
        client=Client(transport,args.first_id)
        identity=client.identify();status=client.status()
        print(json.dumps(dict(device=identity,state="CALIBRATED" if status["calibrated"] else "UNCALIBRATED")))
        if args.action=="identify":return
        if args.action=="status":print(json.dumps(status));return
        if args.action=="cancel":client.cancel(args.capture_id);return
        key=pc_osl.Key(args.rref,args.frequency,args.amplitude)
        reference=dict(id=args.reference_id,impedance_ohms=[args.load_real,args.load_imag],tolerance_pct=args.tolerance_pct)
        existing=json.loads(args.campaign.read_text()) if args.campaign.exists() else {"captures":[]}
        verify_campaign(existing,identity)
        if any(row["condition"]==key.as_json() and row["standard"]==args.standard for row in existing["captures"]):
            print("Already captured; campaign unchanged");return
        item=client.capture(key,args.standard,reference)
        save_capture(args.campaign,item)
        print(json.dumps(dict(capture_id=item["capture_id"],sha256=item["sha256"],evidence=item["evidence"])))


if __name__=="__main__":
    main()
