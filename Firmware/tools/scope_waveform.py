"""Explicit, bounded waveform CSV and offline analysis; no device or GUI dependencies.

All voltages are at the probe tip. A cosine fit uses a-j*b (the firmware DFT sign).
Analysis policies are observability guards, not instrument accuracy specifications.
"""
from __future__ import annotations

from array import array
from dataclasses import asdict, dataclass, field
import csv
import hashlib
import json
import math
from pathlib import Path
import re
import shutil

MAX_BYTES = 512 * 1024 * 1024
MAX_SAMPLES = 4_000_000
MAX_LINE = 4096
FIT_SAMPLES = 32768
TIME_UNITS = {"s": 1., "ms": 1e-3, "us": 1e-6, "ns": 1e-9}
VOLT_UNITS = {"V": 1., "mV": 1e-3, "uV": 1e-6}
B01_KEYS = ("powerup_waveforms", "powerdown_waveforms", "signal_backfeed",
            "analog_adc_voltages", "3v3_stability", "bat54s_conduction")
SAFETY = ("Both probe grounds: verified circuit GND only. Never VMID, RET, TEST_LO or an amplifier output. "
          "Never float protective earth. No energized DUT, mains, high-voltage injection or charged calibration capacitor. "
          "Imported files only; active/fault experiments require separately reviewed bench procedures.")


@dataclass(frozen=True)
class Channel:
    column: int
    net: str
    unit: str
    scale: float
    offset_v: float
    probe_attenuation: float
    # tip: CSV already includes probe compensation; connector: multiply by attenuation once.
    voltage_reference: str
    coupling: str
    bandwidth_hz: float | None = None
    voltage_error_bound_v: float | None = None
    accuracy_evidence: str | None = None
    displayed_limits_v: tuple | None = None


@dataclass(frozen=True)
class ImportConfig:
    time_column: int
    time_unit: str
    channels: tuple
    delimiter: str = ","
    decimal: str = "."
    header: bool = True
    skip_rows: int = 0
    time_offset_s: float = 0.
    simultaneous: bool = False
    declared_interval_s: float | None = None
    profile: str = "generic"
    metadata: dict = field(default_factory=dict)
    max_samples: int = MAX_SAMPLES


@dataclass
class Waveform:
    time: array
    channels: tuple
    config: ImportConfig
    source: dict
    interval_s: float
    jitter_fraction: float
    warnings: list


def finite(value, name, positive=False):
    if isinstance(value, bool) or not isinstance(value, (int, float)) or not math.isfinite(value) or (positive and value <= 0):
        raise ValueError("invalid " + name)
    return float(value)


def validate(config):
    if config.profile == "hantek_dso2c10":
        raise ValueError("UNSUPPORTED Hantek DSO2C10 native CSV: no verified export fixture/layout. Supply an unchanged real CSV, settings screenshot and probe configuration; use explicit generic mapping only for established numeric time/voltage columns.")
    if config.profile != "generic":
        raise ValueError("unsupported CSV profile")
    if config.delimiter not in (",", ";", "\t") or config.decimal not in (".", ",") or config.delimiter == config.decimal:
        raise ValueError("declare distinct supported separator and decimal conventions")
    if config.time_unit not in TIME_UNITS or not 1 <= len(config.channels) <= 2:
        raise ValueError("explicit time unit and one/two channels required")
    columns = [config.time_column] + [c.column for c in config.channels]
    if any(type(i) is not int or not 0 <= i < 64 for i in columns) or len(set(columns)) != len(columns):
        raise ValueError("distinct column indices in 0..63 required")
    if type(config.skip_rows) is not int or not 0 <= config.skip_rows <= 1024 or type(config.max_samples) is not int or not 4 <= config.max_samples <= MAX_SAMPLES:
        raise ValueError("invalid row/sample bound")
    if type(config.header) is not bool or type(config.simultaneous) is not bool:
        raise ValueError("declare header and simultaneity")
    finite(config.time_offset_s, "time offset")
    if config.declared_interval_s is not None:
        finite(config.declared_interval_s, "declared sample interval", True)
    for c in config.channels:
        if not isinstance(c.net, str) or not c.net.strip() or c.unit not in VOLT_UNITS or c.voltage_reference not in ("tip", "connector") or c.coupling not in ("DC", "AC", "UNKNOWN"):
            raise ValueError("declare net, voltage unit, scale reference and coupling; ADC codes are unsupported")
        finite(c.scale, "vertical scale", True); finite(c.offset_v, "vertical offset")
        finite(c.probe_attenuation, "probe attenuation", True)
        if c.bandwidth_hz is not None:
            finite(c.bandwidth_hz, "effective probe/channel bandwidth", True)
        if c.voltage_error_bound_v is not None:
            if finite(c.voltage_error_bound_v, "voltage error bound") < 0 or not c.accuracy_evidence:
                raise ValueError("voltage uncertainty needs nonnegative bound and documented probe/scope evidence")
        if c.displayed_limits_v is not None:
            lo, hi = c.displayed_limits_v
            if finite(lo, "display low") >= finite(hi, "display high"):
                raise ValueError("invalid displayed input limits")
    if not isinstance(config.metadata, dict) or type(config.metadata.get("synthetic")) is not bool:
        raise ValueError("declare synthetic true/false; unknown evidence is never physical approval")
    if len(json.dumps(asdict(config), allow_nan=False)) > 16384:
        raise ValueError("metadata exceeds 16 KiB")


