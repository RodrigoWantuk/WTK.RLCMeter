# 04 — Safety, Power, and Range Control

STATUS: IMPLEMENTED_REQUIRES_BENCH_VALIDATION

## Authoritative Rev.1 Firmware/Hardware Contract

This section is normative for Phase 04 and future firmware work. The values below are frozen firmware inputs for the manufactured Rev.1 baseline; future agents must not reinterpret them from older documents or partial schematic context.

### MCU pinout

```text
PA0  ADC_VEXC
PA1  ADC_VMID
PA2  ADC_RET_1X
PA3  ADC_RET_HG
PA4  ADC_OV_HI
PA5  ADC_OV_LO
PA6  ADC_BAT
PA7  ADC_NTC
PA8  PWM_EXC
PA9  DEBUG_TX
PA10 DEBUG_RX
PA11 K2_CMD
PA12 FLASH_CS
PA13 SWDIO
PA14 SWCLK
PA15 CHG_DETIO

PB0  TFT_BL
PB1  IO_BUZZ
PB3  SW_UP
PB4  SW_DOWN
PB5  RANGE_A0
PB6  RANGE_A1
PB7  RANGE_A2
PB8  RANGE_EN
PB9  K1_CMD
PB10 TFT_RST
PB11 TFT_DC
PB12 TFT_CS
PB13 TFT_SCK
PB14 TFT_MISO
PB15 TFT_MOSI

PC13 SW_OK
PC14 NC
PC15 NC
```

PA13/PA14 remain SWD. PA11 is K2_CMD and PB14 is TFT_MISO in Rev.1.

### K1 and charger inhibit

K1 is an HFD27/005-S monostable relay:

```text
coil OFF = SAFE
coil ON  = MEASURE

PB9 LOW  = SAFE
PB9 HIGH = MEASURE
```

The K1 driver is:

```text
PB9 -> 1 kohm -> BC817 base
BC817 emitter -> GND
BC817 collector -> K1 coil low
K1 coil high -> +5V_SYS
base pull-down = 100 kohm
```

Rev.1 also has a hardware charger inhibit:

```text
CHG_VBUS -> 10 kohm -> QUSB_INH BC817 base
base -> 100 kohm -> GND
collector -> K1_BASE
emitter -> GND
```

When charger power is present this hardware path physically prevents K1 energization. Firmware must still block MEASURE on charger detection and must never attempt to override the hardware inhibit.

Future K1 measurement sequencing must use these firmware guard times after commanding the relay:

```text
K1_OPERATE_GUARD_MS = 10
K1_RELEASE_GUARD_MS = 8
```

Both guard times are conservative firmware margins and remain `REQUIRES_BENCH_VALIDATION`.

### CHG_DETIO

```text
CHG_VBUS -> 10 kohm -> PA15 -> 12 kohm -> GND

PA15 LOW  = charger absent
PA15 HIGH = charger present
```

### K2 / low-Z bank

```text
PA11 = K2_CMD
K2 baseline = DNP
R0_BANK = 0 ohm
PA11 remains LOW
low-Z topology = fixed 0 ohm link
```

Runtime auto-detection is not used for Rev.1.

### Range bank

```text
PB5 = RANGE_A0
PB6 = RANGE_A1
PB7 = RANGE_A2
PB8 = RANGE_EN

RANGE_EN active HIGH
```

Exact map:

```text
A2 A1 A0
0  0  0   10 ohm
0  0  1   100 ohm
0  1  0   1 kohm
0  1  1   10 kohm
1  0  0   100 kohm
1  0  1   1 Mohm
1  1  0   INVALID / UNUSED
1  1  1   INVALID / UNUSED
```

Timing:

```text
RANGE_DEAD_TIME_MS   = 2
RANGE_SETTLE_TIME_MS = 5
```

Both timing values remain `REQUIRES_BENCH_VALIDATION`.

### Battery

