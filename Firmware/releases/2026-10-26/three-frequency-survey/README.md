# Three-frequency factory PRODUCT survey

Source: 0443d3295bd92043493ae805c2763464109ec3fb. Unit: 54ff70068370525760380567.
Implementation/deployment and owner-triggered basic capacitor/inductor checks
are observed. This is not absolute accuracy or polarized-electrolytic qualification.
No new resistor calibration campaign was performed.

## Behavior

Factory PRODUCT surveys **1 kHz -> 10 kHz -> 10 Hz** before a normal final R/C/L,
OPEN or SHORT conclusion. RREF selection adapts inside each stage: at most four
conditions per frequency, twelve raw search records, nine range moves and two
frequency changes. Initial hints cannot skip frequencies. Hardware/cancel
failures still terminate immediately. Weak acquired DSP data can rerange lower
with a present source or advance frequency; invalid/stale processing output is
rejected. Attempted and usable coverage are distinct. F3/3 reports attempted
coverage, not accuracy or three qualified observations.

Low-frequency resistance dominance can coexist with high-frequency inductance;
high-frequency resistance dominance can coexist with low-frequency capacitance.
Opposite reactive signs and inconsistent trends remain UNKNOWN. A resistance-
dominated classification requires usable unambiguous observations at all three
frequencies; sub-ohm uncertain data stays IMPEDANCE/unknown under the documented
approximately 1-ohm lower engineering target. No reactance threshold is lowered to force L.
One supported primary condition supplies the existing seven-point R/X trimmed
estimate; derived magnitude/phase/C/L/conditional Q/D use that exact frequency.
Raw search history and separate retained-coordinate masks remain available.

Fast hardware ownership keeps the original 500-ms continuous and battery-age
limits. App DRAINING completes SAFE/ADC/auxiliary/quiet/workspace cleanup before
10 Hz or another fast segment after its six-search-capture budget; fresh real
permits are required. Ten Hz retains 800-ms sine settling and 400-ms ADC capture,
SAFE per capture, including statistical repeats. **Multiple relay cycles are
intentional for this guarded low-frequency path.** Seven low-frequency windows
therefore take seconds. A 60-s overall app bound covers the finite twelve-search/
eight-extra workflow without granting a 60-s continuous K1 authorization.

Already-SAFE statistical completion now returns FINAL_RESULT on its terminal
step, so PRODUCT replaces the earlier partial. Drained cancel/cleanup failures
also deliver their final event. Session and actual PRODUCT-view regressions cover
these integration boundaries. Legacy/full-wizard/fixed profiles keep their
original policy and storage formats; no new drive frequencies are introduced.

## Actual basic observations

| Owner-confirmed fixture | Final result | Primary | Raw search + extra windows | Firmware start to last K1 SAFE |
| --- | ---: | --- | ---: | ---: |
| 4,700-uF electrolytic | 4,196.611 uF | 10 Hz / RREF10 / HG | 5 + 6 | 9,428 ms |
| 470-uH inductor | 466.910 uH | 1 kHz / RREF10 / HG | 3 + 6 | 1,742 ms |

Both final results have attempted/usable masks 7/7, seven admitted primary
points, final nonpartial PRODUCT view, calibration 1/all 33, hardware IDLE,
K1 SAFE, range/excitation OFF, quiet/workspace released and no hardware fault.
Independent replay reproduces coordinate masks (capacitor R25/X73, inductor
R84/X22), complex means and final C/L. Durations are reconstructed from firmware
app-start and final release deadlines; they exclude final processing/publication
and do not measure contact latency or sum connected intervals.

The inductor's actual 10-Hz point was locally RESISTIVE (about 7.433+j0.0524 Ohm),
while 1/10 kHz were INDUCTIVE. Combined classification correctly stays INDUCTIVE;
the low-frequency scalar L is not averaged into publication. The latest retained
inductor result is session 5; intermediate sessions 3/4 were overwritten and are
not claimed. The capacitor's session 2 is an owner-triggered engineering
observation with no independent capacitance reference; its deviation from the
marked value cannot be assigned to component tolerance or instrument error.
The actual 10 mF upper endpoint and polarized excitation envelope remain pending.

