# WTK.RLCMeter

WTK.RLCMeter is a portable two-wire RLC meter based on the **STM32F103C8T6**, designed to characterize passive components using controlled AC excitation, synchronous acquisition, automatic range selection, and complex calibration.

The project combines a custom mixed-signal PCB, an **STM32 Blue Pill** module, an **ILI9341** TFT display, external **W25Q** SPI Flash, a 1-cell Li-ion power subsystem, and dedicated firmware for measurement, safety, user interface, storage, and diagnostics.

> **Current status — October24,2026:** basic R/C/L and display/buttons/buzzer observations are recorded. Current CAD includes six RGS gate-toGND pull-downs; shared firmware includes acquisition/phase/relay fixes and mounted display orientation. Calibration and battery/charger/metrology qualification remain pending. See [bring-up consolidation](docs/review/bringup-2026-10-24/README.md).

October 25 first-unit update: [all 99 OSL captures](docs/review/bringup-2026-10-25/OSL-All-99.md)
are preserved, with 30 valid fits and three high-Z/10-kHz fits still blocked.
An owner-authorized [experimental NTC factory profile](docs/review/bringup-2026-10-25/NTC-Factory-Profile.md)
is deployed on the first UID and included in nine unit builds. It is unqualified;
generic builds retain nominal NTC conversion. Independent metrology/power
qualification remains unfinished.

A subsequent [coherent calibration pilot](docs/review/bringup-2026-10-25/Coherent-Calibration.md)
recovers actual1X/HG overlap for all three problematic OPEN keys without relaxing
the stability criterion. All 33 engineering OSL fits now pass Python and C
validation using 15 current and 18 explicitly audited complete original triplets.
Sequence 1 is physically installed, with exact full W25Q readback before and
after reboot. The subsequent [calibrated PRODUCT handoff](Firmware/releases/2026-10-25/calibrated-product-ready/README.md)
now boots to READY with sequence 1, all 33 records, resources OK and SAFE outputs.
No measurement is started at boot. The latest pre-PRODUCT battery read was
3.873 V over UART. The owner confirms splash and READY; the subsequent display
update improves rendering speed, duplicate painting and default brightness.
Independent accuracy, scope and final-lead validation remain pending. See
[installation evidence](Firmware/releases/2026-10-25/installation-residual-requalification/README.md).

The subsequent [display update](Firmware/releases/2026-10-25/display-feedback/README.md)
restores normal TFT speed, coalesces READY painting and improves default brightness.
[Reactive publication recovery](Firmware/releases/2026-10-25/reactive-publication/README.md)
separates dominant C/L reactance from unresolved negative estimated loss. Two
actual 103J100 measurements publish 9.55/9.68 nF with SAFE teardown and unchanged
calibration. A subsequent [nominal 470-uH check](Firmware/releases/2026-10-25/inductor-470-check/README.md)
publishes 461.4/472.2 uH as INDUCTIVE with SAFE teardown. Very small inductors
and independent accuracy/loss validation remain pending; no repeated resistor
campaign is needed for these app fixes.

## Project goals

- Measure **resistance, capacitance, inductance, and complex impedance**.
- Derive `R`, `X`, `|Z|`, phase, and, where meaningful, ESR, Q, and D.
- Baseline test frequencies: **10 Hz, 1 kHz, and 10 kHz**.
- Planned excitation levels: **100 mVrms** and **500 mVrms**, selected according to range and headroom; 500 mVrms is not allowed with the 10 Ω reference range.
- Automatic selection among six reference impedances: **10 Ω, 100 Ω, 1 kΩ, 10 kΩ, 100 kΩ, and 1 MΩ**.
- Initial engineering targets, still subject to qualification: approximately **1 Ω–10 MΩ**, **1 nF–10 mF**, and **10 µH–10 H**.
- Synchronous acquisition using the STM32 internal ADCs, without an external ADC in Rev.1.
- Detect residual voltage and protect the analog front-end before connecting a charged DUT.
- Provide a responsive TFT UI with measurement screens, derived visualizations, diagnostics, PWM backlight, and audible feedback.
- Keep the PCB two-layer and favor components/footprints that remain practical for manual assembly and sourcing.

## High-level architecture

