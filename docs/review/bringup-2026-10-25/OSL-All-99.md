# Complete capture campaign, incomplete calibration — October 25, 2026

Status: ALL_99_CAPTURED; 30_FITS_VALID; THREE_GAIN_OBSERVABILITY_FAILURES; NOT_QUALIFIED.

The owner freshly measured the final reference as 996 kohm and confirmed direct
attachment alone. Nominal 1 Mohm +/-1% remains an uncertain fit anchor, with
inferred band identification and no certified actual value. All six 1-Mohm raw
captures passed stable acquisition and SAFE teardown; four fits passed and the
two 10-kHz fits remain blocked. No acquisition or safety gate was relaxed.

The preserved same-image source-7017e5e0 cohort contains 33 OPEN, 33 SHORT and
33 LOAD observations. CRC, SHA, nominal ADC provenance and exact device identity
were verified. Thirty triplets pass strict float32 OSL checks. Three do not:

| RREF | Frequency | Amplitude | Missing evidence |
| --- | --- | --- | --- |
| 100 kohm | 10 kHz | 100 mVrms | Actual same-key HG overlap; OPEN/LOAD are HG-only |
| 1 Mohm | 10 kHz | 100 mVrms | Actual same-key HG overlap; OPEN/LOAD are HG-only |
| 1 Mohm | 10 kHz | 500 mVrms | Actual same-key HG overlap; OPEN/LOAD are HG-only |

After the owner removed the final resistor and confirmed OPEN, exactly these
three OPEN conditions were reacquired once. Each again has only HG usable and
no observed HG overlap; the strict fits remain blocked. The original 99 frames
are unchanged. Diagnostic frames are separate, not silent replacements.

A SAFE/idle no-reset RAM snapshot of the last 1-Mohm/10-kHz/500-mVrms LOAD
confirms six accepted blocks, zero rejected blocks and the actual repeatability
gate: 1X impedance relative standard deviation 8.3275%, HG 0.9996%, observed
gain 7.5451%, against the existing 2% stability limit. Source-amplitude dispersions
are 0.1929% and 0.1544%. These are raw repeatability metrics, not accuracy or SNR
qualification. They explain this rejection without assigning a component fault.

The full candidate builder was attempted offline and rejected the missing valid
path. The existing Python and C contracts require actual HG overlap for an HG
fit and a full 33-key Rev.1 set for installation. No candidate was generated or
installed, no flags or coefficients were fabricated, and no gain was borrowed
from another key. The board remains SAFE/idle, zero safety faults, calibration
blank. Protocol counter stayed at 1 throughout both final sweeps. W25Q resources
and mutable calibration slots were not written.

Battery before 1-Mohm capture: 3.70535 V, OK; post-campaign snapshot: 3.70374 V,
OK, nominal ADC scale. The owner requires a check before every new resistor.
The contact thermometer will support a separate one-point NTC comparison next;
no thermometer model is requested or stored. A single point does not determine
Beta or qualify the full temperature curve. Its stable reference reading and
any persistent adjustment are still pending.

High-Z 10-kHz observability/coverage, independent holdouts, scope-dependent checks
and calibration installation remain unfinished. Do not mark the device fully
calibrated or release-qualified based on the 99-frame count.

See [structured audit](OSL-All-99.json),
[physical evidence](../../../Firmware/releases/2026-10-25/osl-all-99-checkpoint/README.md)
and [the first blocked-key checkpoint](OSL-Load-100000.md).
