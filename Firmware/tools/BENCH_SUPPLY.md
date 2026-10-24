# BRINGUP with an external bench supply

After a completed/failed acquisition, `lab adc capture status` reports a bounded
`ADC_TRACE ... ADC_TRACE_END` snapshot taken before stop/restore. It records ADC
control/status, DMA remaining words and TIM2 state without reading ADC data
registers or halting the core. It is a bench-only diagnostic; `valid=0` means no
capture snapshot exists. Use `ms` and the capture transcript to associate evidence
with an attempt. No retry, reset or capture is triggered by this command.

The owner requested a conservative 30 ms K1 operate guard on 2026-10-24.
Excitation settling remains a separate step; the 8 ms release guard is unchanged.
This policy delay is not proof of mechanical/electrical settling. The first real
10 kOhm attempt reached ADC/DMA TIMEOUT and safely returned to SAFE; no resistance
result exists from that attempt. This is an acquisition-completion fault, not
evidence that a longer contact delay alone fixes the problem.

Owner-requested first-board diagnostic, Phase 09A. This image allows uncalibrated
AC diagnosis of de-energized passive fixtures without a battery. It is not PRODUCT
operation, battery emulation, calibration, or electrical qualification.

## Build

From the repository root:

```text
python Firmware/tools/pc_firmware_deploy.py build --profile BRINGUP --bench-supply --out Firmware/build/bench-supply-image
```

Equivalent CMake configuration from `Firmware/`:

```text
cmake --preset stm32-bringup -B build/bench-supply -DWTK_BRINGUP_BENCH_SUPPLY=ON
cmake --build build/bench-supply
```

The option defaults OFF and is rejected for PRODUCT/BRINGUP_CAL. This separate
image excludes the experimental DC pilot and the optional empty-fixture K1 pulse
to preserve guaranteed C8 Flash. Host tests compile the policy for regression.
Deployment manifests bind `bringup_bench_supply` to the generated build flag.

## Operator setup and activation

Use the already reviewed current-limited external 5 V supply at J_PWR.2 and GND
at J_PWR.3, with physically verified stable 5V_SYS, 5V_A and 3V3. Leave VBAT_PROT
unconnected when there is no protected 1S battery; never connect 5 V to J_PWR.1.
ST-Link 3V3 and TTL VCC remain disconnected; UART logic is 3.3 V. Charger must
remain absent. Use only an empty fixture, a shorting link, or an isolated passive
reference; no external voltage or charged capacitor. Keep the fixture open while
arming, and change fixtures only after K1 SAFE and range DISABLED are reported.

The board cannot directly monitor its 5 V rail. Selecting the external source is
the operator's attestation of the supply setup, not an ADC measurement. Stop on
heating, unstable rails, unexpected actuation or current limiting. No debugger
halt/step during a capture. Actual timing/watchdog/electrical behavior remains
REQUIRES_BENCH_VALIDATION.

At 115200 8N1:

```text
lab bench status
lab bench supply external
lab bench status
```

Every boot starts with `source=BATTERY_REQUIRED`. The explicit external command
requires idle resources, K1 SAFE, range disabled, excitation OFF, no fault, HSE/PLL,
charger ABSENT, valid VMID and real residual SAFE no older than 20 ms. Acceptance:

```text
BENCH_SUPPLY source=EXTERNAL_5V_CONFIRMED max_amp_mvrms=100 qualification=UNQUALIFIED
```

The setting is RAM-only. No measurement or calibration starts automatically.
Revoke it with `lab bench supply battery` or RESET. Revocation during an active
measurement invalidates its supply authorization on the next FSM step and runs
the existing abort cleanup. Do not disconnect the physical supply to test this.

## Measurements without valid calibration

Existing fixed-condition diagnostic commands are preferred for first references:

```text
lab metrology measure 1k 100m 1k
lab metrology measure 1k 100m 10k
lab metrology measure 1k 100m 100k
```

Select a nominal RREF near the known reference resistance. For the first test use
1 kOhm or 10 kOhm. The existing raw `lab metrology capture ...` command keeps K1
SAFE and observes the internal AFE, so it must not be mistaken for a DUT-contact
OPEN/SHORT test. `lab metrology measure ...` uses the full DUT relay transaction.
Use `pc_capture.py` for validation of the existing RAW protocol. The console's
frequency tokens are `10`, `1k`, `10k`; amplitude tokens are `100m`, `500m`.

`lab measure auto` also retains its existing ideal-fallback diagnostic behavior,
but attempts requesting 500 mVrms are refused in external-supply mode. Prefer
fixed 100 mVrms conditions until the analog path is understood. The amplitude
class is nominal; it is not a measured or calibrated amplitude guarantee.

Missing calibration remains missing. Nominal 3.3 V ADC scale, nominal RREF and
nominal HG response may be used only by the existing explicit BRINGUP fallback;
results are IDEAL_UNQUALIFIED, not 1% measurements. No calibration record is
created or committed by selecting the source or taking measurements. A 1%
resistor is a useful interval reference, not an exact standard. OPEN may be
singular and SHORT may fail the signal-quality gate; preserve raw evidence and
do not label those outcomes as wiring failures by themselves.

## Safety and evidence

Only the battery-state/age requirement changes after explicit external-source
confirmation. ADC_BAT, voltage and UNKNOWN state are never fabricated. Ordinary
default BRINGUP and PRODUCT retain their battery policy. The existing residual
freshness, charger, fault, clock, range, single-use permit, K1 guards, ADC/DMA
timeouts, quiet mode and safe teardown remain in force. Permits bind the selected
source at issue/validation, and active external-source transactions check for
revocation. External-source DUT transactions reject the 500 mVrms class before K1.

Record firmware/manifest, `lab bench status`, actual supply evidence, fixture ID,
RAW/result, reference tolerance and final K1/range/fault status together. UART
states are software state, not contact/decoder feedback. Physical RREF, MOSFET,
one-hot selection, excitation amplitude, AFE gain/phase and accuracy all remain
REQUIRES_BENCH_VALIDATION.
