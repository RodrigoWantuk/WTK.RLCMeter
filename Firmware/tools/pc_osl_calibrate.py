#!/usr/bin/env python3
"""Build, inspect or simulate a full PC OSL candidate. No real-device installation."""

from __future__ import annotations

import argparse
import json
from pathlib import Path

from pc_osl import KEYS, build_candidate, load_document, report, validate_frame, c_solver_input
from pc_osl_synthetic import Fixture, campaign
from pc_osl_provision import FakeDevice, transfer_frames


def main(argv=None):
    parser = argparse.ArgumentParser(description=__doc__)
    commands = parser.add_subparsers(dest="command", required=True)
    synthetic = commands.add_parser("synthetic", help="write 99 electrically derived synthetic captures")
    synthetic.add_argument("--out", type=Path, required=True)
    synthetic.add_argument("--ideal", action="store_true")
    synthetic.add_argument("--noise-v", type=float, default=0.0)
    synthetic.add_argument("--dc-offset-v", type=float, default=0.0)
    build = commands.add_parser("build", help="solve all 33 OSL conditions and write unqualified candidate")
    build.add_argument("captures", type=Path)
    build.add_argument("--sequence", type=int, required=True)
    build.add_argument("--out", type=Path, required=True)
    build.add_argument("--report", type=Path, required=True)
    build.add_argument("--c-input", type=Path, help="optional test-only C solver comparison input")
    inspect = commands.add_parser("inspect")
    inspect.add_argument("frame", type=Path)
    simulate = commands.add_parser("simulate", help="blank-to-provisioned offline demonstration")
    simulate.add_argument("--out-dir", type=Path, required=True)
    simulate.add_argument("--noise-v", type=float, default=0.0002)
    simulate.add_argument("--dc-offset-v", type=float, default=0.025)
    args = parser.parse_args(argv)
    try:
        if args.command == "synthetic":
            doc = campaign(Fixture(args.ideal, args.noise_v, args.dc_offset_v))
            args.out.write_text(json.dumps(doc, indent=2, allow_nan=False)+"\n", encoding="utf-8")
            print(f"SYNTHETIC captures=99 conditions=33 output={args.out}")
        elif args.command == "build":
            doc = load_document(args.captures)
            frame, solutions = build_candidate(doc, args.sequence)
            args.out.write_bytes(frame)
            args.report.write_text(report(doc, frame, solutions), encoding="utf-8")
            if args.c_input:
                args.c_input.write_bytes(c_solver_input(doc, args.sequence))
            print(f"UNQUALIFIED frame={args.out} bytes={len(frame)} sha256={validate_frame(frame).sha256}")
        elif args.command == "inspect":
            summary = validate_frame(args.frame.read_bytes())
            print(f"sequence={summary.sequence} conditions={len(summary.conditions)} sha256={summary.sha256}")
        else:
            args.out_dir.mkdir(parents=True, exist_ok=True)
            fixture = Fixture(noise_v=args.noise_v, dc_offset_v=args.dc_offset_v)
            device = FakeDevice()
            try:
                device.standalone_measure(KEYS[0], 0.5+0j)
            except ValueError:
                pass
            else:
                raise ValueError("blank-device gate failed")
            doc = campaign(fixture, device)
            frame, solutions = build_candidate(doc, 1)
            frames = transfer_frames(frame)
            for encoded in frames:
                device.receive(encoded)
            device.reboot()
            errors = []
            for key in KEYS:
                truth = key.rref_ohms*complex(0.37, -0.21)
                result = device.standalone_measure(key, fixture.measure_transfer(key, truth))
                errors.append(abs(result["z_ohms"]-truth)/abs(truth))
            result = {"evidence": "SYNTHETIC_NOT_PHYSICALLY_QUALIFIED", "captures": 99,
                      "conditions": 33, "frame_bytes": len(frame), "transfer_frames": len(frames),
                      "reboot_sequence": device.identify()["active_sequence"],
                      "blank_measurement_blocked": True, "qualified": False,
                      "max_holdout_relative_error": max(errors), "real_device_provisioning": False}
            (args.out_dir/"captures.json").write_text(json.dumps(doc, indent=2)+"\n", encoding="utf-8")
            (args.out_dir/"candidate.bin").write_bytes(frame)
            (args.out_dir/"transfer.wpc").write_bytes(b"".join(frames))
            (args.out_dir/"report.md").write_text(report(doc, frame, solutions), encoding="utf-8")
            (args.out_dir/"simulation.json").write_text(json.dumps(result, indent=2)+"\n", encoding="utf-8")
            print(json.dumps(result, indent=2))
    except (ValueError, KeyError, TypeError, OSError, OverflowError) as exc:
        parser.error(str(exc))
    return 0


if __name__ == "__main__":
    raise SystemExit(main())
