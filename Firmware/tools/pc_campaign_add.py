#!/usr/bin/env python3
"""Add one SHA-bound BRINGUP capture to a host-only calibration campaign."""

from __future__ import annotations

import argparse
import copy
import hashlib
import json
import os
from pathlib import Path
import tempfile
from typing import Any

from calibration_campaign import (CampaignError, bind_campaign_captures,
                                  require_complete_osl, solve_campaign)
from inspect_calibration_record import OslFrameSummary, SLOT_BYTES, decode_full_rev1_frame
from pc_capture import MAX_CAPTURE_LINES, MAX_LINE_BYTES, validate_dut_capture


RANGE_OHMS = {"10R": 10, "100R": 100, "1K": 1000, "10K": 10000,
              "100K": 100000, "1M": 1000000}
OPTIONAL_SPEC_FIELDS = frozenset({
    "esr_max_ohms", "esr_max_ohms_frequency_hz",
    "esr_nominal_ohms", "esr_tolerance_fraction", "esr_nominal_ohms_frequency_hz",
    "d_max", "d_max_frequency_hz", "d_nominal", "d_tolerance_fraction",
    "d_nominal_frequency_hz", "q_min", "q_min_frequency_hz",
    "board_temperature_calibrated", "board_temperature_c", "datasheet_temperature_c",
})


def add_capture_standard(data: dict[str, Any], capture_root: Path,
                         frame: OslFrameSummary, capture_file: str,
                         sample_id: str, kind: str, nominal_si: float,
                         tolerance_fraction: float, role: str = "FIT",
                         datasheet_frequency_hz: int | None = None,
                         extra: dict[str, Any] | None = None) -> tuple[dict[str, Any], dict[str, Any]]:
    """Return a checked new campaign and its diagnostic report; never mutate input."""
    if not isinstance(sample_id, str) or not sample_id.strip():
        raise CampaignError("sample ID must be nonempty")
    if not isinstance(capture_file, str) or not capture_file:
        raise CampaignError("capture file must be nonempty")
    if extra and set(extra) - OPTIONAL_SPEC_FIELDS:
        raise CampaignError("optional specifications cannot override capture identity or condition")
    try:
        root = capture_root.resolve(strict=True)
        path = (root / capture_file).resolve(strict=True)
    except OSError as exc:
        raise CampaignError("capture root or file is missing") from exc
    if not root.is_dir():
        raise CampaignError("capture root must be a directory")
    if not path.is_relative_to(root) or not path.is_file():
        raise CampaignError("capture file must stay inside the capture root")
    if path.stat().st_size > MAX_CAPTURE_LINES * MAX_LINE_BYTES:
        raise CampaignError("capture exceeds bounded RAW format")
    raw = path.read_bytes()
    try:
        capture = validate_dut_capture(raw.decode("ascii"))
        meta = capture.metadata
        key = (RANGE_OHMS[meta["range"]], int(meta["frequency_hz"]),
               int(meta["amplitude_mvrms"]))
    except (UnicodeError, ValueError, KeyError) as exc:
        raise CampaignError(f"invalid completed DUT capture: {exc}") from exc
    if int(meta["calibration_sequence"]) != frame.sequence:
        raise CampaignError("capture and active OSL frame have different sequences")

    candidate = copy.deepcopy(data)
    require_complete_osl(candidate["osl_conditions"])
    if any(standard["id"] == sample_id
           for group in candidate["conditions"] for standard in group["standards"]):
        raise CampaignError("sample ID is already used in this campaign")
    standard = {
        "id": sample_id, "role": role, "capture_file": capture_file,
        "capture_id": hashlib.sha256(raw).hexdigest(), "type": kind,
        "nominal_si": nominal_si, "tolerance_fraction": tolerance_fraction,
        "datasheet_frequency_hz": (key[1] if datasheet_frequency_hz is None
                                   else datasheet_frequency_hz),
    }
    if extra:
        standard.update(extra)
    condition = {"range_ohms": key[0], "frequency_hz": key[1],
                 "amplitude_mv_rms": key[2]}
    group = next((item for item in candidate["conditions"]
                  if item["condition"] == condition), None)
    if group is None:
        group = {"condition": condition, "standards": []}
        candidate["conditions"].append(group)
        candidate["conditions"].sort(key=lambda item: (
            item["condition"]["range_ohms"], item["condition"]["frequency_hz"],
            item["condition"]["amplitude_mv_rms"]))
    group["standards"].append(standard)

    bound = bind_campaign_captures(candidate, root, frame)
    report = solve_campaign(bound)
    return candidate, report


