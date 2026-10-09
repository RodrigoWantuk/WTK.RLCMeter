#!/usr/bin/env python3
"""Host-only interval fit for post-OSL Rev.1 standards.

The output is diagnostic, not an uploadable or qualified calibration record.
Only the current campaign's standards are hard constraints; a previous curve is
the soft starting point for the minimum-change projection.
"""

from __future__ import annotations

import argparse
import copy
import hashlib
import json
import math
import struct
from pathlib import Path
from typing import Any

from pc_capture import MAX_CAPTURE_LINES, MAX_LINE_BYTES, validate_dut_capture
from inspect_calibration_record import OslFrameSummary, SLOT_BYTES, decode_full_rev1_frame


RANGES = (10, 100, 1000, 10000, 100000, 1000000)
FREQUENCIES = (100, 1000, 10000)
AMPLITUDES = (100, 500)
KNOTS = (0.1, 1.0, 10.0)
IDENTITY = (1.0, 0.0, 0.0, 1.0) * 3
MAX_LOSS_TEMPERATURE_DELTA_C = 10.0  # Provisional; requires bench validation.
STANDARD_FIELDS = frozenset({
    "id", "role", "capture_id", "capture_safe", "capture_dsp_status",
    "capture_calibration_sequence", "type", "nominal_si", "tolerance_fraction",
    "frequency_hz", "amplitude_mv_rms", "datasheet_frequency_hz",
    "measured_z_re_ohms", "measured_z_im_ohms", "board_temperature_calibrated",
    "board_temperature_c", "datasheet_temperature_c", "esr_max_ohms", "d_max",
    "q_min", "esr_max_ohms_frequency_hz", "d_max_frequency_hz",
    "q_min_frequency_hz", "capture_file", "esr_nominal_ohms",
    "esr_tolerance_fraction", "esr_nominal_ohms_frequency_hz",
    "d_nominal", "d_tolerance_fraction", "d_nominal_frequency_hz",
})


class CampaignError(ValueError):
    pass


class CampaignConflict(CampaignError):
    pass


def all_osl_conditions() -> set[tuple[int, int, int]]:
    return {(r, f, a) for r in RANGES for f in FREQUENCIES
            for a in AMPLITUDES if not (r == 10 and a == 500)}


def campaign_template() -> dict[str, Any]:
    return {
        "schema_version": 1,
        "hardware_revision": 0x00010001,
        "osl_conditions": [
            {"range_ohms": r, "frequency_hz": f, "amplitude_mv_rms": a}
            for r, f, a in sorted(all_osl_conditions())
        ],
        "conditions": [],
    }


def condition_key(value: dict[str, Any]) -> tuple[int, int, int]:
    key = (value["range_ohms"], value["frequency_hz"], value["amplitude_mv_rms"])
    if key not in all_osl_conditions():
        raise CampaignError(f"unsupported Rev.1 condition: {key}")
    return key


def require_complete_osl(conditions: list[dict[str, Any]]) -> None:
    keys = [condition_key(item) for item in conditions]
    if len(keys) != 33 or set(keys) != all_osl_conditions():
        raise CampaignError("current campaign requires exactly 33 distinct OSL conditions")


def _positive_number(value: Any, label: str) -> float:
    try:
        number = float(value)
    except (TypeError, ValueError) as exc:
        raise CampaignError(f"{label} must be numeric") from exc
    if not math.isfinite(number) or number <= 0.0:
        raise CampaignError(f"{label} must be finite and positive")
    return number


def _finite_number(value: Any, label: str) -> float:
    try:
        number = float(value)
    except (TypeError, ValueError) as exc:
        raise CampaignError(f"{label} must be numeric") from exc
    if not math.isfinite(number):
        raise CampaignError(f"{label} must be finite")
    return number


