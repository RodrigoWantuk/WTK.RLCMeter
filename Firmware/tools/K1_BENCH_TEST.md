# BRINGUP empty-fixture K1 pulse

This is an explicit first-board relay/driver diagnostic requested by the owner.
It is not a measurement or calibration mode. Physical timing, coil current,
contacts, charger inhibition, reset and fault behavior remain
**REQUIRES_BENCH_VALIDATION** until independently observed.

## Build and scope

From `Firmware/`:

```text
cmake --preset stm32-bringup -B build/k1-bench -DWTK_BRINGUP_K1_TEST=ON
cmake --build build/k1-bench
```

Or prepare a sealed deployment artifact from the repository root:

```text
python Firmware/tools/pc_firmware_deploy.py build --profile BRINGUP --k1-bench-test --out Firmware/build/k1-handoff
```

The option defaults OFF and is accepted only for BRINGUP. It compiles the
restricted relay service and commands, while excluding the experimental DC
pilot to fit guaranteed C8 Flash. Ordinary BRINGUP, PRODUCT and BRINGUP_CAL keep
their existing behavior; no test runs automatically at boot. Host builds compile
the pure diagnostic service for tests. Manifests identify `bringup_k1_test` and
validate it against the generated configuration; legacy manifests mean OFF.

## Physical setup and commands

Use the independently reviewed, current-limited external 5 V supply with stable
5V_SYS/5V_A/3V3 rails, no charger, and verified K1 driver/coil/diode assembly. Allow
for the relay coil's nominal approximately 40 mA in addition to idle load; this
is calculated from the specified 5 V/125 ohm coil, not a measured board budget.
Stop on current limiting, heating, rail instability or unexpected actuation.

**Disconnect every DUT, capacitor and external source from TEST_HI/TEST_LO.**
Do not attach a component during the test. The firmware cannot prove an empty
fixture. The literal `empty` token is the operator's physical confirmation.
Do not attach a debugger or halt/step the CPU during an active pulse.

USART1 is 115200 8N1 with 3.3 V logic. Exact commands:

```text
lab k1 status
lab k1 pulse empty
lab k1 cancel
```

An accepted start reports `K1_TEST start=OK pulse_ms=250` and state PULSE. K1
energizes once, then is commanded LOW at the first scheduled step at/after
250 ms. The service retains ownership for the existing 8 ms release guard,
then reports state DONE and K1 SAFE. A second start in the same boot is refused;
an attempted HIGH consumes the one-shot even if the write fails. A rejected
safety check does not consume it. Reset initializes the one-shot but never
starts a pulse. `cancel` commands LOW immediately and completes the release
guard as ABORTED. There is no user-selected duration or latch-ON command.

The pulse deadline is cooperative, not an independent hardware cutoff. UART
input is bounded per loop and the service runs before the ordinary safety
arbiter and slow peripheral work. A delayed loop de-energizes on its next step;
an actual stalled CPU depends on the existing approximately 2 s IWDG reset and
hardware reset defaults. Timing and watchdog behavior require bench evidence.

## Separate safety contract

Before HIGH and on every active step, the diagnostic requires:

- no latched fault, HSE/PLL clock ready and valid VMID;
- charger ABSENT and a real residual SAFE snapshot no older than 20 ms;
- range DISABLED and excitation OFF;
- no acquisition, calibration, W25Q mutation, dump or shared-workspace owner;
- the explicit empty-fixture confirmation and K1 initially commanded SAFE.

This service deliberately does not require a battery sense value when an
external bench supply has been physically verified. It never fabricates
ADC_BAT or `measure_allowed`, clears faults, issues a measurement permit, or
changes ordinary acquisition/calibration battery checks. A floating VBAT_PROT
continues to block ordinary measurement. Supply verification is an operator
precondition; Rev.1 has no direct MCU measurement of the 5 V rail.

While the test owns K1, only its status/cancel commands are accepted. Buzzer is
muted through quiet mode; TFT initialization, background W25Q polling and other
console operations are deferred. A failed guard commands SAFE, stops any
unexpected excitation and disables the range. Failed LOW writes latch an I/O
fault and retain cleanup ownership with LOW retries; they never report DONE.
After confirmed LOW and the release guard, auxiliary residual evidence is
invalidated and the existing fresh eight-sweep qualification starts again.

Read UART state as **commanded state**, not contact feedback. Record the
coil/contacts, PB9, RANGE_EN, PA8 and rail/current observations independently.
Never use resistance/continuity mode on an energized circuit. Hearing one click
does not qualify contact routing, release time, interlocks or measurement accuracy.
