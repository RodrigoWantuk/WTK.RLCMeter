"""Reference Phase 06 impedance calculator and raw-dump replay helper.

This tool is intentionally host-only. It uses Python's double-precision complex math
as an independent reference for the embedded C implementation.
"""

from __future__ import annotations

import argparse
import cmath
import csv
import math
from dataclasses import dataclass
from pathlib import Path
from typing import Iterable

ADC_SCALE_V = 3.3 / 4095.0
HG_NOMINAL = complex(1.0 + 68_000.0 / 4_700.0, 0.0)
RANGE_ZREF = {
    "10R": complex(10.0, 0.0),
    "100R": complex(100.0, 0.0),
    "1K": complex(1_000.0, 0.0),
    "10K": complex(10_000.0, 0.0),
    "100K": complex(100_000.0, 0.0),
    "1M": complex(1_000_000.0, 0.0),
}

COMPACT_METADATA_KEYS = {
    "f": "frequency_hz",
    "a": "amplitude_mvrms",
    "r": "range",
    "sr": "sample_rate_hz",
    "n": "samples",
    "wps": "words_per_sample",
}


@dataclass(frozen=True)
class RawCapture:
    metadata: dict[str, str]
    rows: list[tuple[int, int, int, int, int, int, int]]


@dataclass(frozen=True)
class ReferenceResult:
    vexc: complex
    ret_1x: complex
    ret_hg_reconstructed: complex
    vmid: complex
    z_1x: complex
    z_hg: complex


def parse_raw_dump(text: str) -> RawCapture:
    in_capture = False
    in_rows = False
    metadata: dict[str, str] = {}
    rows: list[tuple[int, int, int, int, int, int, int]] = []

    for raw_line in text.splitlines():
        line = raw_line.strip()
        if not line:
            continue
        if line.startswith("METROLOGY_RAW_BEGIN") or line.startswith("RAW_BEGIN"):
            in_capture = True
            continue
        if line.startswith("METROLOGY_RAW_END") or line.startswith("RAW_END"):
            break
        if not in_capture:
            continue
        if line in {
            "index,vexc1,ret1x,vexc2,rethg,vmid_adc1,vmid_adc2",
            "i,v1,r1,v2,rh,vm1,vm2",
        }:
            in_rows = True
            continue
        if not in_rows:
            if "=" in line:
                key, value = line.split("=", 1)
                metadata[COMPACT_METADATA_KEYS.get(key, key)] = value
            continue

        fields = next(csv.reader([line]))
        if len(fields) != 7:
            raise ValueError(f"invalid raw row: {line!r}")
        rows.append(tuple(int(field) for field in fields))  # type: ignore[arg-type]

    if not rows:
        raise ValueError("no raw rows found")
    return RawCapture(metadata=metadata, rows=rows)


def _extract(values: Iterable[float], samples_per_cycle: int) -> complex:
    vals = list(values)
    total = len(vals)
    if total == 0:
        raise ValueError("empty channel")
    acc = 0j
    for n, value in enumerate(vals):
        theta = 2.0 * math.pi * (n % samples_per_cycle) / samples_per_cycle
        acc += value * complex(math.cos(theta), -math.sin(theta))
    return (2.0 / total) * acc


def samples_per_cycle(capture: RawCapture) -> int:
    frequency = int(capture.metadata["frequency_hz"])
    sample_rate = int(capture.metadata["sample_rate_hz"])
    return sample_rate // frequency


def analyze_capture(capture: RawCapture, hg_transfer: complex = HG_NOMINAL) -> ReferenceResult:
    spc = samples_per_cycle(capture)
    cols = list(zip(*capture.rows))
    vexc1 = _extract((raw * ADC_SCALE_V for raw in cols[1]), spc)
    ret1x = _extract((raw * ADC_SCALE_V for raw in cols[2]), spc)
    vexc2 = _extract((raw * ADC_SCALE_V for raw in cols[3]), spc)
    rethg = _extract((raw * ADC_SCALE_V for raw in cols[4]), spc)
    vmid1 = _extract((raw * ADC_SCALE_V for raw in cols[5]), spc)
    vmid2 = _extract((raw * ADC_SCALE_V for raw in cols[6]), spc)
    vmid = 0.5 * (vmid1 + vmid2)
    ret_hg_reconstructed = vmid + (rethg - vmid) / hg_transfer
    zref = RANGE_ZREF[capture.metadata["range"]]
    z_1x = zref * (ret1x - vmid) / ((vexc1 - vmid) - (ret1x - vmid))
    z_hg = zref * (ret_hg_reconstructed - vmid) / (
        (vexc2 - vmid) - (ret_hg_reconstructed - vmid)
    )
    return ReferenceResult(
        vexc=vexc1,
        ret_1x=ret1x,
        ret_hg_reconstructed=ret_hg_reconstructed,
        vmid=vmid,
        z_1x=z_1x,
        z_hg=z_hg,
    )


def synthetic_capture(
    z_dut: complex,
    *,
    frequency_hz: int = 1000,
    sample_rate_hz: int = 64000,
    sample_count: int = 256,
    range_name: str = "1K",
    vs_peak: complex = complex(0.05, 0.0),
    hg_transfer: complex = HG_NOMINAL,
) -> RawCapture:
    zref = RANGE_ZREF[range_name]
    vx = z_dut * vs_peak / (zref + z_dut)
    rows: list[tuple[int, int, int, int, int, int, int]] = []
    spc = sample_rate_hz // frequency_hz
    for n in range(sample_count):
        theta = 2.0 * math.pi * (n % spc) / spc
        ref = cmath.exp(1j * theta)

        def code(ac: complex) -> int:
            voltage = 1.65 + (ac * ref).real
            return max(0, min(4095, int(voltage / ADC_SCALE_V + 0.5)))

        rows.append(
            (
                n,
                code(vs_peak),
                code(vx),
                code(vs_peak),
                code(vx * hg_transfer),
                code(0j),
                code(0j),
            )
        )
    return RawCapture(
        metadata={
            "frequency_hz": str(frequency_hz),
            "amplitude_mvrms": "100",
            "range": range_name,
            "sample_rate_hz": str(sample_rate_hz),
            "samples": str(sample_count),
            "words_per_sample": "3",
        },
        rows=rows,
    )


def main() -> int:
    parser = argparse.ArgumentParser(description=__doc__)
    parser.add_argument("dump", type=Path, help="METROLOGY_RAW dump file")
    args = parser.parse_args()
    result = analyze_capture(parse_raw_dump(args.dump.read_text(encoding="utf-8")))
    print(f"z_1x_real={result.z_1x.real:.9g}")
    print(f"z_1x_imag={result.z_1x.imag:.9g}")
    print(f"z_hg_real={result.z_hg.real:.9g}")
    print(f"z_hg_imag={result.z_hg.imag:.9g}")
    return 0


if __name__ == "__main__":
    raise SystemExit(main())