def _specified_interval(standard: dict[str, Any], nominal_field: str,
                        tolerance_field: str) -> tuple[float, float] | None:
    if (nominal_field in standard) != (tolerance_field in standard):
        raise CampaignError(f"{nominal_field} requires {tolerance_field} and vice versa")
    if nominal_field not in standard:
        return None
    nominal = _positive_number(standard[nominal_field], nominal_field)
    tolerance = _finite_number(standard[tolerance_field], tolerance_field)
    if not 0.0 < tolerance < 1.0:
        raise CampaignError(f"{tolerance_field} must be in (0,1)")
    return nominal * (1.0 - tolerance), nominal * (1.0 + tolerance)


def _basis(real: float, imag: float, rref: int) -> tuple[list[float], list[float]]:
    rho = math.hypot(real, imag) / rref
    if not KNOTS[0] <= rho <= KNOTS[-1]:
        raise CampaignError("standard lies outside the 0.1..10 RREF correction domain")
    position = math.log10(rho) + 1.0
    lower = min(int(position), 1)
    fraction = position - lower
    weights = [0.0, 0.0, 0.0]
    weights[lower] = 1.0 - fraction
    weights[lower + 1] = fraction
    real_basis = [0.0] * 12
    imag_basis = [0.0] * 12
    for knot, weight in enumerate(weights):
        real_basis[4 * knot] = weight * real / rref
        real_basis[4 * knot + 1] = weight * imag / rref
        imag_basis[4 * knot + 2] = weight * real / rref
        imag_basis[4 * knot + 3] = weight * imag / rref
    return real_basis, imag_basis


def corrected_impedance(curve: list[float], real: float, imag: float,
                        rref: int) -> tuple[float, float]:
    if len(curve) != 12 or not all(math.isfinite(x) for x in curve):
        raise CampaignError("curve must contain 12 finite coefficients")
    rb, xb = _basis(real, imag, rref)
    corrected = (rref * sum(a * b for a, b in zip(rb, curve)),
                 rref * sum(a * b for a, b in zip(xb, curve)))
    if not all(math.isfinite(value) for value in corrected):
        raise CampaignError("corrected impedance is not finite")
    return corrected


def _combine(a: list[float], b: list[float], factor: float) -> list[float]:
    return [x + factor * y for x, y in zip(a, b)]


def _bounds(vector: list[float], low: float | None, high: float | None,
            constraints: list[tuple[list[float], float]]) -> None:
    if low is not None:
        constraints.append(([-x for x in vector], -low))
    if high is not None:
        constraints.append((vector[:], high))