```text
VBAT_PROT -> 100 kohm -> PA6 -> 100 kohm -> GND

VBAT = 2 * V_ADC_BAT

LOW      = 3.50 V
CRITICAL = 3.25 V
```

Battery thresholds remain `REQUIRES_BENCH_VALIDATION`.

### NTC

```text
+3V3 -> 100 kohm fixed resistor -> PA7 / ADC_NTC -> MF58-104J3950GB -> GND

R25  = 100 kohm
Beta = 3950 K
```

Stage 2 exposes resistance telemetry only; target firmware does not add a libm temperature conversion in this stage.

### Residual detector

Each residual channel is:

```text
terminal -> 560 kohm -> 560 kohm -> 560 kohm -> SENSE -> 27 kohm -> VMID
SENSE -> 10 nF -> VMID
SENSE -> 4.7 kohm -> ADC input
```

Therefore:

```text
RH = 1.68 Mohm
RL = 27 kohm

K = (RH + RL) / RL = 63.222222...

Vterminal = VMID + (Vsense - VMID) * 63.222222...
```

Do not document 39 kohm as the Rev.1 residual low resistor.

Residual policy:

```text
release threshold = 0.75 V
block threshold   = 1.00 V
required safe evaluations = 8

raw <= 16   -> saturated/invalid
raw >= 4079 -> saturated/invalid
```

All residual thresholds remain `REQUIRES_BENCH_VALIDATION`.

## Goal

Implement the hardware-service layer that controls SAFE/MEASURE, residual-voltage detection, charger/power state, battery/temperature sensing, and the six-range reference bank.

This phase must establish safety behavior before the metrology acquisition path is allowed to connect a DUT to the AFE.

## Prerequisites

- Phase 02 complete;
- pin polarity verified against the schematic;
- residual-voltage divider values and K1 wiring confirmed;
- UART diagnostics available.

Phase 03 is not strictly required except where TFT diagnostics are convenient.

## In scope

- auxiliary ADC setup needed for residual/battery/NTC readings;
- residual-voltage conversion and threshold framework;
- `CHG_VBUS` detection;
- battery voltage conversion;
- NTC conversion foundation;
- K1 safe service;
- K2 abstraction/variant handling;
- 74HC238 address + `RANGE_EN` service;
- safe state transitions;
- fault integration;
- diagnostics and host-testable policy logic.

## Out of scope

- final high-speed ADC/DMA metrology schedule;
- PWM excitation waveform;
- impedance calculation;
- automatic range-selection intelligence.

## Task 1 — Separate safety policy from ADC transport

Create pure conversion/policy functions where possible so host tests can verify threshold behavior independently of STM32 ADC setup.

Examples:

```text
raw ADC -> volts at sense node
sense volts -> estimated terminal residual voltage
status pair -> residual present / polarity / confidence
```

Do not hard-code policy directly inside HAL callbacks.

## Task 2 — Residual-voltage channels

Implement ADC reads for `ADC_OV_HI` and `ADC_OV_LO` with explicit scaling based on the documented divider.

Requirements:

- handle ADC full-scale/near-rail conditions as faults, not valid low residual;
- define raw plausibility checks;
- convert relative to VMID correctly;
- allow threshold values to be configured/versioned later;
- keep the **MEASURE-permission threshold** conservative and separate from the approximate ±100 V observation envelope.

Do not choose the final safe threshold based only on theory; mark it for bench qualification.

## Task 3 — Residual state model

Return a structured result, for example:

```text
valid
hi_voltage_estimate
lo_voltage_estimate
differential_estimate
residual_present
adc_saturated
reason
```

The application should be able to distinguish “safe,” “unsafe,” and “measurement invalid/unknown.” Unknown must fail closed.

## Task 4 — Charger detection

Implement PA15 `CHG_DETIO` reading after JTAG remap.

Requirements:

- active charger state prevents software MEASURE permission;
- diagnostics expose the state;
- code documents that a separate hardware interlock also exists;
- software must not attempt to defeat or work around that hardware interlock.