```text
                     ┌──────────────────────────────┐
                     │ STM32F103C8T6 / Blue Pill   │
                     │                              │
                     │ PWM excitation               │
                     │ ADC1 + ADC2 / DMA            │
                     │ range + relay control        │
                     │ DSP + calibration            │
                     │ UI + storage + diagnostics   │
                     └──────────────┬───────────────┘
                                    │
                                    ▼
PWM_EXC ── 3-stage RC filter ── buffer ── VEXC
                                    │
                                    ▼
                              selected RREF
                                    │
                                    ▼
                                  RET ───── DUT ───── VMID
                                    │
                         ┌──────────┴──────────┐
                         ▼                     ▼
                      RET_1X               RET_HG
                         │                     │
                         └──────── ADC/DMA ────┘
```

During measurement:

```text
Vs = VEXC - VMID
Vx = RET  - VMID
I  = (Vs - Vx) / ZREF
Zx = ZREF * Vx / (Vs - Vx)
```

The final implementation works with complex phasors and calibrated gain/phase. Nominal resistor, op-amp, switch, filter, and ADC values are not treated as ideal.

## Current hardware baseline

| Block | Current implementation |
|---|---|
| MCU | STM32F103C8T6 Blue Pill module |
| ADC | Internal ADC1 + ADC2, 12-bit |
| AFE | 2 × MCP6002-E/SN |
| Return channels | `RET_1X` + `RET_HG` |
| Nominal HG gain | `1 + 68k/4.7k ≈ 15.47×` |
| RREF bank | 10 Ω / 100 Ω / 1 kΩ / 10 kΩ / 100 kΩ / 1 MΩ |
| Range selection | 74HC238 + ULN2003 + BC807 + back-to-back MOSFETs |
| Low-Z MOSFETs | AO3400A |
| High-Z MOSFETs | Individual 2N7002 in SOT-23 |
| SAFE/MEASURE relay | Hongfa HFD27/005-S |
| K2 low-Z relay | Contingency footprint, DNP in the baseline build |
| Display | ILI9341 SPI TFT |
| External Flash | W25Q64JVSSIQ baseline; firmware should support compatible W25Q densities |
| DUT connection | TEST_HI / TEST_LO, two-wire |
| Power | +5V_SYS from an external 1S Li-ion charge/boost module |
| Temperature | MF58-104J3950GB NTC near the reference bank |
| Controls | Three buttons: UP / OK / DOWN |
| Backlight | PB0 / PWM |
| Buzzer | PB1, external passive piezo through BC817 |
| Charger detect | PA15 / `CHG_VBUS` |

## SAFE / MEASURE architecture

WTK.RLCMeter Rev.1 is intended for **de-energized passive components**. It is not CAT-rated and must not be connected directly to mains or an energized high-voltage circuit.

K1 is fail-safe:

```text
K1 de-energized: TEST_HI/TEST_LO -> SAFE network
K1 energized:    TEST_HI -> RET, TEST_LO -> VMID
```

Before entering MEASURE, firmware verifies the residual-voltage sensing network and other safety gates. `CHG_VBUS` provides both a hardware interlock and firmware-visible charger state. Reset, brownout, watchdog faults, invalid ranges, or safety failures must return the instrument to SAFE.

See [`docs/03-Safety-and-Protection.md`](docs/03-Safety-and-Protection.md).

## Display and external Flash

ILI9341 and W25Q share the SPI bus with independent chip selects. A full 240×320 RGB565 framebuffer requires 153.6 kB, which is larger than the available RAM on the STM32F103C8T6. The UI therefore uses incremental rendering and streaming.

Large assets such as splash screens, icons, and fonts are stored in W25Q Flash and streamed to the TFT through small RAM buffers. A full image never needs to reside in MCU RAM.

## Firmware baseline

The firmware baseline is now fixed as:

- **C17**;
- **CMake** as the canonical build system;
- GNU Arm Embedded / `arm-none-eabi-gcc`;
- CMSIS + STM32CubeF1 HAL/LL;
- no Arduino framework and no `.ino` sources;
- no RTOS initially;
- timer-triggered ADC + DMA;
- short ISRs and DSP outside interrupt context;
- no dynamic allocation in the critical acquisition path;
- host-side tests for pure DSP, calibration, state machines, record formats, and asset tooling;
- reproducible command-line builds independent of STM32CubeIDE.

**Visual Studio Code is the primary supported editor workflow**, using the repository workspace and CMake Tools. The build remains editor-independent.

```text
Firmware/
├── README.md
├── assets/
├── config/
├── src/
│   ├── app/
│   ├── bsp/
│   ├── drivers/
│   ├── hardware/
│   ├── measurement/
│   ├── storage/
│   └── ui/
├── tests/
├── third_party/
└── tools/
```