def standard_constraints(standard: dict[str, Any], key: tuple[int, int, int],
                         ignored_loss_specs: list[str] | None = None) -> list[tuple[list[float], float]]:
    rref, frequency, amplitude = key
    unknown_fields = set(standard) - STANDARD_FIELDS
    if unknown_fields:
        raise CampaignError(f"unsupported standard fields: {sorted(unknown_fields)}")
    if (not isinstance(standard.get("capture_id"), str) or
            not standard["capture_id"] or
            standard.get("capture_safe") is not True or
            standard.get("capture_dsp_status") != "OK" or
            not isinstance(standard.get("capture_calibration_sequence"), int) or
            standard["capture_calibration_sequence"] < 0):
        raise CampaignError("standard needs a safe completed capture with DSP OK and calibration provenance")
    if standard.get("frequency_hz") != frequency or standard.get("amplitude_mv_rms") != amplitude:
        raise CampaignError("capture condition does not match its calibration key")
    if standard.get("datasheet_frequency_hz") != frequency:
        raise CampaignError("datasheet frequency must match an exact supported capture frequency")
    kind = standard["type"]
    if kind not in ("R", "C", "L"):
        raise CampaignError("standard type must be R, C, or L")
    if (kind != "C" and any(field in standard for field in (
            "esr_max_ohms", "d_max", "esr_nominal_ohms", "esr_tolerance_fraction",
            "d_nominal", "d_tolerance_fraction"))) or \
       (kind != "L" and "q_min" in standard):
        raise CampaignError("loss specification does not match component type")
    nominal = _positive_number(standard["nominal_si"], "nominal_si")
    tolerance = _finite_number(standard["tolerance_fraction"], "tolerance_fraction")
    if not 0.0 <= tolerance < 1.0:
        raise CampaignError("tolerance_fraction must be in [0,1)")
    real = _finite_number(standard["measured_z_re_ohms"], "measured real impedance")
    imag = _finite_number(standard["measured_z_im_ohms"], "measured imaginary impedance")
    rb, xb = _basis(real, imag, rref)
    constraints: list[tuple[list[float], float]] = []
    lo, hi = nominal * (1.0 - tolerance), nominal * (1.0 + tolerance)
    omega = 2.0 * math.pi * frequency
    if kind == "R":
        _bounds(rb, lo / rref, hi / rref, constraints)
    elif kind == "C":
        _bounds(xb, -1.0 / (omega * lo * rref),
                -1.0 / (omega * hi * rref), constraints)
    else:
        _bounds(xb, omega * lo / rref, omega * hi / rref, constraints)

    esr_interval = _specified_interval(standard, "esr_nominal_ohms", "esr_tolerance_fraction")
    d_interval = _specified_interval(standard, "d_nominal", "d_tolerance_fraction")
    loss_fields = ("esr_max_ohms", "d_max", "q_min", "esr_nominal_ohms", "d_nominal")
    active_loss = set()
    for field in loss_fields:
        if field not in standard:
            continue
        spec_frequency = standard.get(f"{field}_frequency_hz", standard["datasheet_frequency_hz"])
        if spec_frequency == frequency:
            active_loss.add(field)
        elif ignored_loss_specs is not None:
            ignored_loss_specs.append(f"{standard['id']}:{field}:OUT_OF_BAND")
    if active_loss:
        if (not standard.get("board_temperature_calibrated") or
                "board_temperature_c" not in standard or
                "datasheet_temperature_c" not in standard):
            raise CampaignError("loss constraint requires calibrated board temperature and datasheet temperature")
        board_temp = _finite_number(standard["board_temperature_c"], "board temperature")
        spec_temp = _finite_number(standard["datasheet_temperature_c"], "datasheet temperature")
        if abs(board_temp - spec_temp) > MAX_LOSS_TEMPERATURE_DELTA_C:
            raise CampaignError("loss constraint exceeds provisional 10 C temperature gate")
    if "esr_max_ohms" in active_loss:
        _bounds(rb, 0.0, _positive_number(standard["esr_max_ohms"], "ESR maximum") / rref,
                constraints)
    if "esr_nominal_ohms" in active_loss and esr_interval is not None:
        _bounds(rb, esr_interval[0] / rref, esr_interval[1] / rref, constraints)
    if "d_max" in active_loss:
        dmax = _positive_number(standard["d_max"], "D maximum")
        _bounds(rb, 0.0, None, constraints)
        constraints.append((_combine(rb, xb, dmax), 0.0))
    if "d_nominal" in active_loss and d_interval is not None:
        _bounds(rb, 0.0, None, constraints)
        constraints.append((_combine(rb, xb, d_interval[1]), 0.0))
        constraints.append(([-x for x in _combine(rb, xb, d_interval[0])], 0.0))
    if "q_min" in active_loss:
        qmin = _positive_number(standard["q_min"], "Q minimum")
        _bounds(rb, 0.0, None, constraints)
        constraints.append((_combine(rb, xb, -1.0 / qmin), 0.0))
    return constraints


def _solve_linear(matrix: list[list[float]], rhs: list[float]) -> list[float] | None:
    size = len(rhs)
    rows = [matrix[index][:] + [rhs[index]] for index in range(size)]
    for col in range(size):
        pivot = max(range(col, size), key=lambda row: abs(rows[row][col]))
        if abs(rows[pivot][col]) < 1.0e-14:
            return None
        rows[col], rows[pivot] = rows[pivot], rows[col]
        scale = rows[col][col]
        rows[col] = [value / scale for value in rows[col]]
        for row in range(size):
            if row != col:
                factor = rows[row][col]
                rows[row] = [value - factor * other for value, other in zip(rows[row], rows[col])]
    return [row[-1] for row in rows]