def file_hash(path):
    if Path(path).stat().st_size > MAX_BYTES:
        raise ValueError("hash input exceeds 512 MiB bound")
    digest = hashlib.sha256()
    with Path(path).open("rb") as stream:
        for chunk in iter(lambda: stream.read(65536), b""):
            digest.update(chunk)
            if stream.tell() > MAX_BYTES:
                raise ValueError("hash input grew beyond bound")
    return digest.hexdigest()


def import_csv(path, config, canceled=lambda: False):
    validate(config)
    path = Path(path).resolve()
    before = path.stat()
    if not 0 < before.st_size <= MAX_BYTES:
        raise ValueError("empty/oversized CSV (512 MiB maximum)")
    times, values = array("d"), tuple(array("d") for _ in config.channels)
    digest = hashlib.sha256(); widths = None; min_dt = math.inf; max_dt = 0.
    max_col = max([config.time_column] + [c.column for c in config.channels])
    header_done = not config.header
    numeric = re.compile(r"^[+-]?(?:\d+(?:[.,]\d*)?|[.,]\d+)(?:[eE][+-]?\d+)?$")
    with path.open("rb") as stream:
        line_number = 0; total = 0
        while True:
            raw = stream.readline(MAX_LINE + 1)
            if not raw:
                break
            line_number += 1; total += len(raw); digest.update(raw)
            if len(raw) > MAX_LINE or total > MAX_BYTES:
                raise ValueError("CSV line/file bound exceeded")
            if canceled():
                raise ValueError("waveform operation canceled")
            if line_number <= config.skip_rows:
                continue  # Opaque metadata is deliberately not interpreted.
            try:
                text = raw.decode("utf-8-sig" if line_number == 1 else "utf-8").strip("\r\n")
                row = next(csv.reader([text], delimiter=config.delimiter, strict=True))
            except (UnicodeError, csv.Error) as exc:
                raise ValueError(f"invalid UTF-8/CSV line {line_number}") from exc
            if not row or max_col >= len(row) or len(row) > 64:
                raise ValueError(f"missing/oversized columns at line {line_number}")
            if widths is None:
                widths = len(row)
            if widths != len(row):
                raise ValueError(f"truncated/inconsistent row {line_number}")
            if not header_done:
                for index, declared in [(config.time_column, config.time_unit)] + [(c.column, c.unit) for c in config.channels]:
                    label = row[index].strip()
                    units = re.findall(r"[\[(]([^\])]+)[\])]", label)
                    if any(u.strip() != declared for u in units) or re.search(r"\b(code|codes|count|counts|ADC)\b", label, re.I):
                        raise ValueError("header unit/code ambiguity conflicts with explicit mapping: " + label)
                header_done = True
                continue
            if len(times) >= config.max_samples:
                raise ValueError("sample bound exceeded; export a shorter acquisition/region, never silently truncate")
            def number(index):
                cell = row[index].strip()
                if not numeric.fullmatch(cell) or (config.decimal == "." and "," in cell) or (config.decimal == "," and "." in cell):
                    raise ValueError(f"invalid/ambiguous number at line {line_number}")
                return finite(float(cell.replace(",", ".")), "CSV sample")
            t = number(config.time_column) * TIME_UNITS[config.time_unit] + config.time_offset_s
            finite(t, "scaled time")
            if times:
                dt = t - times[-1]
                if not math.isfinite(dt) or dt <= 0:
                    raise ValueError("nonmonotonic/duplicate timestamp")
                min_dt, max_dt = min(min_dt, dt), max(max_dt, dt)
            times.append(t)
            for c, v in zip(config.channels, values):
                factor = c.probe_attenuation if c.voltage_reference == "connector" else 1.
                v.append(finite(number(c.column) * VOLT_UNITS[c.unit] * c.scale * factor + c.offset_v, "scaled voltage"))
    after = path.stat()
    if (before.st_size, before.st_mtime_ns) != (after.st_size, after.st_mtime_ns) or total != before.st_size:
        raise ValueError("source changed during import")
    if len(times) < 4:
        raise ValueError("insufficient samples (minimum four)")
    dt = (times[-1] - times[0]) / (len(times) - 1)
    finite(dt, "capture interval/duration", True)
    jitter = max(abs(min_dt / dt - 1), abs(max_dt / dt - 1))
    warnings = []
    # 0.1% interval spread bounds time-grid mismatch; not a jitter qualification.
    if jitter > .001:
        warnings.append("NONUNIFORM_TIME: frequency/harmonics unavailable; descriptive statistics are sample-weighted")
    if config.declared_interval_s is not None and abs(config.declared_interval_s / dt - 1) > .001:
        raise ValueError("declared sample interval conflicts with timestamps")
    return Waveform(times, values, config, dict(path=str(path), sha256=digest.hexdigest(), bytes=total), dt, jitter, warnings)


