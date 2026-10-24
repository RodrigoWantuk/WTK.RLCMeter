# B02.0 — Rev.1 assembly decisions

**ENGINEERING_REVIEWED / NEEDS_OWNER_INSPECTION / REQUIRES_BENCH_VALIDATION.**
No board has been assembled, powered or approved by this review. Starting SHA
`ad032fc3975624c15ffa1729266b68cfda075141`, PR #20 confirmed merged; clean main
fetched/fast-forwarded, submodules initialized; branch
`codex/b020-manual-bench-readiness`. Only these three bench documents change.
[Bench card](bench-card.md) → [measurement/calibration baseline](measurement-baseline.md).

Evidence: decoded [EasyEDA project](https://github.com/RodrigoWantuk/WTK.RLCMeter/blob/16633dc/PCB/source/ProPrj_WTK%20RLC%20Meter_2026-08-19.epro2),
five-sheet schematic, August 23 BOM/Gerber/FlyingProbe under
[Rev 1 fabrication](../../../PCB/fabrication/Rev1), and the
[A05 per-part matrix](../a05/assembly-population.csv)/[pad matrix](../a05/pin-net-matrix.csv).
Fresh reconciliation: 149 references / 434 pads / 115 connected nets, copper pad
flashes and 84 drills agree; 17 extractor tests pass. This is
SCHEMATIC_VERIFIED / FABRICATION_VERIFIED **export** evidence, not actual copper
continuity, complete native DRC or purchased-part inspection. The 0.127 mm RSCK
placement delta and unknown fabrication order remain A05-Y01/U01/U03.

Status below concerns the first assembly; every POPULATE requires matching actual
marking, package orientation and bare-board continuity. HOLD is local: omitted
display/buzzer do not prevent UART/analog work. Never solder an unidentified
module/transistor just to complete the BOM. Do not alter manufactured copper.

| Designator / actual footprint | Decision; nominal function/part | Specific inspection / permitted alternative | What can proceed without it; blocked stage |
| --- | --- | --- | --- |
| USTM32, 53.3 × 22.9 mm 40-pad carrier, 2.54 mm pitch / 15.24 mm row spacing | **HOLD — R05.** CAD LCKFB-DKX-STM32F103C8T6; owner identifies STM32F103C8T6 only | Dry-fit and map both rows: .18=5 V; .20/.38=3V3; .21=backup VBAT tied to 3V3; .19/.39/.40=GND; PA13/14 SWD are module-only. Inspect regulator marking/caps, 8 MHz HSE, reset, PA11/12 USB pull-up/load and accessible SWD. No module USB. A numbered external/header adapter is acceptable only after complete mapping; no reversed-header experiment | Low components can be soldered first. Blocks carrier power, programming and measurement until module/regulator equivalence established |
| Module regulator, **no separate carrier regulator footprint** | **HOLD identification**, not an automatic AMS1117 replacement | Confirm actual input/output/GND, +5 V rating, output capacitor requirements, reverse-current behavior, thermal rating and VDDA connection. No established ability to sink externally injected current. If unsuitable, a reviewed regulated external 3V3 adapter requires isolation of the module regulator output and a complete power-state map; never parallel two regulators | Passive assembly yes; first power blocked if regulation/power wiring unknown |
| J_PWR HDR1×4; external 1S charger/protection/boost/cell | Populate header; **HOLD external battery assembly — R06** | .1 protected-cell sense, .2 regulated 5 V, .3 GND, .4 charger VBUS. Exact charger/protection/boost/cell models and USB/load-sharing/UVLO/wire polarity remain unknown. First tests use independent current-limited 5 V and the removable sense fixture on the bench card, **no cell**. Later modules must demonstrate protected 1S output and VBUS hardware inhibit; do not assume “default module” compatibility | Does not block passive assembly or bench-powered tests with the reviewed fixture; blocks battery/charging operation |
| RA R1206; CIN1 C1206; COUT1 Ø6.3 mm/2.5 mm THT; COUT2 C1206, COUT3/4 C0805 | **POPULATE** 4.7 Ω; 10 µF input; 220 µF/10 V +10 µF+1 µF+100 nF analog reservoir | Check electrolytic polarity (.1 +5V_A/.2 GND), ceramic values/rating and solder shorts. Keep original rail filter; no wire bridge across RA. Replacement electrolytic must fit pitch/polarity and at least original voltage/temperature/ripple requirements | Blocks AFE power if missing/wrong; do not power AFE alone while MCU rail absent |
| U4/U5 SOIC8 5.0×4.0 mm body, 1.27 mm pitch; CU1/2 C0805 | **POPULATE** MCP6002-E/SN;100 nF/1 µF +5V_A bypass | Both .8 supply/.4 GND, U4 .1 VMID_PRE/.7 VEXC_PRE, U5 .1 RET_1X/.7 RET_HG. SN SOIC, not DIP/MSOP. Keep exact amplifiers first; no unnecessary substitution. Inspect A05-Y10 ground/bypass loop. Local 100 nF pin 8-to-pin 4 wire-up only if measured instability warrants it and access/lead length is reviewed | Other low parts yes; analog work blocked if absent or ambiguous |
| R_BYP_VM/R_BYP_VEXC/R0_BANK R1206 | **POPULATE** all three 0 Ω links | U4.1→VMID feedback at .2; U4.7→VEXC feedback at .6; LOWZ_BUS→RET. These are functional paths, **not optional guard circuitry** | Missing bypass opens feedback; missing bank removes low ranges; blocks respective analog tests |
| RADC1–4 R0805; CADC1–4 C0805; DCL1–4 SOT23 2.9×1.3 mm body | **POPULATE** 1 kΩ/1 nF C0G/BAT54S-7-F; **Y04 remains open** | RADC1/2 .2 amplifier/.1 ADC; RADC3/4 .1 amplifier/.2 ADC; diode .1 GND/.2+3V3/.3 ADC. BAT54S series pair, not A/C/common variants. Retain nominal parts; no premature larger resistor or removed clamp. Confirm actual resistor tolerance and diode orientation/marking. DCLAMP5/6 and residual ROV networks remain required | Low parts yes; first analog/OPEN tests subject to rail/clamp gate; never omit protection to get cleaner data |
| RVMID1/2 R0805; CVMID1 C1206/CVMID2 C0805; RF1–3 R0805/CF1–3 C0805; RF_HG/RG_HG R0805 | **POPULATE** 10 k/10 k,10 µF/100 nF;5.1 k/1 nF×3;68 k/4.7 k | Inspect values, clean flux, preserve C0G reconstruction caps. X7R 10 µF effective capacitance affects VMID settling. No “three independent RC” assumption or replacement gain network | Missing/wrong values block source/return characterization and OSL |
| UFLASH SOP8,5.3 mm body/8.0 mm lead span,1.27 mm pitch; CU3 C0805 | **POPULATE** W25Q64JVSSIQ +100 nF | Pin 1 CS,2 DO,3 WP/IO2,4 GND,5 DI,6 CLK,7 HOLD/IO3,8 +3V3. Verify **SS208-mil**, 2.7–3.6 V JV part; smaller SN150-mil or 1.8 V variant not drop-in. Retain 10 k CS/WP/HOLD pulls; no external erase as an identification test | UART/rail inspection possible without it; stored calibration and standalone PRODUCT blocked |
| K1 HFD27 THT; QK1/QUSB_INH SOT23; DK1 SOD123; RK1B R0805/RK1PD R0805 | **POPULATE after inspection** HFD27/005-S, BC817-25,1N4148W,1 k/100 k. Temporarily **HOLD RK1B** if module code is unknown at first power | Non-latching 2 Form C, not 2H/FormA; coil .1/.16 nominal 125 Ω±10%; NC .13–.11/.4–.6 closed; NO .13–.9/.4–.8 open. Q B1/E2/C3; DK1 cathode .1 +5 V/anode .2 coil-low. Open RK1B disconnects PB9 from K1_BASE while fitted RK1PD holds base low. Restore only after known BRINGUP safe boot, power removed, repeat continuity | Coil isolation permits first power/programming, **blocks all MEASURE** until resistor restored. Do not force coil with external supply |
| U2 SOIC16 9.9×3.9 mm; U3 same footprint; QG1–6 SOT23; REN/RBASE/RBUP/RGATE/RGS R0805 | **POPULATE** 74HC238D, ULN2003ADR, BC807-25; original drive resistors | U2.16+3V3/.8 GND/.4/.5GND/.6PB8; U3.8 GND, .9COM unconnected (resistive gate-driver load). QG B1/E2(+5V_A)/C3. Keep HC at 3V3; HCT supply/input requirements differ. Confirm pull-down/pull-up values and one-hot gates before DUT. See baseline decoder table | No range/measurement until installed and physically checked; no direct GPIO forcing |
| QSEL1–6 and QSEL7A/B,8A/B,9A/B, SOT23 G1/S2/D3 | **HOLD actual lot pinout — Y03**; AO3400A low ranges /2N7002 high ranges | Original BOM vendors YONGYUTAI/CBI lack established purchased-lot data. Verify bag/marking/datasheet G/S/D and dimensions. A documented same-device supplier is permitted if actual original is unidentifiable; retain topology and characterize RON/leakage/capacitance at observed VGS. Do not assume gate-to-GND is VGS | Other assembly yes; blocks range conduction/first passive test, not MCU-only checks |
| K2; QK2/RK2B/RK2PD/DK2; D_TVS SMA/R_TVS_LINK R0805 | **DNP permanently for this baseline** | Fixed R0_BANK only. Never fit K2 and hard link as alternative paths simultaneously. SMAJ120CA branch is not low-voltage ADC protection. No live active-guard component exists; don't omit U4 “as guard” | No blocker for baseline operation; a different topology requires separate review, not this task |
| J_TFT HDR1×9; R_TFT_LED R0805 | Header **POPULATE**; LED link **HOLD/open — R02** | Owner confirms intended 1×9 module. Verify actual labels/cable:3V3,GND,CS,RST,DC,MOSI,SCK,LED,MISO. .8 comes from PB0 through 0 Ω with **no power driver**. A controller name/5 V module label does not establish backlight current. Logic-enable input may allow original link only after drive specification; raw LEDs need a separately rated off-default external driver/current limit | Display/backlight can stay disconnected; does not block UART/analog/OSL. Blocks usable standalone display until resolved |
| BUZZER1 Ø12 mm/7.6 mm THT; QBUZZ SOT23/RBBUZZ R1206/RPBUZZ R0805 | **HOLD BUZZER1 — R04**, driver passives may be fitted | BOM 9–15 V active magnetic indicator incompatible; purchased 5 V buzzer still needs exact type, pitch, current and waveform. Passive capacitive transducer matching drive is viable. An inductive replacement needs rated flyback/collector review; RPBUZZ 4.7 k is **not** a flyback diode. Don't substitute merely for another brand | UART/analog/OSL proceeds without sound; only sound feature blocked |

RED disposition: R01 intentional connector and R03 corrected UART cable are owner
decisions, not proof of the actual numbered harness. R02 LED stays open; R04 sound
stays unfitted; R05 module/regulator and R06 final supply remain unresolved. Y02
gate transients, Y03 purchased switches, Y04 clamp/backfeed, Y05 adapter injection,
Y06 contacts, Y07 HG headroom, Y08 filtering/drive, Y09 high-Z loading, Y10 return/
decoupling and Y11 battery/NTC remain physical gates. Historical A05 rows are not
silently marked resolved by this supplement.

**Assemble now:** inspected matching low passives, mandatory links, original ICs,
protection and connectors, with temporary RK1B isolation where needed; leave module,
unidentified switches, LED link, buzzer and battery modules HOLD. No export evidence
proves a fabrication defect requiring a new board. An actual inaccessible short/open
which cannot be repaired locally remains an unresolved limitation, never a proposed
refabrication. Signed physical inspection is still required before first power.

Package/rating sources supplement A05: [Microchip MCP6002](https://ww1.microchip.com/downloads/aemDocuments/documents/MSLD/ProductDocuments/DataSheets/MCP6001-1R-1U-2-4-1-MHz-Low-Power-Op-Amp-DS20001733L.pdf),
[Diodes BAT54S](https://www.diodes.com/datasheet/download/BAT54S.pdf),
[Nexperia 238](https://assets.nexperia.com/documents/data-sheet/74HC_HCT238.pdf),
[TI ULN2003A](https://www.ti.com/lit/gpn/uln2003a),
[BC807](https://www.diodes.com/datasheet/download/BC807-25.pdf),
[Hongfa HFD27](https://source.hongfa.com/Uploads/Product/PDF/HFD27_en.pdf),
[Winbond-authored JV datasheet](https://media.digikey.com/pdf/Data%20Sheets/Winbond%20PDFs/W25Q64JV_RevK_3-10-21.pdf).
Hongfa/BC817 live web access was unavailable in this review; Hongfa's retained A05
datasheet/coil drawing and the A05 pin reconciliation were reused, without inventing
new part specifications. Exact module, probe and buzzer documents remain owner inputs.