def _active_projection(prior: list[float],
                       constraints: list[tuple[list[float], float]],
                       multipliers: list[float]) -> tuple[list[float], list[float]] | None:
    active = [index for index, value in enumerate(multipliers) if value > 1.0e-12]
    if not active or len(active) > len(prior):
        return None
    while active:
        matrix = [[sum(a * b for a, b in zip(constraints[i][0], constraints[j][0]))
                   for j in active] for i in active]
        rhs = [sum(a * b for a, b in zip(constraints[i][0], prior)) - constraints[i][1]
               for i in active]
        solved = _solve_linear(matrix, rhs)
        if solved is None:
            return None
        most_negative = min(range(len(solved)), key=lambda index: solved[index])
        if solved[most_negative] < -1.0e-10:
            active.pop(most_negative)
            continue
        polished_multipliers = [0.0] * len(constraints)
        point = prior[:]
        for index, value in zip(active, solved):
            polished_multipliers[index] = max(0.0, value)
            point = [x - value * normal for x, normal in zip(point, constraints[index][0])]
        return point, polished_multipliers
    return None


def minimum_change_fit(prior: list[float],
                       constraints: list[tuple[list[float], float]]) -> list[float]:
    if len(prior) != 12 or not all(math.isfinite(x) for x in prior):
        raise CampaignError("prior curve must contain 12 finite coefficients")
    if any(len(normal) != 12 or not math.isfinite(bound) or
           not all(math.isfinite(value) for value in normal)
           for normal, bound in constraints):
        raise CampaignError("standard produced a non-finite or malformed constraint")
    if not constraints:
        return prior[:]
    point = prior[:]
    multipliers = [0.0] * len(constraints)
    for iteration in range(10000):
        for index, (normal, bound) in enumerate(constraints):
            norm2 = sum(x * x for x in normal)
            if norm2 <= 1.0e-24:
                if bound < -1.0e-12:
                    raise CampaignConflict("zero-sensitivity standard conflicts with its specified bound")
                continue
            excess = sum(a * b for a, b in zip(normal, point)) - bound
            new_multiplier = max(0.0, multipliers[index] + excess / norm2)
            change = new_multiplier - multipliers[index]
            multipliers[index] = new_multiplier
            point = [x - change * n for x, n in zip(point, normal)]
        if iteration % 20 == 19:
            polished = _active_projection(prior, constraints, multipliers)
            if polished is not None:
                candidate, candidate_multipliers = polished
                if max(sum(a * b for a, b in zip(normal, candidate)) - bound
                       for normal, bound in constraints) <= 1.0e-9:
                    return candidate
                point, multipliers = candidate, candidate_multipliers
        max_violation = max(sum(a * b for a, b in zip(normal, point)) - bound
                            for normal, bound in constraints)
        complementarity = max(abs(value *
                                  (sum(a * b for a, b in zip(constraints[index][0], point)) -
                                   constraints[index][1]))
                              for index, value in enumerate(multipliers))
        if max_violation <= 1.0e-9 and complementarity <= 1.0e-9 and iteration > 10:
            return point
    raise CampaignConflict(f"current-campaign standards conflict or fit did not converge; "
                           f"normalized max violation={max_violation:.6g}")


def _constraints_pass(curve: list[float],
                      constraints: list[tuple[list[float], float]]) -> bool:
    return all(sum(a * b for a, b in zip(normal, curve)) - bound <= 1.0e-9
               for normal, bound in constraints)


def _wire_curve(curve: list[float]) -> list[float] | None:
    try:
        encoded = struct.pack("<12f", *curve)
    except (OverflowError, struct.error):
        return None
    decoded = list(struct.unpack("<12f", encoded))
    return decoded if all(math.isfinite(value) for value in decoded) else None


