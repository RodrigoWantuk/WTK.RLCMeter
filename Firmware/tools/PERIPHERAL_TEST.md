# Standalone BRINGUP peripheral test

Owner-requested Phase03/09A bench diagnostic. `WTK_BRINGUP_PERIPHERAL_TEST=ON`
defaults OFF, requires BRINGUP, and is exclusive with the K1 pulse and external
bench-measurement variants. It excludes the experimental DC pilot and active Lab
console. Buttons cannot start a measurement, range change, calibration or Flash
write. GPIO safe boot, real auxiliary sensors/charger/fault checks and watchdog
remain active. Product/default BRINGUP/BRINGUP_CAL behavior is unchanged.

Build from a clean checkout using the canonical CMake-based deployment helper:

```powershell
python Firmware/tools/pc_firmware_deploy.py build --profile BRINGUP --peripheral-test --out Firmware/build/peripheral-handoff
```

Or configure/build from `Firmware/`:

```powershell
cmake --preset stm32-bringup -B build/peripheral -DWTK_BRINGUP_PERIPHERAL_TEST=ON
cmake --build build/peripheral
```

The manifest binds `bringup_peripheral_test` to the generated flag. Composition
checks reject active measurement/Lab/calibration mutation symbols in this image,
and peripheral UI symbols in ordinary profiles. W25Q is only probed; calibration
loading and the write-capable store adapter are not linked for this test.

After ILI9341 initialization, the automatic panel clears black, draws red/green/
blue bars, then shows WTK RLC SETUP, FW version, Git identity and BRINGUP PERIPHERAL.
Each debounced UP/OK/DOWN row shows PRESSED (green) or RELEASED (white). Rendering
is incremental:64-pixel fill blocks and one text glyph per step, with no full
framebuffer or external resources. Quiet mode pauses rendering and silences tones.
A failed display operation stops UI retries while the safety loop/watchdog run.

App policy plays120ms on each PRESS: UP660Hz, OK1000Hz, DOWN1500Hz. A newer press
preempts an older tone; release only updates the row. Faults block new tones.
UI code only handles presentation and button states; it never controls hardware
relays/ranges. Initial backlight remains the existing25% setting, not a current
limit or approval of a raw-LED connection.

Before connecting, power down and check the actual module pinout/supply/backlight
per A05R01/R02. The owner reported the backlight arrangement correct; no independent
current/module evidence was supplied. A white lit screen proves neither SPI nor
controller compatibility. UART may be physically removed for this test, with SWD
retained. Programming/readback and a controlled boot are separate stages; absence
of UART is not proof of boot failure or successful rendering.

Observe all three colors, readable identity and independent press/release state
for every button. Confirm the distinct tones. Record physical observations; host
mocks/build success cannot mark the display, wiring or buttons qualified. Keep
K1 de-energized, range disabled and excitation off. Leave battery/charger and DUT
testing to their separately reviewed procedures. No calibration record is created.

Software verification:42/42 host CTests (MSVC Debug) pass, including paused UI,
events arriving during rendering, simultaneous button state and sticky SPI error.
Deployment-tool tests:54 run,5 environment-dependent skips; profile-tool tests
include operation exclusion. Strict Arm builds pass for the new variant and
existing bench/defaultBRINGUP/PRODUCT profiles. The initial peripheral image is
14092B Flash/11740B accountedRAM including2048B reserved stack; panel context68B
and fill buffer128B are bounded, with no framebuffer. Physical panel/colors/keys/
tones and compatibility remain REQUIRES_BENCH_VALIDATION until owner observations.
