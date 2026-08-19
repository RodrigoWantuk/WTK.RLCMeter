# `hardware`

Instrument-specific hardware services.

## Planned modules

```text
hw_safety.c/.h
hw_relays.c/.h
hw_range.c/.h
hw_excitation.c/.h
hw_power.c/.h
hw_battery.c/.h
hw_temperature.c/.h
hw_backlight.c/.h
hw_buzzer.c/.h
hw_peripherals.c/.h
```

This layer turns BSP-level pins/peripherals into semantically safe instrument operations.

## Phase 04 Stage 1 services

The current Stage 1 implementation adds host-testable safety and control foundations without enabling application MEASURE behavior:

- `hw_safety`: fail-closed policy evaluation with deterministic blocker precedence;
- `hw_residual`: provisional residual-voltage release/block policy and nominal transfer helpers;
- `hw_k1`: semantic K1 service where SAFE is PB9 LOW and MEASURE is PB9 HIGH, gated by `hw_safety_result_t`;
- `hw_k2`: Rev.1 topology service reporting K2 not populated and the low-Z bank fixed through the 0 ohm link;
- `hw_range`: non-blocking range FSM for PB5/PB6/PB7/PB8 with 2 ms dead time and 5 ms settle time;
- `hw_charger`: PA15 semantic mapping, LOW absent and HIGH present, with read failures mapped to UNKNOWN;
- `hw_power`: provisional battery divider/threshold constants and NTC divider resistance helper.

The live application initializes K1/K2/range/charger services and remains fail-closed because residual ADC transport is not yet qualified. Lab range commands may exercise the range bank, but no Lab command can force K1 into MEASURE.

## Representative API

```text
safety_force_safe()
safety_measure_allowed()
relay_set_safe()
relay_set_measure()
range_disable()
range_select()
excitation_configure()
excitation_stop()
charger_connected()
battery_read()
temperature_read()
backlight_set()
buzzer_play()
peripherals_request_quiet()
```

## Invariants

- K1 de-energized is SAFE;
- `RANGE_EN=0` while the range address changes;
- `RANGE_EN` is active HIGH and addresses 0..5 map to 10 ohm, 100 ohm, 1 kohm, 10 kohm, 100 kohm, and 1 Mohm;
- PA11 is K2_CMD and remains LOW for the Rev.1 DNP K2 baseline;
- 500 mVrms is not allowed with the 10 Ω RREF;
- active `CHG_VBUS` prevents MEASURE;
- critical faults stop excitation and return K1 to SAFE;
- UI never controls relays/ranges directly.

K2 is a low-Z-bank contingency. The physical baseline uses `R0_BANK=0 Ω` and K2 DNP; the service boundary should allow a future populated variant without scattering hardware conditionals through higher layers.

`hw_peripherals_request_quiet(true)` is the semantic quiet-mode entry point for future acquisition windows. It asks the BSP to block new shared-peripheral transactions and immediately stops any active buzzer tone without changing the backlight PWM duty. Clearing quiet mode only reopens peripherals; it does not replay an interrupted tone.
