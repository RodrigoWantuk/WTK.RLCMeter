# Safety and Protection

## October24 integration baseline

CHG_VBUS/J_PWR.4 is dimensioned for positive0..5.5V detection with commonGND and normal3.3V board power. The10k/12k divider gives nominal3.0V atPA15 for5.5V. This is not a5.5V system-supply or unpowered/backfeed rating; independent inhibition still needs bench verification.

See [current evidence, input limits and release status](review/bringup-2026-10-24/README.md).

## Scope

WTK.RLCMeter Rev.1 is intended for **de-energized passive components**. The SAFE network is designed to detect and tolerate residual voltage within its intended envelope before connecting the DUT to the measurement AFE, but this does not make the instrument CAT-rated or suitable for direct connection to mains.

Never connect the Rev.1 RLC input directly to an energized high-voltage circuit.

## Fail-safe state

K1 de-energized = SAFE.

```text
TEST_HI -> SAFE_HI
TEST_LO -> SAFE_LO
```

K1 energized = MEASURE.

```text
TEST_HI -> RET
TEST_LO -> VMID
```

Firmware must assume SAFE during boot, reset, fault, watchdog recovery, and brownout handling.

## Residual-voltage detector

Each side uses a high-value divider referenced to VMID:

```text
SAFE_x -> 560k -> 560k -> 560k -> SENSE_x
                                  |
                                 27k
                                  |
                                 VMID
```

With `RTOP = 1.68 MΩ` and `RLOW = 27 kΩ`:

```text
k = 27k / (1.68M + 27k) ≈ 0.015817
VSENSE = VMID + k * (VIN - VMID)
```

For VMID near 1.65 V, the network provides an approximate observation envelope around ±100 V without exceeding the ADC range under the intended conditions.

**The threshold that permits K1 to enter MEASURE must be far below 100 V and will be established through firmware policy and bench validation.**

## Bleeder

```text
SAFE_HI -- 47k -- 47k -- SAFE_LO
```

Total resistance: 94 kΩ.

Discharge time depends on DUT capacitance:

```text
tau = 94k * C_DUT
```

Firmware must never assume that a fixed delay means the DUT is discharged; residual voltage must be measured again.

## ADC protection

Residual-voltage ADC channels use series resistance and BAT54S clamps. The 1.68 MΩ source resistance strongly limits clamp current during residual-voltage events.

## Charger interlock

`CHG_VBUS` is handled in two layers:

1. **Hardware:** the charger interlock prevents K1 from entering MEASURE.
2. **Firmware:** a divided digital signal is read on PA15 for policy, UI, and diagnostics.

Safety must not depend only on the firmware-readable charger signal.

## TVS option

`D_TVS` and `R_TVS_LINK` form an optional protection branch across the test terminals.

First assembly: **DNP**.

Reason: TVS capacitance, leakage, nonlinearity, and temperature dependence can degrade high-impedance and small-capacitance measurements. The footprint exists for controlled A/B evaluation after the baseline metrology is characterized.

## Fault policy

Any of the following conditions must abort or prevent MEASURE and return K1 to SAFE:

- residual voltage above the permitted threshold;
- `CHG_VBUS` / charger detect active;
- relevant ADC failure;
- invalid or indeterminate range state;
- supply/brownout condition that makes measurement unreliable;
- watchdog/reset recovery;
- impossible application-state transition;
- explicit safety fault.

Baseline fault action:

```text
stop excitation
RANGE_EN = 0
K1 -> SAFE
K2 -> safe/default state
buzzer -> off
record fault
report fault through diagnostics when available
```

## Restricted empty-fixture BRINGUP relay diagnostic

The owner also explicitly requested a separate external-supply measurement image.
`WTK_BRINGUP_BENCH_SUPPLY=ON` permits a RAM-only `lab bench supply external`
selection after idle/fresh residual/VMID/clock/charger/fault checks. It substitutes
the operator-confirmed external supply policy for battery state/age only; ADC_BAT
is unchanged. It binds permits to the source and aborts on revocation, limits DUT
transactions to nominal 100 mVrms, and preserves all other safety gates and safe
cleanup. Default BRINGUP, PRODUCT and BRINGUP_CAL retain their existing behavior.
No automatic measurement or valid calibration is created. See
[the bench supply contract](../Firmware/tools/BENCH_SUPPLY.md).

