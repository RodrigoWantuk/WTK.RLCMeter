# A05 — physical inspection and staged bring-up

**NOT EXECUTED — NEEDS_OWNER_INSPECTION / REQUIRES_BENCH_VALIDATION.**
Record operator/date, board photographs/revision/order ZIP, firmware SHA/profile,
instrument identities/settings, part markings, DNP state and a PASS/STOP result
for every row. Empty evidence is not PASS. Resolve applicable RED findings first.
POPULATE in the [matrix](assembly-population.csv) is reviewed assembly intent;
HOLD parts must remain unfitted/disconnected until their finding is resolved.

Only de-energized passive fixtures are authorized. No external live-voltage DUT,
charged capacitor, mains, insulation test or DC experiment is included. Disconnect
cell, charger, USB-UART, SWD and scope during resistance/continuity checks. Discharge
board supply capacitors and confirm zero voltage before using an ohmmeter.

The Hantek DSO2C10 probe grounds are earth-referenced/common: attach **both only to
actual GND** (J_PWR.3, J_UART.1 or a verified GND pad), never VMID, RET, gate/source,
TEST_LO or a power rail. For gate-source or VEXC−VMID use CH1 and CH2 tips on the two
nodes, with both grounds at GND, then CH1−CH2 math after checking common-mode and
input ratings. Do not float the scope earth. Scope amplitude is waveform evidence,
not a precision LOAD or ADC voltage-scale reference. Avoid its probe loading during
high-Z calibration captures. Use a DMM for independent DC rail measurements.

## Stage 1 — unpowered inspection (before and after soldering)

| ID | Exact point / setting | Expected result | STOP condition | Evidence to record | Board required |
| --- | --- | --- | --- | --- | --- |
| 1.1 | Bare PCB, magnifier; compare top/bottom render and order | 100×100 mm outline, actual 1×9 J_TFT and 40-pad USTM32 footprint; order matches audited Gerber ZIP | Wrong revision, damaged mask, copper bridge, incomplete hole plating | Both-side photos, order/ZIP hash | Bare board |
| 1.2 | All parts, magnifier/datasheet; top-view pin1 for IC/transistors; Hongfa bottom-view diagram translated to top | Exact packages/markings; BC807/817 B1/E2/C3; MOSFET G1/S2/D3 after actual vendor check; BAT54S correct series type; UFLASH wide SS package | Unidentified MOSFET lot/pinout, reversed diode/electrolytic, wrong footprint | Part bag/marking/orientation photos; list substitutions | Loose parts + PCB |
| 1.3 | Population CSV vs each footprint | K2 and five-part driver chain absent; TVS/link absent; R0_BANK and both U4 bypass links present; HOLD items left open | K2+hard link both fitted, either mandatory U4 link omitted, R_TFT_LED shorted before approval | Signed fitted/DNP worksheet | Partly assembled |
| 1.4 | USTM32 loose module/header, DMM continuity; no power | Pads18 +5V,20/38 3V3,21 backupVBAT,19/39/40 GND; 15.24mm row spacing/2.54mm pitch; SWD accessible; HSE/regulator/USB circuitry identified | Header/power mismatch, uncertain module regulator or PA11/12 USB loading | Module model, both-side photos, all header checks | Loose module + bare PCB |
| 1.5 | J_UART .1/.2/.3 to USTM32 .19/.6/.7, continuity; adapter labels | GND/TX/RX respectively. Cable RX→.2 and TX→.3; no power wire | TX-to-TX cable, 5-V/RS232 adapter, unknown connector viewing orientation | Numbered cable/connector photo and continuity results | Bare/assembled + loose cable |
| 1.6 | J_TFT .1..9, DMM continuity to matrix; actual display connector | +3V3,GND,CS,RST,DC,MOSI,SCK,LED,MISO. Pitch/pin order and supply match checked adapter | Any different order, module identity unknown, raw LED tied to PB0 | Actual TFT model/labels, numbered wire map, separate LED input specification | Bare/assembled + loose display |
| 1.7 | Rail resistance J_PWR.2/.3, UFLASH.8/.4, RA/+5V_A to GND, 3V3-to5V; DMM ohms both directions after discharge | Bare board: isolated distinct rails. Assembled: capacitor charging/semiconductor paths explain values; no sustained near-zero short | Persistent near-zero rail short, unexpected 3V3/5V hard link | Initial and settled resistance/polarity; fitted state | Bare then assembled |
| 1.8 | Critical-net continuity: REN→GND, U2.4/.5→GND, RGS gate-to-source, RK1PD→GND, R0_BANK LOWZ_BUS→RET, U4.1/7 through bypass to feedback/output | Correct pull/bridge paths and distinct range gate nets; no soldered adjacent-pin bridge | Missing SAFE pull, crossed gate/source/drain, any extra range bridge | Probe endpoints/results, magnified solder photos | Bare then assembled |
| 1.9 | Loose K1 coil .1/.16 DMM ohms; contacts .13/.11, .4/.6 and NO .13/.9, .4/.8; assembled terminal path continuity | Coil nominal125Ω ±10% at23°C for005-S; NC closed, NO open with no coil power; SAFE_HI/LO bleed nominal94kΩ | Wrong relay code/latching/type, NC/NO inversion, stuck contacts | Relay marking and all resistance values | Loose relay; repeat assembled |