def _atomic_json_write(path: Path, value: dict[str, Any]) -> None:
    path.parent.mkdir(parents=True, exist_ok=True)
    name: str | None = None
    try:
        with tempfile.NamedTemporaryFile("w", encoding="utf-8", newline="\n",
                                         dir=path.parent, prefix=path.name + ".",
                                         suffix=".tmp", delete=False) as stream:
            name = stream.name
            json.dump(value, stream, indent=2, sort_keys=True, allow_nan=False)
            stream.write("\n")
            stream.flush()
            os.fsync(stream.fileno())
        os.replace(name, path)
    finally:
        if name is not None and os.path.exists(name):
            os.unlink(name)


def main() -> int:
    parser = argparse.ArgumentParser(description=__doc__)
    parser.add_argument("campaign", type=Path, help="existing campaign JSON; use calibration_campaign.py --template first")
    parser.add_argument("--capture-root", type=Path, required=True)
    parser.add_argument("--osl-frame", type=Path, required=True)
    parser.add_argument("--capture", required=True, help="RAW file relative to capture root")
    parser.add_argument("--id", required=True, help="unique standard identifier")
    parser.add_argument("--type", choices=("R", "C", "L"), required=True)
    parser.add_argument("--nominal-si", type=float, required=True)
    parser.add_argument("--tolerance-fraction", type=float, required=True)
    parser.add_argument("--role", choices=("FIT", "VALIDATION"), default="FIT")
    parser.add_argument("--datasheet-frequency-hz", type=int)
    parser.add_argument("--calibrated-board-temp-c", type=float)
    parser.add_argument("--datasheet-temp-c", type=float)
    parser.add_argument("--esr-max-ohms", type=float)
    parser.add_argument("--esr-nominal-ohms", type=float)
    parser.add_argument("--esr-tolerance-fraction", type=float)
    parser.add_argument("--esr-frequency-hz", type=int)
    parser.add_argument("--d-max", type=float)
    parser.add_argument("--d-nominal", type=float)
    parser.add_argument("--d-tolerance-fraction", type=float)
    parser.add_argument("--d-frequency-hz", type=int)
    parser.add_argument("--q-min", type=float)
    parser.add_argument("--q-frequency-hz", type=int)
    parser.add_argument("--out", type=Path, help="output campaign; defaults to atomic replacement of input")
    args = parser.parse_args()

    extra: dict[str, Any] = {}
    mapping = {
        "esr_max_ohms": "esr_max_ohms",
        "esr_nominal_ohms": "esr_nominal_ohms",
        "esr_tolerance_fraction": "esr_tolerance_fraction",
        "d_max": "d_max",
        "d_nominal": "d_nominal",
        "d_tolerance_fraction": "d_tolerance_fraction",
        "q_min": "q_min",
    }
    for arg, field in mapping.items():
        value = getattr(args, arg)
        if value is not None:
            extra[field] = value
    if args.calibrated_board_temp_c is not None:
        extra["board_temperature_calibrated"] = True
        extra["board_temperature_c"] = args.calibrated_board_temp_c
    if args.datasheet_temp_c is not None:
        extra["datasheet_temperature_c"] = args.datasheet_temp_c
    for prefix, frequency, fields in (
            ("esr", args.esr_frequency_hz, ("esr_max_ohms", "esr_nominal_ohms")),
            ("d", args.d_frequency_hz, ("d_max", "d_nominal"))):
        if frequency is not None:
            matching = [field for field in fields if field in extra]
            if not matching:
                parser.error(f"--{prefix}-frequency-hz requires an {prefix.upper()} specification")
            for field in matching:
                extra[f"{field}_frequency_hz"] = frequency
    if args.q_frequency_hz is not None:
        if "q_min" not in extra:
            parser.error("--q-frequency-hz requires --q-min")
        extra["q_min_frequency_hz"] = args.q_frequency_hz
    try:
        data = json.loads(args.campaign.read_text(encoding="utf-8"))
        blob = args.osl_frame.read_bytes()
        if len(blob) > SLOT_BYTES:
            raise CampaignError("OSL frame exceeds one calibration slot")
        frame = decode_full_rev1_frame(blob)
        updated, report = add_capture_standard(
            data, args.capture_root, frame, args.capture, args.id, args.type,
            args.nominal_si, args.tolerance_fraction, args.role,
            args.datasheet_frequency_hz, extra)
        _atomic_json_write(args.out or args.campaign, updated)
    except (CampaignError, KeyError, TypeError, ValueError, OSError,
            json.JSONDecodeError) as exc:
        parser.error(str(exc))
    added = next(standard for group in updated["conditions"]
                 for standard in group["standards"] if standard["id"] == args.id)
    print(f"added {args.id} role={args.role} capture_sha256={added['capture_id']}")
    print(f"campaign_status={report['status']} qualification={report['qualification']}")
    return 1 if report["status"] == "HOST_VALIDATION_FAILED" else 0


if __name__ == "__main__":
    raise SystemExit(main())
