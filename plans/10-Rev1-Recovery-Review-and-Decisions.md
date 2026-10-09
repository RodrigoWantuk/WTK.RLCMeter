# Rev.1 Recovery Program — Technical Review and Decision Record

STATUS: IN_PROGRESS — PLANNING / REQUIRES_OWNER_DECISIONS
BASELINE: main @ 8abc1b3dd65cbd5cec7ee984b6165c63b0d50c64, 2026-10-09
OWNER: product/hardware owner; execution by assigned coding and bench agents
PURPOSE: recover a usable instrument, not maximize the number of displayed parameters.

## Reading order and authority
Read AGENTS.md, docs/01, 02, 03, 05, 07, 10, 13, 15, 16 and plans/09B before implementing these plans. Existing safety contracts and the actual Rev.1 manufactured PCB take precedence over hypotheses in this program. Documentation is derived from the repo, BOM, firmware and prior decisions. The electrical schematic PDF and EasyEDA source must be independently reconciled pad-by-pad by the hardware agent; this initial review has NOT electrically verified every individual PCB net against the binary schematic.

## Baseline: confirmed in source
- STM32F103C8T6/Blue Pill: guaranteed 65,536 B internal Flash, 20,480 B SRAM. No RTC/RTOS/native USB; SWD PA13/14; USART1 PA9/10; SPI2 W25Q64 + ILI9341.
- PRODUCT and BRINGUP CMake profiles already exist; legacy Lab removed. PRODUCT Debug/Release use size-first compilation and LTO in presets. Do not duplicate these changes.
- Existing PRODUCT path includes safety, click measurement, multi-attempt autorange, OSL wizard, W25Q A/B calibration, external resource-pack recovery, UI, and post-OSL runtime curve reader. Existing BRINGUP adds large console and experimental DC pilot; do not assume it can fit additional functionality.
- 33 software-supported OSL condition keys: 6 RREF × 3 frequencies (100 Hz, 1 kHz, 10 kHz) × 2 nominal amplitudes (100/500 mVrms), less all three forbidden 10-ohm/500-mVrms combinations. Supported by code does NOT mean physically qualified.
- OSL schema v2 / model v4 complex Möbius correction and effective HG path calibration; overlay WCRV v1 with 3 log|Z|/RREF knots and a 2×2 real matrix at each knot. Curve candidate fitting and validation exist on PC, but normal transfer, physical qualification and installation do not.
- No real-board accuracy, leakage, voltage safety, thermal-drift or source-current evidence is documented. This is not a product performance specification.

## Baseline numbers (last documented build; not reproduced by this planning task)
- PRODUCT Release 64,096 B Flash, 17,092 B accounted RAM; 1,440 B remaining before silicon Flash limit. PRODUCT project gate 63 KiB = 64,512 B.
- BRINGUP 65,052 B Flash, 16,636 B accounted RAM; only 484 B physical Flash headroom.
- Host 40/40 CTest in each profile and 123 Python tests were reported by plans/09B; those are historical reports, not freshly run test evidence.
- Size tool Firmware/tools/firmware_size.py includes .text + .data for Flash, .data + .bss + .noinit + reserved stack/heap for RAM. It already exports top symbols and sections.

## Design positions submitted for owner approval
D01. **Operating product**: retain mandatory interlocks, relay/range ownership, measurement, automatic component interpretation, essential bilingual TFT/UI, local OSL minimum, valid W25Q recovery, settings and measured-value provenance. All other code is conditional on release value.
D02. **Factory and lab**: move advanced calibration campaign management, extensive diagnostics, RAW export, curve fitting, plots, uncertainty propagation and scope measurement history to PC. A production instrument shall not require PC for ordinary measurement.
D03. **Bringup**: split into small mutually exclusive images if necessary; external W25Q is data/parameter storage, not transparent execution Flash. Never assume 128 KiB in a nominal C8.
D04. **Metrology**: complex AC impedance is the authoritative primary measurement. R, X, |Z|, phase, conditional equivalent C/L, series AC ESR, Q, D follow from physically qualified regions. Two-wire DCR, low-voltage leakage and time-domain transients are separate gated research tracks, not enabled PRODUCT features by assumption.
D05. **Calibration**: retain OSL as the first layer. Treat supplementary curves as sparse, optional, parsimonious post-OSL corrections; only install where independent standards and holdouts demonstrate improvement. No arbitrary 12-coefficient fit per condition without sufficient identifiability.
D06. **Traceability**: purchased tolerance is an interval, not proof of the actual part's value. Regressions must hold on separate validation standards, repeated removals and reattachments, temperatures and two-way analysis.
D07. **Uncertainty**: show a validated bounded specification only where supported by evidence. Otherwise display 'unqualified / uncertainty not established'; never generate a made-up ±30% or interpret a k≈2 expanded uncertainty as a guaranteed absolute error bound.
D08. **Availability**: start with commonplace Brazilian metal-film resistors, small C0G/NP0 or film capacitors, ordinary inductors for cross-checking, a shorting fixture, a DMM and a scope. Expensive 0.01%, ESR, nanohm, Kelvin or calibration-lab artifacts are options, not prerequisites for initial operation.
D09. **Safety**: no live DUTs, mains, HV, insulation tester, external injected AWG voltage into DUT inputs or oscilloscope ground clips attached to VMID/RET/TEST terminals. K1, ADC and charger protections remain enforced.

