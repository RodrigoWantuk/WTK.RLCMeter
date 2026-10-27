#!/usr/bin/env python3
"""Guarded PRODUCT status/short-OK client; never retries an activation.

115200 8N1, DTR/RTS deasserted. Accepted means a UI event, not a measurement.
Timeout is ambiguous: inspect status/result before making another request.
"""
from __future__ import annotations

import argparse
import json
import math
from pathlib import Path
import struct
import time
import zlib

from pc_bringup import open_serial, utc_now

MAGIC = 0x31434C50
HEADER = struct.Struct("<IBBHHHI")
REQUEST = struct.Struct("<BBHII")
REPLY = struct.Struct("<BBBBIII")
STATUS, OK, RESPONSE = 0x30, 0x31, 0xB0
TELEMETRY, TELEMETRY_RESPONSE, TELEMETRY_BYTES = 0x32, 0xB2, 128
ERRORS = ("ACCEPTED", "BAD_REQUEST", "STALE_REQUEST", "SESSION_MISMATCH",
          "NOT_READY", "SAFETY_BLOCKED", "WOKE_ONLY")


def encode(command: int, request_id: int, expected_session: int) -> bytes:
    if command not in (STATUS, OK, TELEMETRY) or not 0 < request_id <= 0xFFFFFFFF:
        raise ValueError("Invalid command/request ID")
    payload = REQUEST.pack(1, command, 0, request_id, expected_session)
    return HEADER.pack(MAGIC, 1, command, 0, request_id & 0xFFFF,
                       len(payload), zlib.crc32(payload)) + payload


def decode(data: bytes, command: int, request_id: int) -> dict:
    expected = TELEMETRY_BYTES if command == TELEMETRY else REPLY.size
    kind_expected = TELEMETRY_RESPONSE if command == TELEMETRY else RESPONSE
    if len(data) != HEADER.size + expected:
        raise ValueError("Incomplete or ambiguous reply")
    magic, version, kind, flags, sequence, size, crc = HEADER.unpack_from(data)
    payload = data[HEADER.size:]
    if (magic, version, kind, flags, sequence, size, crc) != (
            MAGIC, 1, kind_expected, 0, request_id & 0xFFFF, expected, zlib.crc32(payload)):
        raise ValueError("Reply framing/identity/CRC mismatch")
    api, echoed, error, state, identity, session, blockers = REPLY.unpack(payload[:REPLY.size])
    if (api, echoed, identity) != (1, command, request_id) or error >= len(ERRORS):
        raise ValueError("Reply identity/status mismatch")
    if command == TELEMETRY:
        return decode_telemetry(payload, identity, error, state, session)
    return dict(status=ERRORS[error],ui_state=state,session_sequence=session,
                safety_blocker_flags=blockers,request_id=identity,
                measurement_completion_observed=False)


def decode_telemetry(payload: bytes, identity: int, error: int, state: int, session: int) -> dict:
    def u32(offset): return struct.unpack_from('<I',payload,offset)[0]
    def u16(offset): return struct.unpack_from('<H',payload,offset)[0]
    def number(offset,valid):
        value=struct.unpack_from('<f',payload,offset)[0]
        if not valid: return None
        if not math.isfinite(value): raise ValueError('Valid result contains nonfinite numeric data')
        return value
    valid=u32(48); result=payload[104]
    battery=u16(32) if valid&1 else None
    temperature=struct.unpack_from('<i',payload,40)[0]/1000.0 if valid&2 else None
    if battery==65535 or (valid&2 and struct.unpack_from('<i',payload,40)[0]==-2147483648):
        raise ValueError('Sensor validity conflicts with unavailable sentinel')
    return dict(status=ERRORS[error],request_id=identity,ui_state=state,session_sequence=session,
        uptime_ms=u32(12),safety_blocker_flags=u32(16),fault_mask=u32(20),factory_calibration_sequence=u32(24),
        capability_flags=u32(28),uid=payload[76:88].hex() if valid&128 else None,
        git=payload[64:76].rstrip(b'\0').decode('ascii'),
        battery_mv=battery,battery_v=None if battery is None else battery/1000.0,
        battery_state=payload[34],battery_age_ms=u32(36),charger_state=payload[35],
        ntc_temperature_c=temperature,ntc_age_ms=u32(44),ntc_experimental=bool(valid&4),
        battery_raw=u16(52),ntc_raw=u16(54),uart_errors=u32(56),uart_overflows=u32(60),
        lead_active=bool(valid&32),lead_sequence=u32(108),settings_status=payload[112],settings_error=payload[113],
        k1_commanded_state=payload[114],range_state=payload[115],feedback_dropped=u32(116),feedback_error=payload[120],
        primary_accepted_samples=payload[121],frequency_attempted_mask=payload[122],configured_flash_kib=u16(124),
        last_result=dict(valid=bool(valid&16 and result&1),resistance_ohms=number(88,bool(valid&16 and result&1)),
            reactance_ohms=number(92,bool(valid&16 and result&1)),capacitance_f=number(96,bool(valid&16 and result&2)),
            inductance_h=number(100,bool(valid&16 and result&4)),frequency_id=payload[105],range_id=payload[106],return_channel=payload[107]),
        measurement_completion_observed=bool(valid&16 and result&1 and state==20),
        limitations='Nominal VDDA conversion; loaded engineering OSL and experimental NTC are not independent accuracy qualification')


