# 04 — Safety, Power, and Range Control

STATUS: NOT_STARTED

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