## Evidence ledger required from agents
Each claim carries one of IMPLEMENTED_TESTED_HOST, COMPILES_TARGET, REQUIRES_BENCH_VALIDATION, BENCH_QUALIFIED(condition,fixture,date,rawhash,instrument), DISABLED or UNSUPPORTED. Never equate syntax, synthetic simulation, and hardware accuracy. Bench JSON includes hardware revision, BOM substitutions, photograph/fixture ID, raw capture SHA256, exact firmware SHA, OSL sequence+CRC, overlay sequence+CRC, conditions, board NTC and ambient thermometer, supply, instruments, reference tolerance/calibration state, timestamp, pass/fail and operator notes.

## Current strategic blockers
B01 physical Rev.1 electrical netlist and assembly substitutions not fully reconciled.
B02 no actual ELF/map size attribution or before/after bytes for optimization opportunities.
B03 no scope or DMM validation of PWM, filter, high-gain channel, reference switches, ADC timing.
B04 1 MOhm DC pilot is BRINGUP-only and neither DCR nor leakage qualified.
B05 12-coefficient supplementary complex fit risks underdetermination and overfitting.
B06 PRODUCT does not include independently justified per-reading measurement error bounds or a complete curve-installation qualification ceremony.
B07 the full 33-condition on-device calibration workflow may be too burdensome for routine users; distinguish factory exhaustive versus field verification without sacrificing validity.

## Program index / ownership
- 10 this technical review and decision register (architect + owner).
- 11 Flash reduction and target-profile refactoring (embedded/platform agents; highest immediate priority).
- 12 Hardware metrology map, DC feasibility and bench evidence (hardware/metrology agents).
- 13 Calibration curves, uncertainty and validation mathematics (metrology + PC agents; owner approval before changing on-device model).
- 14 Accessible calibration kit, oscilloscope procedures and PC workbench UX (PC + bench agents).
- 15 Ordered milestones, tests, release gates, dependencies and prompts for workers (integrator).

## Owner answers needed (do not block evidence-only tasks)
Q1 Exact equipment: scope make/model, channels, probes (1×/10× or differential), approximate vertical accuracy, whether it has an AWG, DMM make/model, temperature reference.
Q2 Board assembly: does first Rev.1 PCB exist and power up? Actual K2 / R0_BANK / TVS / active-guard DNP population, charger/boost, original MOSFET/op-amp substitutions, actual RREF tolerances.
Q3 What is the minimum acceptable PRODUCT at first delivery: AC RLC+ESR/Q/D only, or is DC DCR mandatory at launch?
Q4 Calibration split: must a normal user calibrate without a PC, and is a factory/advanced PC workflow acceptable for supplementary curves and DC?
Q5 Preferred error presentation: per-result numerical bounded accuracy (qualified conditions only) with an explicit NOT CHARACTERIZED for others? A 95%-coverage interval is not the same as maximum error.
Q6 Which known resistors/capacitors/inductors and reference measurement tools are already on the bench? Prioritize reuse before ordering.

## Change-management rule
These are review proposals, not permission to bypass established decisions. Agents may start measurement/logging, size-audit, mock UI and host-only solver tests. Changing the product calibration schema, enabling non-1M DC, publishing numerical accuracy, removing the on-device OSL wizard, or changing hard safety behavior requires an explicit recorded owner decision and relevant bench gates.