The owner-requested `WTK_BRINGUP_K1_TEST=ON` variant permits one 250 ms K1 pulse
per boot with no DUT, range disabled, excitation off, fresh real residual SAFE,
valid VMID, charger absent and no fault or competing operation. It has a
separate hardware-service state machine; it never grants measurement permission
or substitutes a battery reading. An independently verified bench supply is the
operator precondition for this diagnostic when VBAT_PROT is unconnected.
Ordinary measurement/calibration still requires the existing battery checks.
It is absent from PRODUCT and BRINGUP_CAL and never runs automatically.
See [the full contract and physical procedure](../Firmware/tools/K1_BENCH_TEST.md).
Physical relay, timeout, fault and interlock behavior remain REQUIRES_BENCH_VALIDATION.


## Bounded continuous measurement session (2026-10-26)

PC-provisioned PRODUCT can retain K1 in MEASURE for one automatic search after
one initially fresh, range-bound, single-use permit is issued and consumed.
This is continuous ownership, not reuse of that token for another K1 activation.
The hardware FSM permits at most six captures, uses a 500-ms cooperative
connection deadline, and also expires at the original battery evidence's
1,000-ms age limit. The actual release latency depends on foreground scheduling;
physical deadline, transient and interlock timing remain REQUIRES_BENCH_VALIDATION.

Between captures ADC/DMA is stopped, excitation is neutral and settled, and
quiet/auxiliary-ADC/workspace ownership is retained through bounded DSP. Only
owned RANGE_REQUEST/RANGE_WAIT can admit the range service's intentional
DISABLED/TRANSITIONING states, with neutral excitation and existing RANGE_EN
break-before-address/dead-time/settling. INVALID or an unexpected state/ID still
aborts. Live charger/unknown charger, fault, DMA error, ownership loss,
continuation limits and cancellation revoke the session. Ten-Hz/DC and fixed
calibration/BRINGUP transactions retain SAFE per capture.

The SAFE residual dividers cannot observe the DUT while K1 connects it to the
AFE. No fresh residual reading is fabricated while the auxiliary ADC is paused.
The DUT must stay connected and unpowered for the complete session. Final
publication waits for K1 SAFE/release guard, range disabled, excitation off,
auxiliary ADC restore/resume, quiet and workspace release. Restore/cleanup errors
revoke even a provisionally successful mathematical result. A subsequent
session requires real residual requalification and a new initial permit.

See [plan 24](../plans/24-Measurement-Evidence-Session-and-UI.md). This change
does not extend the de-energized-passive-component or voltage envelope.

## Three-frequency survey segmentation

Factory PRODUCT includes 10 Hz last, without extending continuous authorization.
App DRAINING completes SAFE/ADC/auxiliary/quiet/workspace cleanup before a
10-Hz capture or another fast segment after its search-capture budget. Cleanup
fault/cancel prevents restarting. Each new segment obtains fresh real residual,
power, clock, charger and range permission. The first statistical window after
SAFE uses the normal initial-permit path; later fast repeats use the bounded,
condition-locked continuation role. The 500-ms connected and original battery
expiry remain unchanged.

Existing 10-Hz fixed capture uses 800-ms sine settling and 400-ms coherent ADC
capture, live charger/fault checks and full SAFE cleanup. It remains outside the
fast held-session optimization. Every repeated 10-Hz statistical capture renews
permission after real auxiliary/residual sampling. Multiple relay cycles are
intentional under this contract; no fresh battery/residual evidence is invented
while ADC auxiliaries are paused. An overall 60-s application survey bound
limits the at-most-twelve search/eight-extra workflow; it does not grant a
60-s continuous K1 authorization or supersede hardware timeouts.

Unpowered passive DUT and existing residual/polarized-component restrictions
remain. All-frequency software coverage does not qualify the bipolar AC mode
for 4,700/10,000-uF electrolytics. No automatic boot measurement is added.
