# A05 — Rev.1 pre-assembly audit

**Disposition: assembly approval is withheld.** Six RED items require resolution
before the affected component is soldered or the corresponding interface is
powered. Several are documentation or module-selection errors, rather than proven
defects in the manufactured copper. No physical board was inspected or powered.
After bare-board inspection, low passive components may be assembled using the
population matrix; this is not permission for first power-on. A05's assembler
sign-off and electrical/metrology qualification remain open.

Starting main: `47bd1f264899ea10e948494e5a3e546e5b51558c` (PR #12 merged).
Branch: `codex/a05-rev1-hardware-audit`. Firmware and PCB source are unchanged.
Only audit tooling, evidence tables, connector documentation and plan status change.
No new STM32 build or Flash/RAM change is attributed to this documentation task.

## Evidence and reproducibility

The requested `Rev1 0` directory is absent. These actual files were inspected:

- [EasyEDA source](../../../PCB/source/ProPrj_WTK%20RLC%20Meter_2026-08-19.epro2):
  ZIP containing `WTK RLC Meter.epru`, a readable JSON record stream. All 219
  documents are decoded, including five schematic sheets, PCB, device, symbol and
  footprint libraries. Null/deleted records are excluded and updated IDs resolved.
- [Schematic PDF](../../../PCB/fabrication/Rev1/SCH_WTK-RLC-Meter_2026-08-19.pdf):
  five sheets, visually reviewed for power, analog, range, relay and MCU wiring.
- [PCB PDF](../../../PCB/fabrication/Rev1/PCB_RLC_2026-08-19.pdf): ten pages
  rendered; top/bottom copper, assembly and mirrored bottom views inspected.
- [Fabrication BOM](../../../PCB/fabrication/Rev1/BOM_RLC-PCB_2026-08-23.csv):
  UTF-16 tab-separated, 51 groups / 149 references. Every value, manufacturer part,
  footprint title and quantity agrees with the live source; no missing/duplicate ref.
- [Gerber ZIP](../../../PCB/fabrication/Rev1/Gerber_RLC-1_2026-08-23.zip): both
  copper layers, outline, masks, paste, silkscreen, drill outputs and
  `FlyingProbeTesting.json`. The outline is 100 × 100 mm. All 434 actual component
  pads have matching top copper flashes and all 84 component hole centers/diameters
  match the plated drill file. The probe export also contains 422
  synthetic `PADn` objects; these are not extra BOM components.

```powershell
# Repository root; Python 3.9+ standard library only. No CAD or PDF dependency.
python PCB/tools/audit_rev1.py --check
python -m unittest discover -s PCB/tests -v
```

Local validation: **17 tests passed / 0 failures**; source/BOM/net partition,
434 copper pad flashes, 84 component drills, firmware mapping, all three CSVs
and 31 local documentation links pass. `git diff --check` passes. The existing
Flash-forensics workflow also runs these checks. No firmware source/build inputs
or PCB design/manufacturing files change, so no firmware size delta is claimed
and the ARM/CTest matrix was not rerun for A05.

The extractor reports all five input SHA-256 hashes, checks firmware GPIO and
peripheral assignments, and reconciles committed CSVs without modifying files.
Evidence such as `epro2!/WTK RLC Meter.epru:14594` means the uncompressed stream's
one-based line. Probe evidence names the actual reference/pad in the ZIP member.
These input identities are fixed for this review:

| Input | SHA-256 |
| --- | --- |
| epro2 | `53ed93c4c03b9208c3adfed344bbcabe2e50dd553b61140802b91a6913f8228a` |
| BOM | `066b46561a9aa232ce19a41912089794f29b1bf21e0fe1a3a22ff1ab94f33889` |
| Gerber ZIP | `fbb1ec32a89709459de773330363079aecc23cdb01bb221d8439c2de45279aaa` |
| Schematic PDF | `1bfa89cc0bd4c855036ca824a7bc152d1c10088aaae84dc51bd6da2144ef2a1a` |
| PCB PDF | `ddbc524c8a994b4aabe458d14edab9b4707eeb39be765479f6fda5e0cd344280` |

All 115 connected source nets preserve exactly the same endpoint partitions in the
fabrication export. Eight labels change: decoder Y0..Y5 become NET_14..NET_9 and
K1/K2 coil-low become NET_1/NET_2. Twelve unconnected pads remain separate isolated
groups. **RSCK's two pads move +5 mil Y (0.127 mm) in fabrication**; this exception is
explicitly checked. Therefore the August 19 source is not geometrically identical
to the later export. No component-net split/merge was found.

`SCHEMATIC_VERIFIED` means decoded live library/PCB connectivity corroborated by
the relevant schematic sheet. `FABRICATION_VERIFIED` here means fabrication-export
endpoints, pad flashes and reviewed coordinates/drills, **not as-built continuity**.
`NEEDS_OWNER_INSPECTION` requires physical parts/board evidence.
`REQUIRES_BENCH_VALIDATION` requires controlled assembled-board measurements.
No native CAD DRC, arbitrary copper-region connectivity solver, clearance
certification, manufactured-board test or signed assembly approval was performed.

## Findings and assembly decisions

[Findings](findings.csv): **6 RED / 11 YELLOW / 7 GREEN / 3 UNKNOWN**.
[Population](assembly-population.csv): **126 POPULATE / 7 DNP / 16 HOLD FOR REVIEW**,
one row per actual component. POPULATE retains the reviewed topology, subject to
actual package/orientation/rating inspection; it is not unconditional sign-off.
[Pin matrix](pin-net-matrix.csv): 51 rows with physical pads and evidence, including
all 33 requested MCU pins. **30/33 are carrier-connected and reconciled**; PA13,
PA14 and PB2 are module-only and require physical inspection. The 40 carrier pads
also include PC14/15, RESET, power, ground and backup VBAT.

| RED | Required resolution |
| --- | --- |
| R01 TFT | Actual connector is **1×9 / 2.54 mm**, not 2×5. Confirm the module, cable order and supply. |
| R02 LED | R_TFT_LED is 0 Ω directly between PB0 and J_TFT.8. Leave open until a logic-enable input is proven or a rated driver is designed. |
| R03 UART | Actual J_UART is **1 GND, 2 MCU TX, 3 MCU RX**. Documentation corrected; inspect the cable. |
| R04 buzzer | BOM's PB-12N23MPW-12Q is a **9–15 V magnetic indicator with internal circuitry**, incompatible with the 5 V/passive-tone design. HOLD BUZZER1. |
| R05 module | CAD specifies LCKFB-DKX-STM32F103C8T6. An MCU type alone does not identify the module pinout/regulator/USB/HSE. HOLD fitting. |
| R06 power | Identify charger/protection/boost/cell and verify their wiring/limits before power or battery connection. |

R01/R03's historical connector errors are corrected in
[docs/05](../../05-Pinout-and-Interfaces.md); no carrier net or firmware pin changed.
The owner supplied “STM32F103C8T6, ILI9341, default charger and boost, single 18650
not yet defined.” Module photographs/part numbers and the actual fabrication order
remain unavailable; no default power circuit is inferred from that answer.

Confirmed **topology** recommendations: K2 DNP; R0_BANK populated. The unused K2
driver chain QK2/RK2B/RK2PD/DK2 can also be DNP (seven total DNP refs including the
TVS pair). D_TVS and R_TVS_LINK both DNP. K2's NO contact and the hard link connect
the same LOWZ_BUS/RET nets; do not approve both paths or fit relay-only with the
current fixed-link firmware. No live active-guard component/footprint/net was
identified. **R_BYP_VM and R_BYP_VEXC must be populated**: they are U4 closed-loop
buffer connections, not optional guard circuitry. No other alternative protection
path was found; DNP the SMAJ120CA branch does not grant external-voltage capability.

## Reconstructed electrical paths

| Decoder address | Reference | Switch pair | Gate driver | Output |
| --- | --- | --- | --- | --- |
| 000 | RREF1 10 Ω | QSEL1/2 AO3400A | QG1 | LOWZ_BUS → R0_BANK → RET |
| 001 | RREF2 100 Ω | QSEL3/4 AO3400A | QG2 | LOWZ_BUS → R0_BANK → RET |
| 010 | RREF3 1 kΩ | QSEL5/6 AO3400A | QG3 | LOWZ_BUS → R0_BANK → RET |
| 011 | RREF4 10 kΩ | QSEL7A/B 2N7002 | QG4 | RET |
| 100 | RREF5 100 kΩ | QSEL8A/B 2N7002 | QG5 | RET |
| 101 | RREF6 1 MΩ | QSEL9A/B 2N7002 | QG6 | RET |

For each path: VEXC → RREF → first MOSFET drain → common sources → second drain →
bank/RET. Both gates share the selected driver; common-source opposing body diodes
block both directions when off within the devices' ratings. RGS=10 kΩ is
gate-to-common-source, not gate-to-GND. 74HC238 U2.4/.5 are grounded active-low
enables; U2.6 is active-high RANGE_EN with REN 10 kΩ to GND. Its active-high Y0..Y5
feed U3.1..6; the ULN2003 sinks the corresponding BC807 base through 4.7 kΩ.
BC807 emitter is +5V_A, collector drives the gate through 100 Ω; 47 kΩ base-emitter
pull-up turns it off when not selected. This double inversion agrees with firmware.
U3.9 COM is unused because these outputs drive bases, not relay coils; unused U3
input/output and U2 Y6/Y7 are not additional active ranges.

`hw_range_request()` disables enable before writing address, then waits the existing
dead/settling times before READY. Logical correctness does not prove physical
break-before-make: capacitive gate decay, decoder/ULN thresholds, supply collapse
and unpowered HC outputs require traces. Enabled VGS is about 5V_A minus the actual
source common-mode and transistor drop, not “5 V drive.” HOLD the twelve switches
until the **actual YONGYUTAI/CBI parts** are checked. Familiar AOS/Nexperia numeric
RON/leakage guarantees cannot be transferred to those lots.

VMID: +3V3 → two 10 kΩ divider → VMID_RAW, 10 µF/100 nF filtering → U4.3;
U4.1 → R_BYP_VM → VMID with feedback at U4.2. VEXC: PA8 PWM → three loaded
5.1 kΩ/1 nF RC sections → FILT3/U4.5; U4.7 → R_BYP_VEXC → VEXC, feedback U4.6.
For ideal equal parts and an unloaded final filter node,
`H=1/(1+6s+5s²+s³), s=jωRC`; predicted magnitude/phase at 100 Hz, 1 kHz, 10 kHz are
0.9999/−1.10°, 0.9869/−10.94°, 0.5125/−75.56°. These are circuit calculations,
not measured amplitude/phase or three independent buffered poles. Loaded U4 drive,
PWM quantization/ripple and settling remain unqualified.

RET → U5.3 unity follower U5.1/2 → RET_1X. RET → U5.5, with 68 kΩ from U5.7 to
U5.6 and 4.7 kΩ from U5.6 to VMID → RET_HG. Nominal gain is 15.468; the typical
1 MHz op-amp GBW implies frequency-dependent HG behavior. At ideal 1.65 V center,
the **ADC** headroom alone corresponds to only about 75 mVrms RET difference on HG;
500 mVrms OPEN-like returns necessarily exceed that ideal HG range. Clipping-aware
1X selection is essential. No nominal gain substitutes for effective HG calibration.

AFE outputs → 1 kΩ → ADC pins with 1 nF/GND and BAT54S rails: pad1 GND, pad2 +3V3,
pad3 signal. U4/U5 are powered +5V_A; an op-amp driven toward 5 V can feed the
3V3 rail through the Schottky clamp (order-mA per channel through 1 kΩ). In partial
power this can phantom-power the module or raise an LDO rail that cannot sink.
Do not start the AFE alone without a valid 3V3 rail. BAT/NTC do not have these six
clamp footprints; their normal divider voltages must remain within the ADC rails.

K1's de-energized poles: TEST_HI (13) → SAFE_HI (11), TEST_LO (4) → SAFE_LO (6).
Energized poles: 13 → RET (9), 4 → VMID (8). Coil 16 is +5V_SYS, 1 is low-side;
QK1 is B1/E2/C3, DK1 cathode pad1 is +5V_SYS. RK1PD=100 kΩ pulls the base down;
positive CHG_VBUS through RUSB_B drives QUSB_INH to shunt K1_BASE to GND.
SAFE_HI/LO are joined by two series 47 kΩ bleeders, not tied directly to GND.
Each SAFE node reaches its SENSE node through 3×560 kΩ, with 27 kΩ/10 nF to VMID,
then 4.7 kΩ to its clamped residual ADC. Nominal residual ratio relative to VMID is
27k/(1680k+27k); this is sensing of de-energized fixtures, not HV protection approval.
No contact-position feedback exists; software SAFE is commanded state until bench
continuity confirms it.

## Packages, power and physical limits

BC807/817 CAD/library and Diodes part pinout agree B1/E2/C3; MOSFET library is
G1/S2/D3. BAT54S is a **series** dual diode: BAT54A/C is not an equivalent clamp.
MCP6002 and the two SOIC-16 parts have matching pad functions. Relay dimensions,
coil/NC/NO were checked against Hongfa's **bottom-view** drawing; do not mirror that
drawing when placing the top-side part. UFLASH uses the wide 208-mil SS package
footprint, not a narrow 150-mil SOIC8 replacement. COUT1 is polarized, pad1 +5V_A,
pad2 GND; EEUFR1A221 denotes the specified 220 µF/10 V part. Inspect actual bags,
pin1 dots/notches, electrolyte stripe, solder-mask lands and clearance before solder.

J_PWR.1 is protected cell voltage, .2 regulated +5V_SYS, .3 GND, .4 charger VBUS.
RA=4.7 Ω feeds +5V_A with 220 µF + 10 µF + 1 µF + 100 nF filtering. The module
supplies +3V3 to Flash, decoder, divider and clamp rails. USTM32.21 is the module's
**backup VBAT pin tied to +3V3**, not J_PWR's 18650 node. ADC_BAT uses 100k/100k,
100 nF; ADC_NTC uses 100k pull-up/100k B3950 NTC, 100 nF. Both have about 5 ms
nominal Thevenin RC at equal divider resistance. CHG_VBUS 10k/12k division predicts
PA15=2.727 V at a 5 V charger input; actual logic margin and hardware inhibit require
verification. Debug USB attachment does not itself prove CHG_VBUS is asserted.

| Identified load | Defensible input to budget | Still missing |
| --- | --- | --- |
| K1 HFD27/005-S | 125 Ω ±10% at 23°C; nominal 40 mA/200 mW at 5 V | Actual coil current, pickup/release under supply sag |
| U4/U5 | Four amplifiers, 100 µA typical each unloaded | Output/load current, saturation and supply noise |
| VMID divider | 3.3/20k = 0.165 mA nominal | Rail scale and buffered-load current |
| Gate network | Selected RGS draws VGS/10k; base/ULN currents also consume power | Actual VGS/ULN sink/transient peaks |
| BUZZER1 BOM | 50 mA at 12 V, outside board operating range | Replacement type; not included as a valid 5 V load |
| Module/TFT/Flash/boost/charger/cell | Exact parts and active states required | Total current, thermal margin, efficiency/runtime and power-off backfeed |

A complete power budget cannot be closed with unspecified modules. Current limits
in the checklist are cautious test settings, not predicted consumption. MCU POR
and IWDG reset reporting do not validate converter UVLO or analog rail sequencing.
USB-UART TX/SWD can backfeed an unpowered target; supply it from one controlled
source and use debugger VTref only as sense. Never connect module native USB on
PA11/12 in this board configuration. Digital/relay/display return currents and
bottom-pour necks need physical inspection; no clearance or noise guarantee follows
from visually intact tracks.

The source placement puts CU1 (100 nF) about 12.3 mm from U4 pin8 and 11.2 mm from
U5 pin8 (straight-line distance to capacitor center; not measured copper path).
CU2's distances are about 11.7/15.8 mm respectively. Thus the schematic's shared
AFE bypass parts are not demonstrably close local IC bypasses. Inspect the actual
supply/return loop, record ringing/noise and consider explicitly documented local
100 nF supply-to-GND rework if required; this is Y10, not a proven oscillation defect.

No unequivocal unrepairable fabrication defect was demonstrated. The unresolved
TFT driver may need external circuitry/cut-and-wire work; a different module may
need an adapter. Population choices cannot fix a proven routed short or missing
track: such a discovery requires explicit PCB rework/replacement and continuity
verification, not a DNP workaround.

## Primary component evidence

- [ST STM32F103C8 datasheet](https://www.st.com/resource/en/datasheet/stm32f103c8.pdf), GPIO/ADC limits, supply and package pinout.
- [Nexperia 74HC238](https://assets.nexperia.com/documents/data-sheet/74HC_HCT238.pdf), enable truth table and pad assignment.
- [TI ULN2003A](https://www.ti.com/lit/gpn/uln2003a), input/output/COM and sink characteristics.
- [Diodes BC807](https://www.diodes.com/datasheet/download/BC807-25.pdf), [BC817](https://www.diodes.com/datasheet/download/BC817-25.pdf), [BAT54S](https://www.diodes.com/datasheet/download/BAT54S.pdf).
- [Microchip MCP6002](https://www.microchip.com/en-us/product/MCP6002), supply, GBW and unloaded current; typical data is not qualification.
- [Hongfa HFD27](https://source.hongfa.com/Uploads/Product/PDF/HFD27_en.pdf), sensitive 005-S coil and bottom-view contact drawing.
- [Mallory indicators catalog](https://mspindy.com/wp-content/uploads/2020/12/LIT-Indicators-Electromagnetic.pdf), p2 exact buzzer voltage/type/pitch.
- [Winbond-authored W25Q64JV datasheet, distributor mirror](https://media.digikey.com/pdf/Data%20Sheets/Winbond%20PDFs/W25Q64JV_RevK_3-10-21.pdf), SS package and SPI pins. Official Winbond download access failed; the source/BOM match is independently verified, actual part inspection remains required.

Proceed using the [bring-up checklist](bringup-checklist.md) and
[metrology gates](metrology-readiness.md). The smallest next step is an unpowered
physical module/header/cable inspection with the board and this matrix, followed by
identified current-limited power wiring. BRINGUP_CAL is not electrically released.