def _standard_evidence(standard: dict[str, Any], key: tuple[int, int, int],
                       curve: list[float], constraints: list[tuple[list[float], float]],
                       role: str) -> dict[str, Any]:
    real, imag = corrected_impedance(
        curve,
        _finite_number(standard["measured_z_re_ohms"], "measured real impedance"),
        _finite_number(standard["measured_z_im_ohms"], "measured imaginary impedance"),
        key[0])
    kind = standard["type"]
    omega = 2.0 * math.pi * key[1]
    value = (real if kind == "R" else
             -1.0 / (omega * imag) if kind == "C" and imag < 0.0 else
             imag / omega if kind == "L" else None)
    if value is not None and not math.isfinite(value):
        value = None
    nominal = _positive_number(standard["nominal_si"], "nominal_si")
    tolerance = _finite_number(standard["tolerance_fraction"], "tolerance_fraction")
    return {
        "id": standard["id"], "role": role, "type": kind,
        "corrected_z_re_ohms": real, "corrected_z_im_ohms": imag,
        "corrected_value_si": value,
        "specified_min_si": nominal * (1.0 - tolerance),
        "specified_max_si": nominal * (1.0 + tolerance),
        "constraint_count": len(constraints),
        "within_all_specified_intervals": _constraints_pass(curve, constraints),
    }


def constraint_rank(constraints: list[tuple[list[float], float]]) -> int:
    """Necessary linear-span evidence, not proof of bounded coefficient uncertainty."""
    basis: list[list[float]] = []
    for normal, _ in constraints:
        original_norm = math.sqrt(sum(value * value for value in normal))
        if original_norm <= 1.0e-12:
            continue
        vector = [value / original_norm for value in normal]
        for unit in basis:
            projection = sum(a * b for a, b in zip(vector, unit))
            vector = [a - projection * b for a, b in zip(vector, unit)]
        norm = math.sqrt(sum(value * value for value in vector))
        if norm > 1.0e-9:
            basis.append([value / norm for value in vector])
    return len(basis)


def solve_campaign(data: dict[str, Any]) -> dict[str, Any]:
    if data.get("schema_version") != 1 or data.get("hardware_revision") != 0x00010001:
        raise CampaignError("unsupported campaign schema or hardware revision")
    require_complete_osl(data["osl_conditions"])
    outputs = []
    seen_keys = set()
    seen_captures = set()
    active_sequence = None
    for group in data["conditions"]:
        key = condition_key(group["condition"])
        if key in seen_keys:
            raise CampaignError("duplicate calibration condition group")
        seen_keys.add(key)
        prior = [_finite_number(x, "prior curve coefficient") for x in group.get("prior_curve", IDENTITY)]
        if len(prior) != 12:
            raise CampaignError("prior curve must contain 12 coefficients")
        constraints: list[tuple[list[float], float]] = []
        evidence_inputs: list[tuple[dict[str, Any], str, list[tuple[list[float], float]]]] = []
        seen_ids = set()
        ignored_loss_specs: list[str] = []
        for standard in group["standards"]:
            sample_id = standard["id"]
            capture_id = standard.get("capture_id")
            if (not isinstance(sample_id, str) or not sample_id or sample_id in seen_ids or
                    capture_id in seen_captures):
                raise CampaignError("sample and capture IDs must be nonempty and unique per condition")
            seen_ids.add(sample_id)
            seen_captures.add(capture_id)
            sequence = standard["capture_calibration_sequence"]
            if active_sequence is None:
                active_sequence = sequence
            elif sequence != active_sequence:
                raise CampaignError("standards from different active OSL sequences cannot be combined")
            role = standard.get("role", "FIT")
            if role not in ("FIT", "VALIDATION"):
                raise CampaignError("standard role must be FIT or VALIDATION")
            sample_constraints = standard_constraints(standard, key, ignored_loss_specs)
            evidence_inputs.append((standard, role, sample_constraints))
            if role == "FIT":
                constraints.extend(sample_constraints)
        try:
            solved = minimum_change_fit(prior, constraints)
        except CampaignConflict as exc:
            fit_ids = [sample["id"] for sample, role, _ in evidence_inputs if role == "FIT"]
            shown = fit_ids[:20]
            suffix = f" (+{len(fit_ids) - 20} more)" if len(fit_ids) > 20 else ""
            raise CampaignConflict(f"condition {key}, FIT samples {shown}{suffix}: {exc}") from exc
        evidence = [_standard_evidence(sample, key, solved, checks, role)
                    for sample, role, checks in evidence_inputs]
        if any(not item["within_all_specified_intervals"] for item in evidence
               if item["role"] == "FIT"):
            raise CampaignConflict(f"fitted coefficients do not satisfy all FIT standards at {key}")
        failed_validation = [item["id"] for item in evidence
                             if item["role"] == "VALIDATION" and
                             not item["within_all_specified_intervals"]]
        wire_curve = _wire_curve(solved)
        wire_failed_ids = [sample["id"] for sample, _, checks in evidence_inputs
                           if wire_curve is None or not _constraints_pass(wire_curve, checks)]
        validation_count = sum(item["role"] == "VALIDATION" for item in evidence)
        rank = constraint_rank(constraints)
        outputs.append({"condition": group["condition"], "curve": solved,
                        "sample_count": len(seen_ids),
                        "fit_count": len(seen_ids) - validation_count,
                        "validation_count": validation_count,
                        "validation": ("FAIL" if failed_validation else
                                       "PASS" if validation_count else "NOT_PROVIDED"),
                        "failed_validation_ids": failed_validation,
                        "float32_validation": "FAIL" if wire_failed_ids else "PASS",
                        "float32_failed_ids": wire_failed_ids,
                        "standard_evidence": evidence,
                        "constraint_rank": rank,
                        "ignored_loss_specs": ignored_loss_specs,
                        "coverage": ("FULL_LINEAR_SPAN_UNQUALIFIED" if rank == 12 else
                                     "PARTIAL_LINEAR_SPAN" if constraints else "PRIOR_UNCHANGED")})
    status = ("HOST_VALIDATION_FAILED" if any(group["validation"] == "FAIL" for group in outputs)
              else "HOST_QUANTIZATION_FAILED" if any(group["float32_validation"] == "FAIL"
                                                    for group in outputs)
              else "HOST_PROVISIONAL_NOT_FLASH_READY")
    return {"schema_version": 1,
            "status": status,
            "qualification": "UNQUALIFIED", "conditions": outputs}