## Task 5 — Battery sensing

Implement `ADC_BAT` conversion using the actual divider ratio.

Provide raw voltage first. Percentage/state-of-charge estimation can remain simple or deferred because 1S Li-ion voltage is load dependent.

Expose at least:

```text
battery_voltage
low_battery flag
critical_battery flag (policy may remain provisional)
```

Thresholds require validation against the actual charge/boost module behavior.

## Task 6 — NTC sensing

Implement NTC raw/resistance/temperature conversion based on MF58-104J3950GB nominal parameters.

Requirements:

- detect open/short implausible values;
- expose raw ADC plus temperature estimate;
- keep temperature compensation out of measurement math until calibration data justifies it.

## Task 7 — K1 service

Implement K1 through semantic APIs only:

```text
relay_force_safe()
relay_request_measure()
relay_get_commanded_state()
```

`relay_request_measure()` must not blindly energize K1. It should require a safety token/status or be called only from a higher `safety_enter_measure()` operation that rechecks prerequisites.

Minimum prerequisites:

- charger absent;
- residual status valid and below threshold;
- range state valid;
- supply state acceptable;
- no critical application fault.

## Task 8 — K2 abstraction

Current baseline may have K2 DNP with `R0_BANK=0 Ω`.

Create a compile-time hardware-revision configuration so higher layers do not care whether K2 is populated.

If K2 is absent, its API should resolve to the fixed baseline topology without pretending a relay physically switched.

## Task 9 — Range-address service

Implement range enumeration:

```text
10 Ω
100 Ω
1 kΩ
10 kΩ
100 kΩ
1 MΩ
```

The service owns PB5/PB6/PB7 and PB8.

Required transition:

```text
RANGE_EN=0
set A0/A1/A2
wait dead-time using non-blocking state or bounded low-level delay justified by switching timing
RANGE_EN=1
wait settling before measurement is allowed
```

Prefer a non-blocking state machine so application control remains responsive.

## Task 10 — One-hot and invalid-state policy

74HC238 provides one-hot decode when enabled. Firmware still must:

- map enum -> address explicitly;
- reject invalid enums;
- disable bank on faults;
- expose current selected range plus whether the bank is enabled;
- start with bank disabled after reset.

## Task 11 — Safety service

Create a single coherent safety-status API aggregating:

```text
charger
residual validity/value
supply/battery critical status
ADC validity
range validity
application fault state
```

Representative output:

```text
SAFE_ALLOWED
MEASURE_ALLOWED
BLOCKED_RESIDUAL
BLOCKED_CHARGER
BLOCKED_RANGE
BLOCKED_SUPPLY
BLOCKED_SENSOR_INVALID
BLOCKED_FAULT
```

Host-test the policy matrix extensively.

## Task 12 — Application-state integration

Implement at least:

```text
SAFE_CHECK
WAIT_SAFE
READY
```

Do not yet implement full measurement acquisition states if Phase 05 is absent.

The application should visibly/logically refuse a measurement request when safety gates fail.

## Automated acceptance criteria

- all pure safety-policy tests pass;
- invalid/unknown sensor states fail closed;
- range transition code cannot update address while enabled;
- K1 cannot be energized through a UI/driver shortcut;
- build remains warning clean according to project policy;
- Phase 01 host tests remain green.

## Bench acceptance criteria

1. verify `CHG_VBUS` state and hardware lockout;
2. verify residual ADC polarity at 0 V and small controlled positive/negative voltages;
3. verify divider conversion against a DMM;
4. validate saturation/invalid handling before increasing voltage;
5. verify battery ADC against DMM;
6. verify NTC temperature plausibility;
7. verify K1 remains de-energized on reset/fault/charger/residual conditions;
8. verify each RREF address one at a time;
9. verify bank is disabled between transitions;
10. verify no two range branches are simultaneously selected.

The final residual-safe threshold remains `REQUIRES_BENCH_VALIDATION` until characterized.