## Stage 2 — one current-limited power source, no DUT/display/buzzer

Do not start with an unidentified 18650/charge/boost assembly. First use a
regulated bench 5 V source at J_PWR.2/.3, with battery/charger/debug power leads
disconnected. The verified module must provide a valid 3V3 rail; do not separately
power the 5V AFE while 3V3 is absent. An initial **100 mA** limit is a cautious test
setting, not a validated budget. Stop if it limits or a rail is unstable; diagnose
before increasing it. No K1 operation until quiescent loads and actual supply/
converter/regulator ratings justify the additional coil current. VBAT_PROT is a
sense input from the protected cell path, not an alternative carrier power input.

| ID | Exact point / setting | Expected result | STOP condition | Evidence | Board required |
| --- | --- | --- | --- | --- | --- |
| 2.1 | J_PWR.2/GND DMM DC; +5V_A at COUT1.1; +3V3 at UFLASH.8; scope rail ramps with GND clips | Bench5.0V; +5V_A close to5V minus RA drop; regulated3V3 near3.3V. For first gate require3V3 within3.0–3.6V and5V rails within4.75–5.25V (engineering test windows) | Current limit, overvoltage, oscillation, hot part, 5V AFE present while3V3 invalid/clamp injection occurs | Rail/current/time traces and temperature observation | Assembled |
| 2.2 | PB9/K1_BASE/coil-low, PB8/U2.6, PA11, PA8 scope/DMM during reset | K1_CMD/RANGE_EN/K2_CMD/PWM inactive; K1 NC continuity; all gates follow their sources when off | Coil energizes, range gate VGS stays positive, unsafe boot pulse/contact path | Reset/start/stop traces with fitted population | Assembled |
| 2.3 | PA15/USTM32.10, J_PWR.4 DMM; only after identified service5V VBUS test input is wired safely, never a DUT voltage | PA15 near0 withoutVBUS, nominal2.727V with5V VBUS; firmware charger PRESENT blocks capture; QUSB_INH holds K1_BASE low | Wrong detect level, charger not reported, base not inhibited, unexpected supply backfeed | Both VBUS states, raw/digital status and K1 trace | Assembled |
| 2.4 | Module SWDIO/SWCLK/GND/VTref; 3V3-target SWD low initial speed; no programmer supply output | MCU ID/read access, verified C8 flash size; safe GPIO across program/reset; 8MHz HSE/72MHz clock status | Wrong target, SWD pulls power rail, clock fault, loss of SAFE | Probe/cable map, ID, firmware SHA, boot log | Assembled |
| 2.5 | BRINGUP USART115200 8N1: `lab flash info`; UFLASH CS/WP/HOLD pads1/3/7 DMM/logic probe | JEDEC EF4017 for specified64Mbit Winbond;3V3 supply and inactiveCS HIGH; source WP/HOLD pull-ups present | Wrong ID/voltage, CS contention, no response; do not erase to fix detection | Read-only JEDEC/status/log | Assembled |
| 2.6 | Target supply OFF, separately attach UART/SWD driven signals; DMM3V3/5V rails; disconnect after each observation | No significant phantom rail rise; module nativeUSB remains disconnected | Any target rail becomes powered from signal/adapter | Adapter state/rail readings; corrected cable configuration | Assembled |

## Stage 3 — digital peripherals

Use BRINGUP's diagnostic console for this stage. BRINGUP_CAL instead speaks binary
PLC1 and does not accept `lab` text commands. Never attach the TFT until R01/R02 are
resolved; leave buzzer absent until R04 is resolved. No DUT or calibration write yet.