def statistics(values):
    # Welford avoids cancellation of small AC variation riding on DC.
    mean = m2 = 0.; lo = math.inf; hi = -math.inf
    for n, value in enumerate(values, 1):
        delta = value - mean; mean += delta / n; m2 += delta * (value - mean)
        lo, hi = min(lo, value), max(hi, value)
    ac2 = max(0., m2 / len(values))
    if not all(math.isfinite(v) for v in (mean,ac2,lo,hi,hi-lo)):
        raise ValueError("numeric statistics overflow")
    return dict(min_v=lo, max_v=hi, vpp_v=hi-lo, mean_v=mean, rms_v=math.hypot(mean, math.sqrt(ac2)), ac_rms_v=math.sqrt(ac2))


def crossings(time, values, level, rising=True, limit=4096):
    found = []
    for i in range(1, len(values)):
        a, b = values[i-1], values[i]
        if (a < level <= b) if rising else (a > level >= b):
            found.append(time[i-1] + (time[i]-time[i-1]) * (level-a)/(b-a))
            if len(found) == limit:
                break
    return found


def solve_linear(matrix, rhs):
    n = len(rhs); scale = max(abs(x) for row in matrix for x in row)
    for i in range(n):
        pivot = max(range(i, n), key=lambda j: abs(matrix[j][i]))
        matrix[i], matrix[pivot] = matrix[pivot], matrix[i]; rhs[i], rhs[pivot] = rhs[pivot], rhs[i]
        if abs(matrix[i][i]) <= scale * 1e-12:
            raise ValueError("singular sinusoid fit")
        divisor = matrix[i][i]
        matrix[i] = [x/divisor for x in matrix[i]]; rhs[i] /= divisor
        for j in range(n):
            if j != i:
                factor = matrix[j][i]
                matrix[j] = [a-factor*b for a,b in zip(matrix[j], matrix[i])]; rhs[j] -= factor * rhs[i]
    return rhs


def sinusoid(time, values, frequency, count):
    matrix = [[0.]*3 for _ in range(3)]; rhs = [0.]*3
    origin = time[0]
    for i in range(count):
        angle = math.tau * frequency * (time[i]-origin)
        row = (1., math.cos(angle), math.sin(angle))
        for j in range(3):
            rhs[j] += row[j] * values[i]
            for k in range(3):
                matrix[j][k] += row[j]*row[k]
    dc, a, b = solve_linear(matrix, rhs)
    residual = 0.
    for i in range(count):
        angle = math.tau * frequency * (time[i]-origin)
        residual += (values[i]-dc-a*math.cos(angle)-b*math.sin(angle))**2
    peak = math.hypot(a,b)
    return dict(frequency_hz=frequency, peak_v=peak, rms_v=peak/math.sqrt(2), phase_deg=math.degrees(math.atan2(-b,a)),
                dc_v=dc, residual_rms_v=math.sqrt(residual/count), phase_origin_s=origin, samples=count,
                fit_duration_s=time[count-1]-origin)