## Handoff

Report:

- ADC scaling constants;
- provisional vs qualified thresholds;
- K1 polarity/behavior measured;
- K2 population configuration;
- range address map;
- dead-time/settling assumptions;
- safety policy test matrix;
- hardware anomalies;
- readiness for Phase 05.

## Stage 1 implementation status

Stage 1 is implemented in firmware and host tests. Phase 04 remains `IN_PROGRESS` because ADC transport, physical bench validation, and later safety integration stages are still pending.

Implemented on 2026-08-19:

- Pure fail-closed safety policy module with charger, residual, battery, range, and application-fault inputs.
- Deterministic blocker precedence:
  1. `BLOCKED_FAULT`
  2. `BLOCKED_CHARGER`
  3. `BLOCKED_SENSOR_INVALID`
  4. `BLOCKED_RESIDUAL`
  5. `BLOCKED_SUPPLY`
  6. `BLOCKED_RANGE`
  7. `MEASURE_ALLOWED`
- K1 semantic service:
  - PB9 LOW = SAFE;
  - PB9 HIGH = MEASURE;
  - `hw_k1_request_measure()` requires a fully allowed `hw_safety_result_t`;
  - application runtime does not call the MEASURE request path in Stage 1.
- K2 Rev.1 topology service:
  - K2 populated = false;
  - low-Z bank mode = fixed 0 ohm link;
  - physical switch request returns `BSP_STATUS_NOT_SUPPORTED`;
  - PA11 remains LOW.
- Charger semantic service:
  - PA15 LOW = `ABSENT`;
  - PA15 HIGH = `PRESENT`;
  - GPIO read failure = `UNKNOWN`.
- Range FSM service:
  - PB5/PB6/PB7 map A0/A1/A2;
  - PB8 `RANGE_EN` is active HIGH;
  - exact address map 0..5 for 10 ohm through 1 Mohm;
  - addresses 6 and 7 rejected;
  - `RANGE_DEAD_TIME_MS = 2`;
  - `RANGE_SETTLE_TIME_MS = 5`;
  - new requests during transition restart safely from disabled state;
  - address writes are rejected while the service believes `RANGE_EN` is HIGH.
- Provisional residual policy:
  - `RESIDUAL_RELEASE_V = 0.75 V`;
  - `RESIDUAL_BLOCK_V = 1.00 V`;
  - eight consecutive release evaluations are required before SAFE;
  - saturated/invalid samples fail closed.
- Pure helper constants:
  - residual transfer ratio uses 1.68 Mohm / 27 kohm, `K ~= 63.2222222`;
  - battery divider ratio is 2x, with 3.50 V LOW and 3.25 V CRITICAL thresholds;
  - NTC helper uses fixed 100 kohm top resistor and NTC bottom leg.
- Minimal application safety foundation:
  - conceptual `SAFE_CHECK`, `WAIT_SAFE`, `READY` states;
  - runtime remains `WAIT_SAFE` because residual and battery sensor transport are not qualified in Stage 1;
  - K1 is forced SAFE whenever policy blocks;
  - no fake sensor data can grant READY/MEASURE.
- Lab-only diagnostics:
  - `lab range 10r|100r|1k|10k|100k|1m`;
  - `lab range off`;
  - `lab range status`;
  - `lab safety status`;
  - `lab charger status`.

Host-tested:

- safety policy nominal and blocker matrix;
- blocker flags and precedence;
- recovery after blockers clear;
- K1 SAFE/MEASURE command polarity through service-level callbacks;
- denied/NULL K1 permissions force or leave SAFE;
- K2 Rev.1 DNP/fixed-link behavior;
- charger GPIO semantic mapping;
- all six range mappings and invalid 6/7 rejection;
- range transition dead/settle timing;
- no address change while enabled;
- force-disable from every FSM state;
- replacement request during transition;
- residual release/block/hysteresis/saturation policy;
- residual transfer and battery/NTC helper math.

