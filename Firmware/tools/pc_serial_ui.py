#!/usr/bin/env python3
"""Guarded PRODUCT status/short-OK client; never retries an activation.

115200 8N1, DTR/RTS deasserted. Accepted means a UI event, not a measurement.
Timeout is ambiguous: inspect status/result before making another request.
"""
from __future__ import annotations

import argparse
import json
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
ERRORS = ("ACCEPTED", "BAD_REQUEST", "STALE_REQUEST", "SESSION_MISMATCH",
          "NOT_READY", "SAFETY_BLOCKED", "WOKE_ONLY")


def encode(command: int, request_id: int, expected_session: int) -> bytes:
    if command not in (STATUS, OK) or not 0 < request_id <= 0xFFFFFFFF:
        raise ValueError("Invalid command/request ID")
    payload = REQUEST.pack(1, command, 0, request_id, expected_session)
    return HEADER.pack(MAGIC, 1, command, 0, request_id & 0xFFFF,
                       len(payload), zlib.crc32(payload)) + payload


def decode(data: bytes, command: int, request_id: int) -> dict:
    if len(data) != HEADER.size + REPLY.size:
        raise ValueError("Incomplete or ambiguous reply")
    magic, version, kind, flags, sequence, size, crc = HEADER.unpack_from(data)
    payload = data[HEADER.size:]
    if (magic, version, kind, flags, sequence, size, crc) != (
            MAGIC, 1, RESPONSE, 0, request_id & 0xFFFF, REPLY.size, zlib.crc32(payload)):
        raise ValueError("Reply framing/identity/CRC mismatch")
    api, echoed, error, state, identity, session, blockers = REPLY.unpack(payload)
    if (api, echoed, identity) != (1, command, request_id) or error >= len(ERRORS):
        raise ValueError("Reply identity/status mismatch")
    return dict(status=ERRORS[error],ui_state=state,session_sequence=session,
                safety_blocker_flags=blockers,request_id=identity,
                measurement_completion_observed=False)


def exchange(link, command: int, request_id: int, expected_session: int,
             out: Path | None = None, stem: str = "request") -> tuple[dict, bytes, bytes]:
    sent = encode(command, request_id, expected_session)
    if out is not None:
        (out/(stem+"-tx.bin")).write_bytes(sent)
    received = bytearray()
    try:
        if link.write(sent) != len(sent):
            raise RuntimeError("Short write; activation outcome ambiguous; no retry")
        link.flush()
        deadline = time.monotonic() + 3.0
        while len(received) < HEADER.size + REPLY.size and time.monotonic() < deadline:
            received.extend(link.read(HEADER.size + REPLY.size - len(received)))
    finally:
        if out is not None:
            (out/(stem+"-rx.bin")).write_bytes(received)
    return decode(bytes(received), command, request_id), sent, bytes(received)


def main() -> int:
    parser = argparse.ArgumentParser(description=__doc__)
    parser.add_argument("--port", default="COM4")
    parser.add_argument("--ok", action="store_true", help="Request one short OK after a fresh status query")
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
        status, sent, received = exchange(link, STATUS, identity, 0, args.out, "status")
        record["exchanges"].append(status)
        (args.out/"status-tx.bin").write_bytes(sent)
        (args.out/"status-rx.bin").write_bytes(received)
        if status["status"] != "ACCEPTED":
            raise ValueError("Fresh status rejected; no activation sent")
        if args.ok:
            if status["ui_state"] not in (4, 20) or status["safety_blocker_flags"] & ~8:
                raise ValueError("UI/power state cannot admit a short OK; no activation sent")
            reply, sent, received = exchange(link, OK, identity + 1, status["session_sequence"], args.out, "ok")
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