The first corrected attempt canceled after two retained OK cycles 165 ms apart.
A subsequent single longer press completed. Separately, the first integration
image internally computed about 4,224 uF but left a stale partial in PRODUCT;
that display-publication bug is fixed. The initial boot diagnostic also paused
session 1 during acquisition: that tooling error was resumed, explicitly
invalidated and preserved. Corrected boot/result checks use live DAP with stable
identity/image/output/sequence guards, no postboot halt/reset, and no remote
acquisition. An initial unpolled OpenOCD unknown state safely deferred a later
preflight before any programming. None of these discontinued runs is labeled
as a successful accuracy test.

## Deployment and validation

Internal programming and complete readback match SHA-256
26cb50c86e798d0ca0b040ddd5ab3a6a1f8e9791145a504bd2e8c6f46fa8e20f
(60,136 B). Corrected controlled boot captured exactly one SAFE_BOOT on COM4,
READY/SAFE with zero sessions/captures and all four factory feature flags. Active
engineering OSL sequence 1/all 33, assets/settings and the UID-bound experimental
NTC profile are preserved; no external-Flash erase/write or SWD battery-voltage
read was requested. UART boot was captured; completed data came from nonhalting
live SWD, since PRODUCT does not expose a result/start serial command. All DUT
measurements were initiated by owner buttons. TFT was disconnected for COM4;
final PRODUCT-view data is observed, physical appearance remains to be checked.

Host CTest **49/49 factory**, **47/47 standard** pass, including real PRODUCT-view
final delivery, weak DSP recovery, synthetic 4,700/10,000-uF and R/C/L vectors,
all-frequency/endpoint limits, sub-ohm ambiguity, contradictory signs, per-stage
budget, seven-point estimates, cleanup/cancel/overall deadline and all existing
regressions. All nine strict UID-bound embedded profiles pass:

| Profile | Flash bytes | Accounted RAM bytes | Three-frequency survey |
| --- | ---: | ---: | --- |
| factory-release | 60136 | 17080 | ON |
| bringup | 63828 | 15380 | OFF |
| product-debug | 61280 | 15200 | OFF |
| product-release | 64036 | 15520 | OFF |
| factory-debug | 59708 | 17000 | ON |
| bringup-cal | 37572 | 13744 | OFF |
| bench-supply | 65272 | 15468 | OFF |
| k1-test | 65392 | 15396 | OFF |
| peripheral | 14628 | 12252 | OFF |

Factory uses 17,080 B accounted RAM: above the preferred 16,384 B target but below
the unchanged 17,408 B hard gate, including the 2-KiB stack reservation. Twelve
156-B raw attempt records cost 936 B more than six; this traceability allocation
is explicit. No extra ADC buffer, heap, framebuffer, acquisition-profile change,
new persistent format, calibration rewrite or NTC coefficient change occurs.
BSP/drivers/hardware and DSP/calibration equations are unchanged. Stack high-water,
phase/loss/accuracy, analog switching/abort timing, sub-uH sensitivity and the
actual 10 mF/polarized envelope remain REQUIRES_BENCH_VALIDATION.

firmware-images.zip contains all nine images/maps/ELFs and the guarded factory
manifest. validation-evidence.zip preserves final and discontinued runs, raw
points, layouts, exact readbacks, source audit, host/embedded logs and procedures.
See checkpoint.json and [plan 26](../../../../plans/26-Three-Frequency-Product-Survey.md).

Archived discontinued procedures are retained for forensic traceability only;
the original boot diagnostic is labeled UNSAFE_DO_NOT_RUN. Use the corrected
live snapshot/boot procedures for the matching final image.