Validation run on 2026-08-19:

- host Debug CTest: 10/10 passed;
- host Release CTest: 10/10 passed;
- Python tooling unittest: 13 passed;
- STM32 Debug build: passed;
- STM32 Release build: passed, Flash 12900 B, RAM 2496 B;
- STM32 Lab build: passed, Flash 14484 B, RAM 2680 B;
- Wokwi `--lint-only`: passed with Wokwi CLI `0.26.1 (9d71b975b7eb)`;
- Wokwi smoke scenarios were not executed because `WOKWI_CLI_TOKEN` is not set.

REQUIRES_BENCH_VALIDATION:

- actual PB9/PB8/PB5/PB6/PB7/PA11 electrical levels and external circuit response;
- hardware charger inhibit through QUSB_INH;
- PA15 threshold behavior on the real divider;
- residual ADC transport, polarity, saturation behavior, and provisional safety thresholds;
- battery and NTC ADC readings against DMM/temperature evidence;
- relay, range decoder, ULN/PNP/MOSFET one-hot behavior and timing.

## Stage 2 implementation status

Stage 2: `IMPLEMENTED_REQUIRES_BENCH_VALIDATION`.

Phase 04 remains `IN_PROGRESS`; K1 MEASURE activation, Stage 3 integration, and physical validation are still out of scope for this status.

Implemented on 2026-08-19:

- This plan now contains the authoritative Rev.1 firmware/hardware contract for pinout, K1, charger inhibit, PA15 charger semantics, K2 DNP/fixed-link topology, range map/timing, battery divider, NTC divider, residual divider, residual transfer ratio, residual thresholds, and ADC rail guards.
- Range fail-closed behavior was hardened:
  - `hw_range_force_disabled()` now returns `bsp_status_t`;
  - a failed `RANGE_EN LOW` write leaves the range state `INVALID`, not `DISABLED`;
  - a failed disable before a request prevents address updates;
  - a failed address write after a successful disable leaves the bank disabled and state `INVALID`;
  - a failed enable after dead-time leaves the state `INVALID` and never proceeds to settling/ready;
  - any I/O uncertainty maps to `HW_RANGE_INVALID`.
- K1 failure semantics were hardened:
  - `hw_k1_force_safe()` returns the LOW-write status;
  - a failed LOW write does not change the commanded state to SAFE;
  - denied/NULL measure requests attempt LOW and return the LOW-write failure if that command fails;
  - a failed HIGH write does not change the commanded state to MEASURE.
- Added reset-only latched safety faults:
  - `APP_SAFETY_FAULT_GPIO_INIT`;
  - `APP_SAFETY_FAULT_K1_IO`;
  - `APP_SAFETY_FAULT_K2_IO`;
  - `APP_SAFETY_FAULT_RANGE_IO`;
  - `APP_SAFETY_FAULT_ADC_INIT`;
  - `APP_SAFETY_FAULT_ADC_RUNTIME`.
- The application now captures and acts on safe GPIO, K1, K2, range, ADC, and auxiliary-sensor initialization/runtime statuses. Any latched safety fault sets `application_fault=true`, blocks MEASURE, commands K1 SAFE, and commands `RANGE_EN` LOW.
- The application no longer hardcodes `application_fault=false`, residual UNKNOWN, and battery UNKNOWN. It consumes the real Stage 2 sensor snapshot and freshness policy.
- K1 still remains physically SAFE in runtime. The normal application does not call `hw_k1_request_measure()` in Stage 2, even if diagnostics report `MEASURE_ALLOWED`.
- Added `bsp_adc` for Stage 2 auxiliary ADC1 ownership:
  - ADC1;
  - 12-bit right-aligned raw values;
  - nominal `VDDA_NOMINAL_V = 3.300 V`;
  - ADC clock remains the Phase 02 12 MHz clock tree result;
  - channels PA1/PA4/PA5/PA6/PA7 only;
  - sample time 239.5 ADC cycles on every Stage 2 auxiliary channel;
  - reset calibration and calibration with `ADC_CALIBRATION_TIMEOUT_MS = 10`;
  - non-blocking one-shot `start` / `poll` / `cancel`;
  - `ADC_CONVERSION_TIMEOUT_MS = 2`.
