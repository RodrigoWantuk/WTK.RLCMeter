# Contributing to WTK.RLCMeter

WTK.RLCMeter is a measurement-instrument project. Hardware, firmware, and documentation changes must preserve traceability between design intent, implementation, and bench evidence.

## Core principles

- Safety and fail-safe behavior take priority over implementation convenience.
- Accuracy is measured and calibrated; it must not be inferred from datasheets or simulation alone.
- Changes to the analog front-end, ranges, protection, or pinout must be reflected in the technical documentation.
- DSP code must remain decoupled from the UI, external Flash, and GPIO details.
- External dependencies must use compatible licenses and remain isolated under `Firmware/third_party`.
- Repository documentation, source comments intended as documentation, commit-facing plans, and agent instructions must be written in **English**.

## Firmware baseline

The current firmware architecture is fixed to:

- C17;
- CMake as the canonical build system;
- GNU Arm Embedded (`arm-none-eabi-gcc`);
- CMSIS + STM32CubeF1 HAL/LL;
- no Arduino framework and no `.ino` sources;
- no RTOS initially;
- no dynamic allocation in the critical acquisition path;
- short ISRs;
- deterministic ADC/DMA/timer acquisition;
- host-side tests for pure logic where practical;
- Visual Studio Code as the primary supported editor, while keeping builds editor-independent.

Do not introduce C++, Arduino abstractions, a different build system, or an RTOS without an explicit architecture decision recorded in the documentation.

## Hardware changes

A relevant PCB change should include, as applicable:

1. schematic update;
2. PCB synchronization;
3. DRC without shorts, open nets, or critical violations;
4. Gerber/BOM exports matching the same hardware revision;
5. updates to `docs/01-Hardware-Architecture.md` and/or `docs/05-Pinout-and-Interfaces.md`;
6. updated DNP population notes;
7. a revision note when electrical or mechanical behavior changes.

Changes involving `SAFE_HI`, `SAFE_LO`, K1, residual-voltage sensing, bleeders, clamps, charger interlock, or clearances must be treated as safety-sensitive changes.

## Firmware changes

Firmware code should preserve these invariants:

- K1 returns to SAFE on fault/reset;
- `RANGE_EN` remains disabled while the range address changes;
- UI code never drives K1/K2/range GPIOs directly;
- `measurement` remains independent of TFT/W25Q/GPIO implementation details;
- heavy DSP, storage operations, and rendering never run inside ISRs;
- hardware-dependent assumptions are documented and measurable;
- persistent formats are versioned and protected by CRC.

## Tests

Pure mathematics and pure state machines should have host-side tests under `Firmware/tests` whenever practical.

Priority areas include:

- complex arithmetic and I/Q extraction;
- impedance calculation;
- high-gain channel reconstruction;
- autorange policy;
- calibration correction and record parsing;
- SAFE/MEASURE policy;
- asset-pack parsing;
- state-machine transitions and fault handling.

Hardware-dependent work must document which checks can run automatically and which require bench validation.

## AI-assisted changes

AI coding agents must read [`AGENTS.md`](AGENTS.md) before making changes and then follow the relevant plan under [`plans/`](plans/).

An agent should not silently expand its scope into later phases. If a prerequisite is missing, the agent should either implement that prerequisite when explicitly permitted by the current plan or stop with a precise handoff note.

## Documentation

[`docs/README.md`](docs/README.md) is the index for the current normative technical documentation.

Use explicit language to distinguish:

- existing hardware;
- implemented firmware;
- planned firmware;
- bench-validated behavior;
- `NOMINAL` measurement regions;
- `EXTENDED` regions;
- experimental/DNP hardware options;
- future-revision concepts.

Do not present engineering targets as guaranteed specifications before qualification.

## License

Contributions to this repository are provided under the same terms as the project, described in [`LICENSE.md`](LICENSE.md).

The project uses the PolyForm Noncommercial License 1.0.0. Commercial use requires a separate license from the copyright holder.
