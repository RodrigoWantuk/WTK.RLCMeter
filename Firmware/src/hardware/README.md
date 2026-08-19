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

## Phase 04 Stage 2 services

Stage 2 adds the auxiliary ADC and runtime sensor path while keeping K1 physically SAFE in the application:

- `bsp_adc`: ADC1 one-shot start/poll/cancel API, ADC1 channels 1/4/5/6/7, 239.5-cycle sample time, bounded calibration timeout, and 2 ms conversion timeout;
- `bsp_adc_core`: host-testable ADC busy/complete/timeout/cancel state logic;
- `app_safety_fault`: reset-only latched internal fault bitmask for safety-critical GPIO/K1/K2/range/ADC failures;
- `hw_aux_sensors`: cooperative four-sample averaging FSM for residual, battery, and NTC telemetry.

The sensor manager uses these fixed Stage 2 cadences:

```text
Residual sweep: 10 ms period, 50 ms max age
Battery sweep:  500 ms period, 2000 ms max age
NTC sweep:      1000 ms period, 5000 ms max age
```

Residual sweeps collect four VMID, four OV_HI, and four OV_LO conversions before publishing one residual evaluation. Battery and NTC publish only after four conversions. Stale/invalid residual and battery states fail closed through the safety policy. NTC is telemetry-only in Stage 2.

## Phase 04 Stage 3 services

Stage 3 hardens the Stage 2 sensor semantics and closes the Phase 04 software foundation while still keeping K1 physically SAFE in the application:

- individual saturated VMID, OV_HI, OV_LO, and ADC_BAT samples are not hidden by averaging;
- battery telemetry rejects rail samples and values above the frozen 4.35 V plausibility ceiling;
- residual SAFE hysteresis is retained after the initial eight safe evaluations until a block, invalid, or saturated sample occurs;
- stale snapshots normalize residual, battery, and NTC semantic validity at read time;
- pausing auxiliary ADC immediately invalidates residual evidence so future metrology ownership cannot reuse a stale SAFE result;
- NTC diagnostics expose a fixed-LUT temperature estimate without target libm;
- `hw_measure_permit` provides a pure, short-lived, single-use pre-measure permit object for the future Phase 05 measurement sequencer.

Normal application code still does not call `hw_k1_request_measure()`. That API is reserved for the future authorized measurement sequencer after bench-qualified prerequisites exist.

## Phase 05 Stage 1 services

Stage 1 adds the deterministic metrology transport foundation while K1 remains physically SAFE:

- `hw_metrology_clock`: pure HSE/PLL 72 MHz contract gate;
- `APP_SAFETY_FAULT_CLOCK`: latched at boot when metrology clock contract fails;
- `hw_excitation` + `bsp_excitation`: TIM1 450 kHz carrier, 45-point LUT, OFF/NEUTRAL/SINE, DMA1 Ch5;
- `hw_metrology_raw`: packed dual-ADC layout, unpack, hard-clip scan;
- `bsp_metrology_adc`: dual ADC1/ADC2, TIM2 internal trigger, DMA1 Ch1, static 768-word buffer;
- `hw_metrology_session`: host-tested non-blocking Lab capture FSM;
- Lab command `lab metrology capture` (Lab build only).

No DSP, no production MEASURE, no K1 energization in Stage 1.

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