| ID | Exact point / setting | Expected result | STOP condition | Evidence | Board required |
| --- | --- | --- | --- | --- | --- |
| 3.1 | J_UART TX/RX,115200 8N1; `lab fault status`, `lab charger status`, `lab sensors status`, `lab range status`, `lab adc status` | Consistent boot/status without capture; no reset loop; expected blockers retained if battery sensing not yet provided | Garbled frames, unexpected faults/permission, K1 motion | Full boot/status log and rail/current state | Assembled |
| 3.2 | Approved display on J_TFT; scope SPI and PB0 logic with isolated verified driver/input | Correct reset/image/color/orientation; separateCS, no MISO contention, no GPIO overload | Wrong pin voltage, backlight current through PB0, Flash fails with display attached | TFT model, interface/load currents, screen photos, SPI traces | Assembled + approved display |
| 3.3 | J_BTN2/3/4 closures individually to1/GND; observe PB3/PC13/PB4 | UP/OK/DOWN activeLOW, releaseHIGH; no relay activation from an unqualified request | Wrong button mapping or direct MEASURE without permit | Button/status log | Assembled |
| 3.4 | Approved buzzer only; PB1/QBUZZ collector with GND scope | Passive tone follows firmware waveform; commanded off is quiet; supply stable | Wrong/current-heavy magnetic load, collector transient beyond rating, unstable rails | Replacement datasheet, waveform/current/sound observation | Assembled + approved transducer |
| 3.5 | Normal reset and available controlled watchdog diagnostic with empty terminals; PB9/PB8/PA8 scope | Safe outputs before/after reset; IWDG reason recorded; no contact unsafe state | K1 remains MEASURE or excitation continues after fault/reset | Reset reason + safe-output traces; if no controlled stall facility, mark watchdog test pending | Assembled |

## Stage 4 — analog and safety, empty terminals only

| ID | Exact point / setting | Expected result | STOP condition | Evidence | Board required |
| --- | --- | --- | --- | --- | --- |
| 4.1 | VMID_RAW/U4.3, VMID/U4.2/USTM32.26, RET_1X/RET_HG, DMM+10× GND scope | VMID nominal3V3/2; no oscillation; firmware VMID window1.35–1.95V is a guard, not an accuracy spec. All ADC pins stay0..3V3 | Saturation/rail injection, VMID invalid/oscillating, unexplained HG rail | DC values/ripple, ADC raw/status with supply state | Assembled |
| 4.2 | PWM_EXC USTM32.5, FILT1/2/3, VEXC U4.6; GND scope and safe CH1−CH2 againstVMID | Allowed waveform only through existing service/permissions after source limits accepted; planned450kHz carrier; loaded RC response recorded | Uncommanded excitation, excessive ripple/amplitude/current, ADC clipping on required path | PWM/filter/source traces per frequency/amplitude; no rawGPIO forcing | Assembled |
| 4.3 | Empty TEST_HI/LO, SENSE_HI/LO and ADC_OV_HI/LO; DMM+`lab sensors status` | Settled residual safe/known, sense nearVMID for empty fixture; no clamp rail state. Blocked/invalid states remain blocked | Status says safe despite wrong/rail ADC or invalidVMID | Raw sensor/status + DMM; actual energized-DUT rejection test remains pending separate approved fixture | Assembled |
| 4.4 | `lab range 1m`, then10k/100k/1k/100r/10r and `lab range off`, no DUT; probe U2 Y and all gate/source pairs | Addresses0..5 only; disable precedes address changes; dead time2ms/settle5ms in code; exactly one positiveVGS after settling, zero when off | Any overlap, disabled range still on, gate overvoltage, supply glitch | Decoder truth table, selected/off VGS and current, reset/power decay | Assembled |
| 4.5 | K1 terminals/contact points, coil-low and PB9 while the authorized session FSM requests then cancels/aborts; no DUT | K1 only energizes with permit; tears down to NC SAFE, PWM off and range off; charger assertion prohibits capture | NC/NO wrong, stuck/bouncing excessive contact, failed teardown/inhibit | Coil/contact/enable/excitation timeline including cancel/reset | Assembled; earlier gates passed |

