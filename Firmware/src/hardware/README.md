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
- 500 mVrms is not allowed with the 10 Ω RREF;
- active `CHG_VBUS` prevents MEASURE;
- critical faults stop excitation and return K1 to SAFE;
- UI never controls relays/ranges directly.

K2 is a low-Z-bank contingency. The physical baseline uses `R0_BANK=0 Ω` and K2 DNP; the service boundary should allow a future populated variant without scattering hardware conditionals through higher layers.

`hw_peripherals_request_quiet(true)` is the semantic quiet-mode entry point for future acquisition windows. It asks the BSP to block new shared-peripheral transactions and immediately stops any active buzzer tone without changing the backlight PWM duty. Clearing quiet mode only reopens peripherals; it does not replay an interrupted tone.