def bind_campaign_captures(data: dict[str, Any], root: Path,
                           osl_frame: OslFrameSummary | None = None) -> dict[str, Any]:
    """Bind declared standard values to completed BRINGUP dumps, without claiming OSL verification."""
    bound = copy.deepcopy(data)
    if osl_frame is not None:
        require_complete_osl(bound["osl_conditions"])
        if {condition_key(item) for item in bound["osl_conditions"]} != osl_frame.conditions:
            raise CampaignError("campaign OSL keys differ from the supplied active-set frame")
    root = root.resolve(strict=True)
    if not root.is_dir():
        raise CampaignError("capture root must be a directory")
    for group in bound["conditions"]:
        rref, frequency, amplitude = condition_key(group["condition"])
        range_token = {10: "10R", 100: "100R", 1000: "1K", 10000: "10K",
                       100000: "100K", 1000000: "1M"}[rref]
        for standard in group["standards"]:
            name = standard.get("capture_file")
            if not isinstance(name, str) or not name:
                raise CampaignError("every standard needs a capture_file")
            path = (root / name).resolve(strict=True)
            if not path.is_relative_to(root) or not path.is_file():
                raise CampaignError("capture_file must stay inside the capture root")
            if path.stat().st_size > MAX_CAPTURE_LINES * MAX_LINE_BYTES:
                raise CampaignError(f"capture exceeds bounded raw format: {name}")
            raw = path.read_bytes()
            capture_id = hashlib.sha256(raw).hexdigest()
            if standard.get("capture_id") != capture_id:
                raise CampaignError(f"capture SHA-256 mismatch: {name}")
            try:
                capture = validate_dut_capture(raw.decode("ascii"))
            except (UnicodeError, ValueError) as exc:
                raise CampaignError(f"invalid capture {name}: {exc}") from exc
            meta = capture.metadata
            if osl_frame is not None and int(meta["calibration_sequence"]) != osl_frame.sequence:
                raise CampaignError(f"capture OSL sequence differs from supplied frame: {name}")
            if (meta["range"] != range_token or int(meta["frequency_hz"]) != frequency or
                    int(meta["amplitude_mvrms"]) != amplitude):
                raise CampaignError(f"capture condition mismatch: {name}")
            if (meta["dsp_status"] != "OK" or
                    meta.get("return_channel") not in ("RET_1X", "RET_HG") or
                    meta.get("calibration") not in ("FOUND source=PERSISTED",
                                                    "UNQUALIFIED source=PERSISTED")):
                raise CampaignError(f"capture lacks a successful persisted-OSL DSP result: {name}")
            try:
                real = int(meta["z_real_mohm"]) / 1000.0
                imag = int(meta["z_imag_mohm"]) / 1000.0
            except (KeyError, ValueError) as exc:
                raise CampaignError(f"capture lacks numeric complex impedance: {name}") from exc
            derived = {
                "capture_safe": True,
                "capture_dsp_status": "OK",
                "capture_calibration_sequence": int(meta["calibration_sequence"]),
                "frequency_hz": frequency,
                "amplitude_mv_rms": amplitude,
                "measured_z_re_ohms": real,
                "measured_z_im_ohms": imag,
            }
            for field, value in derived.items():
                if field in standard and standard[field] != value:
                    raise CampaignError(f"declared {field} differs from capture {name}")
            standard.update(derived)
    return bound