An empty OPEN fixture is a controlled passive capture, not permission to short any
GPIO or force the relay. If BRINGUP cannot issue an operation through its safety
API, stop and diagnose the blocker; do not bypass it. Unknown battery/residual
status must be corrected using the identified protected power path or a reviewed
current-limited battery-sense simulator on J_PWR.1/GND, never by faking ADC values.

## Stage 5 — calibration capture, persistence and standalone check

Build/deploy BRINGUP_CAL over SWD from the synchronized revision. Do not interpret
the software's synthetic tests as completion of any checklist row. Use the exact
3.3V cable established in1.5; no nativeUSB firmware switch exists.

| ID | Exact point / setting | Expected result | STOP condition | Evidence | Board required |
| --- | --- | --- | --- | --- | --- |
| 5.1 | `pc_cal_capture.py identify/status --port COM5`; no START yet | Rev1/model4, BRINGUP_CAL, capture/install capabilities; explicit uncalibrated state on blank Flash; settled valid sensors, safe teardown | Wrong identity, safety fault/blocker inconsistent with known state, falsely qualified blank result | Identity/statusJSON, firmwareSHA, cable/supply/NTC | Assembled |
| 5.2 | Empty OPEN then passiveSHORT then knownLOAD for an already electrically checked key; first use low excitation; inspect returned six-channel artifact | Exact requested key/standard, ADC nominal provenance, clipping/quality and SHA retained; no QUALIFIED assertion | Wrong condition, timeout, clipping/invalid required path, no SAFE teardown, repeat instability | Campaign raw records, LOAD ID/tolerance/measurement, scope traces acquired separately | Assembled + passive fixtures |
| 5.3 | Repeat allowed33 keys only where prior waveform/current tests passed | 99 genuine observations if all keys observable;10ohm500mV rejected; unavailable keys recorded as blockers, never invented | Any electrically invalid key; do not force a complete candidate | Key-by-key raw validity/limits and repeats | Assembled + references |
| 5.4 | Read-only storage/status first; back up any existing W25Q contents through a verified read-only procedure before first write; PC candidate validation | Correct schema2/model4, full coverage and queried successor; unrelated resource/settings partitions identified and preserved | No verified backup on occupied media, unknown partition/sequence, candidate invalid or self-qualified | Pre-write hashes/status, candidate/report/partition map | Assembled + complete valid campaign |
| 5.5 | `pc_osl_install.py install`, then readback/reset/readback; bounded real service only | Device INSTALLED + verified readbackCRC/bytes/sequence; after reboot same usable record; safe outputs throughout | Transfer-only success, CRC mismatch, active older record lost, write outside calibration slots, unsafe hardware state | Installation report and both readback hashes/status + scope SAFE trace | Assembled; all earlier gates passed |
| 5.6 | Flash PRODUCT overSWD while preserving externalW25Q; provision Resource Pack separately if missing | Full-set OSL loaded; resources/boot prerequisites still enforced; standalone controlled passive measurement only when gates satisfied | Wizard/boot gate bypass, missing resources presented as READY, unqualified output promoted to accuracy guarantee | PRODUCTSHA/profile, boot/calibration provenance and passive holdout results | Assembled + provisioned board |

From `Firmware/`, the first capture example below is valid only after its physical
condition has passed Stage4; it does not preauthorize all keys:

```powershell
cmake --preset stm32-bringup-cal
cmake --build --preset stm32-bringup-cal
python tools/pc_cal_capture.py identify --port COM5
python tools/pc_cal_capture.py status --port COM5
python tools/pc_cal_capture.py capture --port COM5 --rref 1000 --frequency 100 --amplitude 100 --standard OPEN --campaign captures.json
python tools/pc_osl_install.py status --port COM5
# Only after all required genuine captures; sequence must equal queried next_sequence.
python tools/pc_osl_calibrate.py build captures.json --sequence 1 --out candidate.bin --report calibration.md
python tools/pc_osl_install.py install --port COM5 --candidate candidate.bin --report installation.json --readback installed.bin
python tools/pc_osl_install.py readback --port COM5 --readback after-reset.bin
```

These are existing executable commands, not tests run on a board in A05. Complete
capture/install contracts and their limitations remain in
[PC_CAL_CAPTURE](../../../Firmware/tools/PC_CAL_CAPTURE.md) and
[PC_OSL_INSTALL](../../../Firmware/tools/PC_OSL_INSTALL.md). Do not use a global
chip erase or exploratory raw Flash write as a calibration diagnostic.