def fundamental(wave, values, settings, expected=None):
    result = dict(valid=False, reasons=[], fit=None, frequency_method=None, harmonics=None)
    fs = 1 / wave.interval_s; count = min(len(values), FIT_SAMPLES)
    if wave.jitter_fraction > .001:
        result["reasons"].append("nonuniform timestamps; no frequency/phase inference")
        return result
    # Contiguous original samples, never decimated: no invented anti-aliasing.
    subset = values[:count]; ts = wave.time[:count]
    stats = statistics(subset)
    if stats["vpp_v"] <= 1e-12:
        result["reasons"].append("no observable AC")
        return result
    zero = crossings(ts, subset, stats["mean_v"])
    measured = None
    if len(zero) >= 5:
        periods = sorted(zero[i]-zero[i-1] for i in range(1,len(zero)))
        median = periods[len(periods)//2]
        if max(abs(p/median-1) for p in periods) <= .05:
            # Least-squares crossing-time slope uses all cycles, reducing endpoint noise.
            center=(len(zero)-1)/2; average=math.fsum(zero)/len(zero)
            slope=math.fsum((i-center)*(t-average) for i,t in enumerate(zero))/math.fsum((i-center)**2 for i in range(len(zero)))
            measured=1/slope
    if measured is None and expected is None:
        result["reasons"].append("frequency not observable: need >=4 stable cycles; crossing method is not a dominant-spectrum search")
        return result
    freq = measured if measured is not None else finite(expected, "requested frequency", True)
    result["frequency_method"] = "interpolated mean-level rising crossings" if measured else "operator-specified frequency; not independently measured"
    if expected is not None and measured and abs(freq/expected-1) > .05:
        result["reasons"].append("measured frequency differs >5% from requested fit; investigate")
        return result
    # >=16 samples/cycle (well above Nyquist) and four cycles is a declared analysis policy.
    if fs/freq < 16 or (ts[-1]-ts[0])*freq < 4:
        result["reasons"].append("insufficient cycles or samples/cycle (policy: >=4 cycles and >=16 samples/cycle)")
        return result
    if settings.bandwidth_hz is None or settings.bandwidth_hz < freq:
        result["reasons"].append("effective probe/channel bandwidth unknown or below fundamental")
        return result
    fit = sinusoid(ts, subset, freq, count)
    if fit["peak_v"] <= 1e-12:
        result["reasons"].append("unobservable fitted fundamental")
        return result
    result.update(valid=True, fit=fit)
    if fs/(3*freq) >= 16 and (ts[-1]-ts[0])*freq >= 10 and settings.bandwidth_hz >= 3*freq:
        h2 = sinusoid(ts, subset, 2*freq, count)["peak_v"]
        h3 = sinusoid(ts, subset, 3*freq, count)["peak_v"]
        result["harmonics"] = dict(second_peak_v=h2, third_peak_v=h3, thd_2_3_estimate=math.hypot(h2,h3)/fit["peak_v"],
            limitation="Separate least-squares projections; finite-window leakage/noise included. Not total THD or a scope accuracy bound.")
    else:
        result["reasons"].append("harmonics unavailable: require 10 cycles, >=16 samples/third-harmonic cycle and known bandwidth")
    return result


def channel_metrics(wave, index, frequency=None):
    c = wave.config.channels[index]; values = wave.channels[index]; result = statistics(values)
    result["dc_valid"] = c.coupling == "DC"
    result["display_limit_samples"] = None if c.displayed_limits_v is None else sum(v <= c.displayed_limits_v[0] or v >= c.displayed_limits_v[1] for v in values)
    result["clipping_status"] = "UNKNOWN: no declared display/input limits" if c.displayed_limits_v is None else "LIMIT_CONTACT_OBSERVED" if result["display_limit_samples"] else "NO_DECLARED_LIMIT_CONTACT"
    result["fundamental"] = fundamental(wave, values, c, frequency)
    result["extrema_plateau_fraction"] = sum(v == result["min_v"] or v == result["max_v"] for v in values)/len(values)
    result["plateau_limitation"] = "Exact repeated extrema may be quantization or saturation; clipping cannot be certified without acquisition/display/headroom evidence."
    return result


def pair(wave):
    if len(wave.channels) != 2 or not wave.config.simultaneous:
        raise ValueError("simultaneous two-channel acquisition required; never align separate recordings")
    return wave.channels


def transition(wave, index):
    values = wave.channels[index]; count = max(1, len(values)//20)
    start = math.fsum(values[:count])/count; end = math.fsum(values[-count:])/count
    delta = end-start
    if abs(delta) <= 1e-12:
        return dict(valid=False, reason="no observable rail transition", baseline_v=start, end_v=end)
    t10 = crossings(wave.time, values, start+.1*delta, delta>0)
    t90 = crossings(wave.time, values, start+.9*delta, delta>0)
    valid = bool(t10 and t90 and t90[0] > t10[0])
    return dict(valid=valid, direction="rise" if delta>0 else "fall", baseline_v=start, end_v=end,
        start_10_s=t10[0] if t10 else None, end_90_s=t90[0] if t90 else None,
        duration_10_90_s=t90[0]-t10[0] if valid else None,
        repeated_threshold_crossings=len(t10)>1 or len(t90)>1,
        overshoot_v=max(0.,max(values)-max(start,end)), undershoot_v=max(0.,min(start,end)-min(values)),
        time_resolution_s=wave.interval_s)


def analyze(wave, preset="basic", parameters=None):
    p = dict(parameters or {}); warnings = list(wave.warnings)
    nets = [c.net for c in wave.config.channels]
    result = dict(schema="WTK_SCOPE_1", preset=preset, parameters=p, source=wave.source, import_config=asdict(wave.config),
        samples=len(wave.time), interval_s=wave.interval_s, sample_rate_hz=1/wave.interval_s,
        duration_s=wave.time[-1]-wave.time[0], jitter_fraction=wave.jitter_fraction,
        channels={net: channel_metrics(wave,i,p.get("frequency_hz")) for i,net in enumerate(nets)},
        warnings=warnings, analysis={}, electrical_status="NOT_TESTED", physically_qualified=False,
        evidence_state="SYNTHETIC" if wave.config.metadata["synthetic"] else "REAL_FILE_UNREVIEWED", safety=SAFETY)
    if not wave.config.metadata["synthetic"] and any(wave.config.metadata.get(k) in (None,"","UNKNOWN") for k in ("instrument","operator","capture_date")):
        result["evidence_state"]="INSUFFICIENT_EVIDENCE"
        warnings.append("record instrument identity, operator and capture date; unknown provenance cannot be reviewed physical evidence")
    if len(set(nets)) != len(nets):
        raise ValueError("channel nets must be distinct")
    if preset == "basic":
        if wave.jitter_fraction>.001 and not wave.config.metadata["synthetic"]:
            result["evidence_state"]="INSUFFICIENT_EVIDENCE"
        return result
    a,b = pair(wave)
    if preset in ("supply", "excitation", "clamp") and any(c.coupling != "DC" for c in wave.config.channels):
        raise ValueError("DC coupling required for rail/differential/clamp interpretation")
    if preset == "supply":
        if nets != ["+5V_A", "+3V3"]:
            raise ValueError("Supply requires CH1 +5V_A, CH2 +3V3 / actual GND")
        one,two = transition(wave,0),transition(wave,1)
        for rail in (one,two):
            if rail["valid"] and rail["duration_10_90_s"] < 4*wave.interval_s:
                warnings.append("rail 10–90% transition spans fewer than four intervals; timing is insufficiently resolved")
        result["analysis"] = dict(rail_5v=one,rail_3v3=two,relative_start_s=two["start_10_s"]-one["start_10_s"] if one["valid"] and two["valid"] else None,
             relative_end_s=two["end_90_s"]-one["end_90_s"] if one["valid"] and two["valid"] else None,
             regulator_inactive_observation=None)
        for net, bounds in (("+5V_A",(4.75,5.25)),("+3V3",(3.,3.6))):
            entry = result["channels"][net]
            entry["A05_stage2_window_v"] = bounds
            if entry["max_v"] > bounds[1]:
                warnings.append(net+" exceeds A05 engineering upper window; independent review/STOP investigation required")
            warnings.append(net+": low ramp values are not settled-rail failures; use independent DMM and recorded test state")
        interval = p.get("regulator_inactive_interval_s")
        if interval is not None:
            if len(interval)!=2 or finite(interval[0],"inactive interval") >= finite(interval[1],"inactive interval") or interval[0]<wave.time[0] or interval[1]>wave.time[-1] or not p.get("supply_state_evidence"):
                raise ValueError("inactive interval needs in-capture bounds and independent regulator-state evidence")
            points = array("d",(v for t,v in zip(wave.time,b) if interval[0]<=t<=interval[1]))
            if len(points)<4:
                raise ValueError("inactive interval insufficiently resolved")
            result["analysis"]["regulator_inactive_observation"] = statistics(points)
        warnings.append("No permitted sequencing delay invented. Apparent backfeed needs recorded regulator state, adapters and independent review.")
    elif preset == "excitation":
        if nets != ["VEXC", "VMID"] or p.get("frequency_hz") not in (10,1000,10000):
            raise ValueError("Excitation requires VEXC/VMID and expected 10/1000/10000 Hz")
        diff = array("d", (x-y for x,y in zip(a,b)))
        result["analysis"] = dict(VEXC_DIFF=statistics(diff), fundamental=fundamental(wave,diff,wave.config.channels[0],p["frequency_hz"]))
        # Both channels must support the requested band before their subtraction is evidence.
        bandwidths = [c.bandwidth_hz for c in wave.config.channels]
        if any(x is None or x < p["frequency_hz"] for x in bandwidths):
            result["analysis"]["fundamental"] = dict(valid=False, reasons=["differential pair bandwidth unknown/insufficient"], fit=None)
        if all(x is not None and x >= 450000 for x in bandwidths) and 1/wave.interval_s/450000>=16 and wave.jitter_fraction<=.001 and wave.interval_s*(min(len(diff),FIT_SAMPLES)-1)*450000>=4:
            result["analysis"]["carrier_450khz"] = dict(valid=True, fit=sinusoid(wave.time,diff,450000,min(len(diff),FIT_SAMPLES)),
                limitation="Targeted projection includes finite-window leakage; presence is not confirmed merely by a finite amplitude.")
        else:
            result["analysis"]["carrier_450khz"] = dict(valid=False, reason="INSUFFICIENT_EVIDENCE: 450 kHz needs known >=450 kHz effective bandwidth, uniform timestamps, >=16 samples/cycle and >=4 cycles")
        warnings.append("Only VEXC and VMID simultaneous. RET requires a separate run; absolute phase across runs is not synchronized.")
    elif preset == "clamp":
        resistor = finite(p.get("series_resistor_ohms"),"fitted series resistor",True)
        if not p.get("component") or not p.get("mapping_evidence"):
            raise ValueError("record actual output/ADC component and resistor mapping evidence")
        currents = array("d", ((x-y)/resistor for x,y in zip(a,b)))
        st = statistics(currents)
        result["analysis"] = dict(resistor_current=dict(peak_positive_a=max(0.,st["max_v"]), peak_negative_a=min(0.,st["min_v"]),rms_a=st["rms_v"],
            positive_duration_s=sum(wave.time[i+1]-wave.time[i] for i in range(len(currents)-1) if currents[i]>0),
            negative_duration_s=sum(wave.time[i+1]-wave.time[i] for i in range(len(currents)-1) if currents[i]<0),duration_method="left-sample interval occupancy; not interpolated diode conduction"),
            diode_current_a=None, actual_3v3_injection_a=None, current_bound_a=None)
        rail = p.get("measured_3v3_v")
        if rail is not None:
            rail = finite(rail,"independently measured 3V3",True)
            if not p.get("rail_evidence"):
                raise ValueError("3V3 voltage needs independent measurement provenance")
            result["analysis"]["adc_nominal_limits"] = dict(lower_v=0.,upper_v=rail,samples_above=sum(v>rail for v in b),samples_below=sum(v<0 for v in b),upper_margin_v=rail-max(b),lower_margin_v=min(b),
                limitation="Nominal GND..measured VDDA input window, not an absolute maximum/injection allowance; rail was independently recorded, not synchronized")
        errors = [c.voltage_error_bound_v for c in wave.config.channels]
        tol = p.get("resistor_tolerance_pct")
        if tol is not None:
            tol = finite(tol,"resistor tolerance")
            if not 0<=tol<100 or not p.get("resistor_evidence"):
                raise ValueError("resistor tolerance needs documented bound in [0,100)")
            if all(e is not None for e in errors):
                drop = max(abs(x-y) for x,y in zip(a,b))
                result["analysis"]["current_bound_a"] = (sum(errors)+drop*tol/100)/(resistor*(1-tol/100))
        warnings.append("I_R is estimated series-resistor current. It may include capacitive/input loading; possible BAT54S conduction and actual +3V3 injection are NOT directly measured or certified.")
    elif preset == "ret":
        if nets != ["RET_1X", "RET_HG"]:
            raise ValueError("RET requires explicitly mapped RET_1X/RET_HG")
        fits = [result["channels"][n]["fundamental"] for n in nets]
        result["analysis"] = dict(nominal_gain_context=1+68000/4700, gain_ratio=None, relative_phase_deg=None,
            dynamic_range="UNKNOWN: review scope limits, loading, residuals and amplifier/ADC headroom")
        if any(result["channels"][n]["display_limit_samples"] for n in nets):
            warnings.append("declared display-limit contact; gain/phase comparison rejected until usable dynamic range established")
        elif all(f["valid"] for f in fits):
            fa,fb = [f["fit"] for f in fits]
            if abs(fa["frequency_hz"]/fb["frequency_hz"]-1)>.001:
                warnings.append("paths have inconsistent measured frequency; ratio/phase unavailable")
            else:
                # Fit both at one common measured frequency, preserving the shared origin.
                freq = fa["frequency_hz"]
                fa,fb = [sinusoid(wave.time,v,freq,min(len(v),FIT_SAMPLES)) for v in (a,b)]
                result["analysis"].update(gain_ratio=fb["peak_v"]/fa["peak_v"],relative_phase_deg=(fb["phase_deg"]-fa["phase_deg"]+180)%360-180)
        warnings.append("Observed gain deviation is not a design-failure verdict. Check probe loading, coupling, channel skew, scope accuracy and operating point. Residual RMS includes noise AND distortion.")
    else:
        raise ValueError("unsupported analysis preset")
    if wave.jitter_fraction>.001:
        result["evidence_state"] = "SYNTHETIC" if wave.config.metadata["synthetic"] else "INSUFFICIENT_EVIDENCE"
    return result


def independent_comparison(first, second, index=0, frequency=None):
    """Separate recordings: amplitude-only comparison; no synchronized phase claim."""
    a,b = channel_metrics(first,index,frequency),channel_metrics(second,index,frequency)
    fa,fb = a["fundamental"],b["fundamental"]
    return dict(first_source=first.source,second_source=second.source,amplitude_ratio=fb["fit"]["peak_v"]/fa["fit"]["peak_v"] if fa["valid"] and fb["valid"] else None,
                relative_phase_deg=None, synchronized=False, warning="Independent clocks/triggers; absolute phases are not synchronized")


def envelope(wave, values, start=None, end=None, buckets=1024):
    """Min/max envelope + endpoints in original order, <=4096 points; preserves peaks."""
    import bisect
    left = 0 if start is None else bisect.bisect_left(wave.time,start)
    right = len(values) if end is None else bisect.bisect_right(wave.time,end)
    if right-left<2 or not 1<=buckets<=2048:
        raise ValueError("invalid plot interval/bucket bound")
    buckets=min(buckets,2047)
    step = max(1,math.ceil((right-left)/buckets)); points=[]
    for i in range(left,right,step):
        stop = min(right,i+step); low=high=i
        for j in range(i+1,stop):
            if values[j]<values[low]: low=j
            if values[j]>values[high]: high=j
        for j in sorted(set((low,high))):
            points.append((wave.time[j],values[j]))
    if points[0][0]!=wave.time[left]: points.insert(0,(wave.time[left],values[left]))
    if points[-1][0]!=wave.time[right-1]: points.append((wave.time[right-1],values[right-1]))
    return points


def summary(report):
    """Human operator view over canonical results, no separate calculations."""
    lines=[report["evidence_state"]+" | electrical acceptance NOT_TESTED | no physical accuracy qualification",
           f'{report["samples"]:,} samples | {report["sample_rate_hz"]:.8g} Sa/s | {report["duration_s"]:.8g} s',
           "Source SHA-256: "+report["source"]["sha256"]]
    for net,c in report["channels"].items():
        lines.append(f'{net}: min {c["min_v"]:.7g}, max {c["max_v"]:.7g}, pp {c["vpp_v"]:.7g}, mean {c["mean_v"]:.7g} V (DC valid: {c["dc_valid"]})')
        lines.append(f'  RMS {c["rms_v"]:.7g} / AC RMS {c["ac_rms_v"]:.7g} V | {c["clipping_status"]}')
        f=c["fundamental"]
        if f["valid"]:
            fit=f["fit"]; lines.append(f'  Fit {fit["frequency_hz"]:.8g} Hz, peak {fit["peak_v"]:.7g} V, phase {fit["phase_deg"]:.7g} deg at t={fit["phase_origin_s"]:.8g} s')
            lines.append("  "+f["frequency_method"]+f'; residual RMS {fit["residual_rms_v"]:.7g} V (noise + distortion)')
        else: lines.append("  Frequency/fit n/a: "+"; ".join(f["reasons"]))
    lines.append("Preset results:\n"+json.dumps(report["analysis"],indent=2,allow_nan=False))
    if "independent_comparison" in report:
        lines.append("Independent recording (no synchronized phase):\n"+json.dumps(report["independent_comparison"],indent=2))
    lines.append("Limitations:\n"+"\n".join(report["warnings"]))
    return "\n".join(lines)


def export_bundle(wave, report, out, b01=None, check=None, review=None):
    """New sidecar bundle; never modifies B01 entries or grants PASS/qualification."""
    import pc_bringup
    out = Path(out)
    if out.exists():
        raise ValueError("use a new evidence directory; prior evidence is immutable")
    if file_hash(wave.source["path"]) != wave.source["sha256"]:
        raise ValueError("source changed since import; evidence export refused")
    report = json.loads(json.dumps(report,allow_nan=False))
    if (report.get("schema") != "WTK_SCOPE_1" or report.get("source") != wave.source or
        report.get("physically_qualified") is not False or report.get("electrical_status") != "NOT_TESTED" or
        (wave.config.metadata["synthetic"] and report.get("evidence_state") != "SYNTHETIC")):
        raise ValueError("report/source provenance or nonqualification contract mismatch")
    compared_source = report.get("independent_comparison", {}).get("second_source")
    if compared_source and file_hash(compared_source["path"]) != compared_source["sha256"]:
        raise ValueError("independent comparison source changed")
    if b01 is not None:
        if check not in B01_KEYS:
            raise ValueError("unsupported B01 association")
        session = pc_bringup.load_json(b01)
        pc_bringup.validate_session(session.get("session",session))
        report["b01_sidecar"] = dict(path=str(Path(b01).resolve()),sha256=file_hash(b01),check=check,
            status="NOT_TESTED", instruction="Attach report path/hash manually; independently review electrical acceptance. No B01 entry was modified.")
    if review:
        if report["evidence_state"] == "SYNTHETIC":
            raise ValueError("synthetic evidence cannot become manually reviewed physical evidence")
        if not all(isinstance(review.get(k),str) and review[k].strip() for k in ("operator","date","rationale","evidence")):
            raise ValueError("manual review needs named/date/rationale/evidence provenance")
        report["manual_review"] = review
        if report["evidence_state"] != "INSUFFICIENT_EVIDENCE":
            report["evidence_state"] = "MANUALLY_REVIEWED"
    out.mkdir(parents=True)
    source = out/"source.csv"
    shutil.copyfile(wave.source["path"],source)
    if file_hash(source) != wave.source["sha256"]:
        raise ValueError("source changed while copying; incomplete bundle is not evidence")
    if compared_source:
        shutil.copyfile(compared_source["path"], out/"independent-source.csv")
        if file_hash(out/"independent-source.csv") != compared_source["sha256"]:
            raise ValueError("comparison source changed while copying")
        report["immutable_comparison_copy"] = "independent-source.csv"
    report["immutable_source_copy"] = "source.csv"
    (out/"report.json").write_text(json.dumps(report,indent=2,allow_nan=False)+"\n",encoding="utf-8")
    (out/"report.md").write_text("# Waveform evidence — "+report["evidence_state"]+"\n\n"+SAFETY+"\n\nNOT_TESTED electrical acceptance; no accuracy/safety or diode-current certificate.\n\n```text\n"+summary(report)+"\n```\n\nFull provenance and parameters:\n\n```json\n"+json.dumps(report,indent=2)+"\n```\n",encoding="utf-8")
    # Portable SVG requires no plotting dependency; bounded envelopes only.
    colors=("#146a9e","#ac5420")
    lo=min(min(v) for v in wave.channels); hi=max(max(v) for v in wave.channels); span=max(hi-lo,1e-12)
    svg=['<svg xmlns="http://www.w3.org/2000/svg" width="1000" height="420" viewBox="0 0 1000 420">', '<rect width="1000" height="420" fill="white"/>']
    import html
    svg.append('<text x="20" y="20">'+html.escape(report["evidence_state"]+" | voltage V / time s | "+wave.source["sha256"][:16])+'</text>')
    for i,values in enumerate(wave.channels):
        points=envelope(wave,values)
        coords=' '.join(f'{40+920*(t-wave.time[0])/(wave.time[-1]-wave.time[0]):.2f},{380-330*(v-lo)/span:.2f}' for t,v in points)
        svg.append(f'<polyline fill="none" stroke="{colors[i]}" stroke-width="1" points="{coords}"/>')
        svg.append(f'<text x="40" y="{40+18*i}" fill="{colors[i]}">'+html.escape(wave.config.channels[i].net)+'</text>')
    svg.append(f'<text x="40" y="405">{wave.time[0]:.8g} .. {wave.time[-1]:.8g} s | {lo:.8g} .. {hi:.8g} V | min/max envelope</text></svg>')
    (out/"waveform.svg").write_text('\n'.join(svg),encoding="utf-8")
    files=("source.csv","report.json","report.md","waveform.svg") + (("independent-source.csv",) if compared_source else ())
    (out/"manifest.json").write_text(json.dumps(dict(schema="WTK_SCOPE_BUNDLE_1",complete=True,
        files={name:file_hash(out/name) for name in files}),indent=2)+"\n",encoding="utf-8")
    return report


def verify_bundle(out):
    out=Path(out); path=out/"manifest.json"
    if not path.is_file() or path.stat().st_size>16384:
        raise ValueError("incomplete evidence bundle")
    manifest=json.loads(path.read_text(encoding="utf-8"))
    expected={"source.csv","report.json","report.md","waveform.svg"}
    if manifest.get("schema")!="WTK_SCOPE_BUNDLE_1" or manifest.get("complete") is not True or set(manifest.get("files",{})) not in (expected,expected|{"independent-source.csv"}):
        raise ValueError("invalid bundle manifest")
    if any(file_hash(out/name)!=digest for name,digest in manifest["files"].items()):
        raise ValueError("evidence bundle checksum mismatch")
    return manifest


def main():
    import argparse
    parser=argparse.ArgumentParser(description=__doc__)
    parser.add_argument("source",type=Path); parser.add_argument("--config",type=Path,required=True)
    parser.add_argument("--preset",choices=("basic","supply","excitation","clamp","ret"),default="basic")
    parser.add_argument("--parameters",type=Path); parser.add_argument("--out",type=Path,required=True)
    parser.add_argument("--b01",type=Path); parser.add_argument("--check",choices=B01_KEYS)
    args=parser.parse_args()
    try:
        value=settings_json(args.config); value["channels"]=tuple(Channel(**c) for c in value["channels"])
        wave=import_csv(args.source,ImportConfig(**value))
        parameters=settings_json(args.parameters) if args.parameters else {}
        export_bundle(wave,analyze(wave,args.preset,parameters),args.out,args.b01,args.check)
    except (ValueError,TypeError,KeyError,OSError) as exc:
        parser.error(str(exc))
    print(args.out/"report.md")


def settings_json(path):
    path=Path(path)
    if path.stat().st_size>16384:
        raise ValueError("settings/parameters exceed 16 KiB")
    return json.loads(path.read_text(encoding="utf-8"),parse_constant=lambda x: (_ for _ in ()).throw(ValueError("nonfinite settings")))


if __name__ == "__main__":
    main()
