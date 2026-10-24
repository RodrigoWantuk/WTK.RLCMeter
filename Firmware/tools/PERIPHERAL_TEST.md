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

## White-screen investigation, 2026-10-24

The owner confirms button tones but no visible pixels after reset. Module header
labels match J_TFT: VCC/GND/CS/RESET/DC/SDI/SCK/LED/SDO. A brief SWD halt/read/resume
without reset on image e1e49c2 found display READY, UI stage8/failed0, SPI2 SR=2,
GPIOB CRH=0x94922222 and ODR/IDR=0x1C18. Reset and CS are high; K1/range and
excitation are off. This proves MCU-side progress, not receipt by the LCD.

The peripheral variant now reads five read-only registers once before rendering:
D3,0A,0B,0C,0F, retaining four raw response bytes and each transfer status in
`g_display_readback` for SWD (26 bytes, no ISR ownership). No response is discarded
or automatically interpreted as identity: dummy/padding bits are retained.
`completed=5` only means all attempts finished. An all-zero/all-one response is
not presence evidence. BUSY defers, errors are recorded, and UI/safety continue.
Reads finish before any RAMWR stream starts and never recur during rendering.
Ordinary profiles do not call this diagnostic. Each operation holds TFT CS from
command through response, keeps Flash deselected, uses the existing mode0/PCLK1/8
clock, and releases ownership on error. No initialization voltages are changed.

Protocol reference: ILITEK ILI9341 V1.05, section7.1.10 and register descriptions
[manufacturer datasheet hosted by Waveshare](https://files.waveshare.com/upload/e/e3/ILI9341_DS.pdf).
Module-side VCC/reset/CS and any onboard regulator output are requested from the
owner. Display communication/rendering remain REQUIRES_BENCH_VALIDATION.

The e4c8633 image was subsequently programmed/read back and reset/run. SWD found
all five responses zero, software statuses0/completed5, UI stage8/failed0. The
shared-bus Flash probe returned EF4016/4MiB. The owner reports module VCC/CS/reset/
DC=3.3V. End-to-end unpowered resistance checks are pending. See the durable
[display investigation](../../docs/review/a05/Display-White-Screen-2026-10-24.md).

The next peripheral-only diagnostic uses PCLK1/64 for the TFT (562.5kHz with36MHz
PCLK1), keeping Flash at/8. Hardware reset holds50ms, then waits250ms. Common DCS
sequence: SWRESET(wait150),DISPOFF(20),SLPOUT(150),RGB565(10),MADCTL48(10),INVOFF(20),
NORON(20),DISPON(150), waits in milliseconds. No panel-specific voltage, VCOM,
gamma or porch configuration is guessed. This is a diagnostic common subset,
not a qualified ST7789 product driver. Internal CMake definition
WTK_ILI9341_DIAGNOSTIC_SUPPORT is1 only for this variant and its host driver test;
ordinary builds retain the original initialization command sequence.

Readback order becomes04,D3,09,0A,0B,0C,0F with five raw bytes/status each, completed7;
retained context43B. All reads finish before RAMWR drawing. Baseline init and
wrap-safe diagnostic timing, raw response preservation, CS release, slow-TFT/
unchanged-Flash clocks and quiet exclusion pass five relevant host tests. Arm
build/memory/composition checks pass for peripheral, ordinaryBRINGUP andPRODUCT.
The diagnostic image is14468B Flash/11788B accountedRAM. Physical outcome pending.

Common-command references: ILITEK datasheet above and
[Sitronix ST7789V datasheet hosted by Espressif](https://dl.espressif.com/dl/schematics/ST7789V_SPEC_V1.0.pdf).

Owner subsequently repaired a bad solder joint on the display flat and confirmed
visible rendering. The previous all-zero response/white screen did not establish
a defective module. For the owner's requested mounting, diagnostic MADCTL changes
from48 to88 and its logical rotation becomes2 (portrait180, still240x320). Ordinary
profiles keep MADCTL48. Colors, button rows/tones, clock and safety are unchanged.
Final rotated appearance remains an owner observation after deployment.

Deployed3d276b1 with exact readback and controlled boot. Owner confirms rotated
appearance and buttons. Post-repair LCD readback: MADCTL88 matches the requested
rotation, power-mode9C and diagnosticC0 are nonzero, UIstage8/failed0. ID04/D3 remain
zero, so controller identity is not established. Image14476B/11788B accountedRAM.
This closes the white-screen/rotation subtask; broader Phase03 criteria remain.