def exchange(link, command: int, request_id: int, expected_session: int,
             out: Path | None = None, stem: str = "request", pace_ms: float = 0.0) -> tuple[dict, bytes, bytes]:
    if not math.isfinite(pace_ms) or not 0.0 <= pace_ms <= 5.0:
        raise ValueError('Pacing must be 0..5 milliseconds per byte')
    sent = encode(command, request_id, expected_session)
    if out is not None:
        (out/(stem+"-tx.bin")).write_bytes(sent)
    received = bytearray()
    transmitted = bytearray()
    expected = TELEMETRY_BYTES if command == TELEMETRY else REPLY.size
    try:
        chunks = [sent] if pace_ms == 0.0 else [sent[i:i+1] for i in range(len(sent))]
        for chunk in chunks:
            written = link.write(chunk)
            transmitted.extend(chunk[:max(0,written)])
            if written != len(chunk):
                raise RuntimeError("Short write; activation outcome ambiguous; no retry")
            if pace_ms > 0.0: time.sleep(pace_ms/1000.0)
        link.flush()
        deadline = time.monotonic() + 3.0
        while len(received) < HEADER.size + expected and time.monotonic() < deadline:
            received.extend(link.read(HEADER.size + expected - len(received)))
    finally:
        if out is not None:
            (out/(stem+"-rx.bin")).write_bytes(received)
            (out/(stem+"-tx-sent.bin")).write_bytes(transmitted)
    return decode(bytes(received), command, request_id), sent, bytes(received)


def main() -> int:
    parser = argparse.ArgumentParser(description=__doc__)
    parser.add_argument("--port", default="COM4")
    action = parser.add_mutually_exclusive_group()
    action.add_argument("--ok", action="store_true", help="Request one short OK after a fresh status query")
    action.add_argument("--telemetry", action="store_true", help="Read genuine PRODUCT battery/NTC/result diagnostics")
    parser.add_argument('--pace-ms',type=float,default=0.0,help='Optional legacy polled-receiver pacing, 0..5 ms/byte')
    parser.add_argument("--request-id", type=int, default=None)
    parser.add_argument("--out", type=Path, required=True)
    args = parser.parse_args()
    args.out.mkdir(parents=True, exist_ok=False)
    record = dict(utc=utc_now(),port=args.port,activation_requested=args.ok,
                  automatic_retry=False,reset_requested=False,exchanges=[])
    identity = args.request_id if args.request_id is not None else int(time.time()*1000) & 0xFFFFFFFF
    if not 0 < identity < 0xFFFFFFFF:
        raise ValueError("Need two consecutive nonzero increasing request IDs")
    link = open_serial(args.port)
    try:
        link.reset_input_buffer()
        time.sleep(0.3)  # Expire a preceding partial MCU frame without a reset.
        command = TELEMETRY if args.telemetry else STATUS
        status, sent, received = exchange(link, command, identity, 0, args.out, "status",args.pace_ms)
        record["exchanges"].append(status)
        (args.out/"status-tx.bin").write_bytes(sent)
        (args.out/"status-rx.bin").write_bytes(received)
        if status["status"] != "ACCEPTED":
            raise ValueError("Fresh status rejected; no activation sent")
        if args.ok:
            if status["ui_state"] not in (4, 20) or status["safety_blocker_flags"] & ~8:
                raise ValueError("UI/power state cannot admit a short OK; no activation sent")
            reply, sent, received = exchange(link, OK, identity + 1, status["session_sequence"], args.out, "ok",args.pace_ms)
            record["exchanges"].append(reply)
            (args.out/"ok-tx.bin").write_bytes(sent)
            (args.out/"ok-rx.bin").write_bytes(received)
            if reply["status"] not in ("ACCEPTED", "WOKE_ONLY"):
                raise ValueError("Short OK rejected: " + reply["status"])
        record["outcome"] = "REPLIED"
    except Exception as exc:
        record["outcome"] = "FAILED_OR_AMBIGUOUS_NO_RETRY"
        record["error"] = str(exc)
        raise
    finally:
        link.close()
        (args.out/"report.json").write_text(json.dumps(record,indent=2)+"\n",encoding="utf-8")
    print(json.dumps(record,indent=2))
    return 0


if __name__ == "__main__":
    raise SystemExit(main())