See [`Firmware/README.md`](Firmware/README.md), [`docs/04-Firmware-Architecture.md`](docs/04-Firmware-Architecture.md), and [`docs/13-Detailed-Firmware-Design.md`](docs/13-Detailed-Firmware-Design.md).

## AI-assisted implementation

The repository contains an explicit execution program for coding agents. Agents must follow [`AGENTS.md`](AGENTS.md) and execute work through the phase plans under [`plans/`](plans/).

The plans define prerequisites, permitted scope, expected files, implementation sequence, tests, hardware dependencies, completion criteria, and handoff requirements. The intent is to let multiple AI-assisted implementation passes progress without silently changing architecture or safety assumptions.

## Repository layout

```text
WTK.RLCMeter/
├── README.md
├── LICENSE.md
├── CONTRIBUTING.md
├── AGENTS.md
├── WTK.RLCMeter.code-workspace
├── .vscode/
├── PCB/
├── Firmware/
├── docs/
└── plans/
```

- [`PCB/README.md`](PCB/README.md) — PCB source, fabrication, render, and revision conventions.
- [`Firmware/README.md`](Firmware/README.md) — firmware architecture and development workflow.
- [`docs/README.md`](docs/README.md) — technical documentation index.
- [`plans/README.md`](plans/README.md) — AI-agent execution-plan index.

## Known Rev.1 limitations

- Final accuracy is not yet qualified.
- The 1 MΩ range and small capacitances are especially sensitive to leakage and parasitic capacitance.
- MCP6002 gain/phase response, particularly `RET_HG` at 10 kHz, requires complex calibration and bench validation.
- The SAFE network detects residual voltage; it is not a general-purpose high-voltage measurement front-end.
- The instrument is two-wire; Kelvin/4-wire operation is a future revision possibility.
- Native USB device operation is unavailable with the current PA11/PA12 allocation.

## Future work explicitly outside Rev.1

Potential future revisions include Kelvin/4-wire measurement, native USB, improved high-Z guarding, and a separate high-voltage measurement front-end. Direct measurement around 400 Vrms AC or 600–800 VDC requires a separate architecture, connectors, clearances, protection analysis, and validation; it is not an extension of the current RLC input.

See [`docs/14-Future-Extensions.md`](docs/14-Future-Extensions.md).

## License

WTK.RLCMeter is released under the **PolyForm Noncommercial License 1.0.0**, following the licensing model used by other WTK.* projects.

Personal use, study, research, evaluation, hobby use, and other noncommercial purposes are permitted under the license terms. Commercial use, resale, or integration into a paid product or service requires a separate commercial license from the copyright holder.

**Required Notice:** Copyright 2026 Rodrigo Wantuk.

See [`LICENSE.md`](LICENSE.md) for the complete terms.

## Boot splash and resource provisioning — October 25, 2026

Every embedded firmware profile now streams the owner-supplied personalized image
from W25Q and holds the completed image for three seconds. The updated seven-entry
pack was installed and read back completely on the first board; calibration/settings
storage is unchanged. All nine embedded builds and44 host tests pass;490 Python
tests ran with391 passing and99 skipped. The board runs updated standard BRINGUP
in SAFE. Visible TFT confirmation is deferred by the owner; metrology calibration
and remaining electrical qualification are still separate release prerequisites.
See [delivery evidence](docs/review/bringup-2026-10-25/Boot-Splash.md) and
[the supported upload procedure](Firmware/assets/README.md).

The [nominal 0.47-uH follow-up](Firmware/releases/2026-10-25/inductor-047-limit-check/README.md) was resistance-like at 1 kHz
and did not publish L. Automatic frequency search and sub-microhenry
capability remain unvalidated; this is not a proven hardware lower limit.

[Bounded low-impedance search](Firmware/releases/2026-10-25/low-impedance-search/README.md)
is now deployed: usable resistive 1-kHz results on RREF 10/100 ohm can request
one supporting 10-kHz check after existing range/quality decisions. All 46/47
host tests and nine strict unit-bound builds pass; calibration is preserved.
Four actual attempts now include 10 kHz; the nominal 0.47-uH result remains
inconclusive, with no published L. K1 still returns SAFE per attempt;
session-wide relay ownership is a separate safety architecture change.

[Session/evidence/menu improvements](plans/24-Measurement-Evidence-Session-and-UI.md)
add reliable classification evidence admission, bounded continuous K1 ownership
and exact row updates in PC-provisioned PRODUCT. Fixed/legacy capture paths and
engineering calibration are preserved. Unattended validation records software
and passive boot separately from pending physical relay/display qualification.