- Added host-testable `bsp_adc_core` state logic for start/busy/poll/complete/timeout/cancel behavior.
- Added cooperative `hw_aux_sensors` manager:
  - one ADC conversion at a time;
  - no watchdog service inside the ADC/sensor driver;
  - four conversions per published value;
  - residual priority over battery and NTC;
  - residual sweep: 4 x VMID, 4 x OV_HI, 4 x OV_LO;
  - battery sweep: 4 x ADC_BAT;
  - NTC sweep: 4 x ADC_NTC;
  - residual period 10 ms, max age 50 ms;
  - battery period 500 ms, max age 2000 ms;
  - NTC period 1000 ms, max age 5000 ms;
  - pause/resume/is-idle contract for later Phase 05 ADC ownership.
- Residual conversion now uses actual averaged VMID/OV_HI/OV_LO samples:
  - `Vadc = raw * 3.300 / 4095`;
  - `1.35 V <= VMID <= 1.95 V` required;
  - individual OV_HI/OV_LO samples at raw <= 16 or raw >= 4079 mark the evaluation saturated;
  - `V_SAFE_HI = VMID + (V_OV_HI - VMID) * 63.222222...`;
  - `V_SAFE_LO = VMID + (V_OV_LO - VMID) * 63.222222...`;
  - `V_DIFF = V_SAFE_HI - V_SAFE_LO`;
  - the Stage 1 residual hysteresis policy remains unchanged.
- Battery conversion uses `VBAT = 2 * V_ADC_BAT` and classifies OK/LOW/CRITICAL/UNKNOWN using the frozen 3.50 V / 3.25 V thresholds.
- NTC telemetry uses `Rntc = 100000 * Vn / (3.300 - Vn)`, exposes raw average, ADC voltage, resistance, validity, and timestamp/age, and does not add target libm temperature conversion.
- Lab diagnostics now include:
  - `lab sensors status`;
  - `lab adc status`;
  - `lab fault status`.
- Safety diagnostics now emit when the application safety state or primary blocker changes, not every superloop iteration.

Host-tested:

- range I/O-fault injection for failed disable from READY and SETTLING, failed address write, failed enable after dead-time, failed force-disable, and failed replacement disable;
- K1 failed LOW write visibility and state preservation;
- reset-only fault latch behavior, accumulation, safety blocking, and reset/reinit clearing;
- ADC channel mapping, invalid-channel rejection, busy rejection, poll busy, completion, timeout, and cancel;
- sensor FSM residual sequence, four-sample averaging, no partial publication, 63.222222 transfer math, saturated individual OV sample, invalid VMID, eight safe sweeps to SAFE, stale residual to UNKNOWN, battery OK/LOW/CRITICAL/stale, NTC resistance/valid/stale, ADC error faulting, partial-group discard, pause/resume.

Validation run on 2026-08-19:

- host Debug CTest: 13/13 passed;
- host Release CTest: 13/13 passed;
- Python tooling unittest: 13 passed;
- STM32 Debug build: passed, Flash 14576 B, RAM 2680 B;
- STM32 Release build: passed, Flash 17456 B, RAM 2688 B;
- STM32 Lab build: passed, Flash 21204 B, RAM 2872 B;
- Wokwi `--lint-only`: passed with Wokwi CLI `0.26.1 (9d71b975b7eb)`;
- Wokwi smoke/full scenarios were not executed because `WOKWI_CLI_TOKEN` is not set.

Release growth relative to the Stage 1 baseline:

```text
Stage 1 Release: Flash 12900 B, RAM 2496 B
Stage 2 Release: Flash 17456 B, RAM 2688 B
Growth:          Flash +4556 B, RAM +192 B
```