def main() -> int:
    parser = argparse.ArgumentParser(description=__doc__)
    parser.add_argument("campaign", nargs="?", type=Path,
                        help="JSON current campaign with all 33 OSL keys")
    parser.add_argument("--template", action="store_true",
                        help="emit an unverified 33-key input template")
    parser.add_argument("--out", type=Path, help="diagnostic JSON output, not a firmware record")
    parser.add_argument("--capture-root", type=Path,
                        help="directory containing SHA-256-bound BRINGUP RAW v1 captures")
    parser.add_argument("--osl-frame", type=Path,
                        help="supplied committed Rev.1 33-condition calibration frame/slot image")
    parser.add_argument("--synthetic-unbound", action="store_true",
                        help="allow synthetic/manual inputs for development only")
    args = parser.parse_args()
    if args.template:
        if args.campaign is not None:
            parser.error("a campaign file cannot be supplied with --template")
        result = campaign_template()
    else:
        if args.campaign is None:
            parser.error("campaign file is required unless --template is used")
        if args.synthetic_unbound:
            if args.capture_root is not None or args.osl_frame is not None:
                parser.error("--synthetic-unbound cannot be combined with capture/frame evidence")
        elif args.capture_root is None or args.osl_frame is None:
            parser.error("provide both --capture-root and --osl-frame, or --synthetic-unbound")
        try:
            data = json.loads(args.campaign.read_text(encoding="utf-8"))
            frame = None
            if args.capture_root is not None:
                if args.osl_frame.stat().st_size > SLOT_BYTES:
                    raise CampaignError("OSL frame exceeds one calibration slot")
                osl_blob = args.osl_frame.read_bytes()
                frame = decode_full_rev1_frame(osl_blob)
                data = bind_campaign_captures(data, args.capture_root, frame)
            result = solve_campaign(data)
            result["capture_binding"] = "RAW_SHA256_BOUND" if args.capture_root else "SYNTHETIC_UNBOUND"
            if frame is not None:
                result["supplied_osl_frame_sha256"] = frame.sha256
                result["supplied_osl_frame_crc32"] = int.from_bytes(osl_blob[56:60], "little")
                result["supplied_osl_sequence"] = frame.sequence
        except (CampaignError, KeyError, TypeError, OSError, json.JSONDecodeError) as exc:
            parser.error(str(exc))
    output = json.dumps(result, indent=2, sort_keys=True, allow_nan=False) + "\n"
    if args.out:
        args.out.write_text(output, encoding="utf-8")
    else:
        print(output, end="")
    return 1 if result.get("status") in ("HOST_VALIDATION_FAILED", "HOST_QUANTIZATION_FAILED") else 0


if __name__ == "__main__":
    raise SystemExit(main())