Lab growth relative to the Stage 1 baseline:

```text
Stage 1 Lab: Flash 14484 B, RAM 2680 B
Stage 2 Lab: Flash 21204 B, RAM 2872 B
Growth:      Flash +6720 B, RAM +192 B
```

This growth is meaningful but expected: Stage 2 adds ADC1 BSP code, cooperative sensor snapshots/FSM state, fault-latch logic, additional safety diagnostics, and host-testable ADC/sensor logic. The firmware remains within the guaranteed 64 KiB Flash and 20 KiB SRAM limits.

Evidence classification:

- ADC register/configuration build: `HOST_TESTED` where pure, `REQUIRES_BENCH_VALIDATION` physically;
- residual mathematical conversion: `HOST_TESTED`, `REQUIRES_BENCH_VALIDATION` against real voltages;
- battery conversion: `HOST_TESTED`, `REQUIRES_BENCH_VALIDATION` against DMM;
- NTC resistance conversion: `HOST_TESTED`, `REQUIRES_BENCH_VALIDATION` against known resistance/temperature;
- range/K1/K2/charger digital service behavior: `HOST_TESTED`, `REQUIRES_BENCH_VALIDATION` physically.

## Stage 3 implementation status

Stage 3: `IMPLEMENTED_REQUIRES_BENCH_VALIDATION`.

Implemented on 2026-08-19:

- Residual acquisition now treats any individual VMID/OV_HI/OV_LO sample at raw <= 16 or raw >= 4079 as a saturated residual group. Saturation is not hidden by four-sample averaging.
- Battery acquisition now treats any individual ADC_BAT rail sample as `HW_BATTERY_UNKNOWN`, and applies the frozen Rev.1 plausibility ceiling `HW_BATTERY_MAX_PLAUSIBLE_V = 4.35 V`.
- The residual hysteresis FSM now retains `SAFE` after the initial eight-sweep qualification while all valid nonsaturated magnitudes remain below the 1.00 V block threshold. Returning from the hysteresis band to the release band does not require another eight samples unless SAFE was lost.
- Sensor snapshots normalize stale semantics at read time:
  - residual age > 50 ms returns `HW_RESIDUAL_UNKNOWN` and safe-count 0;
  - battery age > 2000 ms returns `HW_BATTERY_UNKNOWN`;
  - NTC age > 5000 ms returns invalid resistance/temperature flags.
- `hw_aux_sensors_pause()` immediately invalidates published residual evidence and reinitializes the residual policy. Battery and NTC continue to age under their normal freshness rules. After resume, eight fresh safe residual sweeps are required before `SAFE` can be published again.
- `bsp_adc` now applies `BSP_ADC_POWER_STABILIZATION_US = 2` after ADC1 is powered with ADON during initialization and cancel/recovery. This bounded microsecond-level hardware stabilization wait is isolated in the BSP; normal auxiliary acquisition remains cooperative and non-blocking.
- `bsp_adc_cancel()` now power-cycles/re-enables ADC1 deterministically, clears stale conversion status/data, and returns the ADC core to IDLE with `BSP_ADC_CHANNEL_INVALID`.
- NTC diagnostics now include a provisional fixed-LUT temperature estimate from the frozen MF58-104J3950GB nominal model, without target libm. Temperature is valid only from -20 C through +80 C; resistance telemetry may remain valid outside that range, but temperature is not extrapolated.
- Added pure `hw_measure_permit` for future Phase 05 entry:
  - permit TTL is 5 ms;
  - issue requires charger absent, residual SAFE age <= 20 ms, battery OK/LOW age <= 1000 ms, range READY with a valid Rev.1 range ID, K1 commanded SAFE, no latched safety fault, and `hw_safety_evaluate()` allowing MEASURE;
  - validation after auxiliary ADC pause requires the permit to be valid/unconsumed, age <= 5 ms, charger still absent, range still READY with the same range ID, K1 still SAFE, and fault mask still zero;
  - every validation attempt consumes the permit, whether validation succeeds or fails.
- Added `lab permit status`, which reports current eligibility/reason without issuing or consuming a real permit.
- `hw_k1_request_measure()` remains uncalled by production application code and is documented as reserved for the future authorized measurement sequencer.

Future Phase 05 post-measure residual rule:

After K1 has been returned LOW and the 8 ms release guard has completed, old residual evidence is invalid. Auxiliary ADC must resume and the normal eight-evaluation SAFE qualification must complete again before any new permit can be issued. No immediate retry shortcut may bypass this rule.

Stage 3 host-tested:

- VMID individual rail saturation;
- battery individual rail saturation and >4.35 V plausibility;
- corrected residual SAFE hysteresis retention, including 0.70 V / 0.80 V / 0.99 V stay-SAFE, 1.00 V UNSAFE, and SAFE -> invalid -> UNKNOWN requalification;
- stale snapshot normalization without requiring `hw_aux_sensors_step()`;
- residual invalidation across pause/resume, including UNKNOWN immediately after resume before the first new sweep;
- NTC LUT exact points/interpolation/out-of-range behavior;
- measurement permit issue denial matrix, including range INVALID and unused addresses 6/7;
- permit validation at age 0 ms and 5 ms, expiry at 6 ms, dynamic blockers, disabled-range invalidation, and single-use consumption on both success and failure;
- ADC recovery/stabilization constants, cancel-to-IDLE core state, and start-after-cancel acceptance.

Validation run on 2026-08-19:

- host Debug CTest: 14/14 passed;
- host Release CTest: 14/14 passed;
- Python tooling unittest: 13 passed;
- STM32 Debug build: passed, Flash 15344 B, RAM 2688 B;
- STM32 Release build: passed, Flash 18292 B, RAM 2696 B;
- STM32 Lab build: passed, Flash 23012 B, RAM 2880 B;
- Wokwi `--lint-only`: passed with Wokwi CLI `0.26.1 (9d71b975b7eb)`;
- Wokwi smoke: executed with a live token on 2026-08-19; 0/4 scenarios passed (all timed out with empty USART logs);
- Wokwi full suite: not run after smoke failure;
- no `VIRTUAL_HARDWARE_TESTED` claim. See `plans/03A-Wokwi-Virtual-Hardware-Validation.md`.

Release growth relative to the Stage 2 baseline:

```text
Stage 2 Release: Flash 17456 B, RAM 2688 B
Stage 3 Release: Flash 18292 B, RAM 2696 B
Growth:          Flash +836 B, RAM +8 B
```

Lab growth relative to the Stage 2 baseline:

```text
Stage 2 Lab: Flash 21204 B, RAM 2872 B
Stage 3 Lab: Flash 23012 B, RAM 2880 B
Growth:      Flash +1808 B, RAM +8 B
```

The Release growth is expected from the NTC fixed lookup table, sensor hardening paths, and pure permit helpers retained where referenced. `logf()`/libm is not linked, and Lab diagnostic strings remain dead-stripped from Release.

Phase 04 software implementation is closed at `IMPLEMENTED_REQUIRES_BENCH_VALIDATION`. Physical bench validation remains open and no Phase 05 implementation has started.

Bench plan prepared for future hardware validation:

1. power current-limited;
2. verify K1 LOW;
3. verify `RANGE_EN` LOW;
4. read raw VMID;
5. compare VMID against DMM;
6. with DUT terminals at 0 V, record OV_HI/OV_LO;
7. apply small controlled +1 V / -1 V;
8. compare residual conversion against DMM;
9. only then increase controlled residual test voltage;
10. validate battery ADC against DMM;
11. validate NTC against known ambient/reference;
12. validate charger PA15 and hardware QUSB_INH;
13. validate all six range outputs.

Do not apply high voltage as the first residual test.
