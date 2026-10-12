# B02.0 — first-board manual bench card

**NOT EXECUTED / REQUIRES_BENCH_VALIDATION.** Follow the
[assembly decisions](assembly-decisions.md), then this order. Leave a STOP unresolved;
never retry by bypassing a permit. Record numbers/settings in the
[blank worksheet and expected-response reference](measurement-baseline.md).
Manual readings/notes suffice; no screenshot/CSV obligation for every check.
Apply voltage gates to the measured interval including instrument/probe/offset
uncertainty; a reading whose interval crosses a limit is HOLD, not PASS.

Minimum: magnifier, soldering/DMM continuity, independently powered current-limited 5 V
supply, Hantek DSO2C10 with two compensated 10× probes, verified ST-Link and 3.3 V UART,
Windows PC and one 1 kΩ 1% metal-film resistor. Add 10 kΩ/33 kΩ 1% resistors for the optional
battery-sense fixture. No precision LCR meter required. No cell/charger/module USB,
energized DUT, mains, charged capacitor or external stimulus into TEST terminals.

**All scope grounds: verified GND only** (J_PWR.3 or nearby continuity-verified GND
pad, e.g. U4.4/U5.4). Never VMID/RET/TEST_LO/source/gate. No floated protective earth.
Tips on two signals plus CH1−CH2 is safe only with this common GND arrangement.
Move probes/fixture only while operation idle, K1 SAFE, range/PWM off; for soldering
remove every supply/adapter, discharge and confirm zero volts. One simultaneous pair
per run; separate files/trigger events do not share absolute phase.

Use **tip-referred** volts/div below, set probe switches and channel attenuation 10×,
DC coupling initially. Display actual sample rate/record length; timebase alone does
not establish sampling. Normal/single for transients, no averaging before checking
glitches. Repetitive clean sines may then use 16–32 averages. Short ground springs
on verified nearby GND help ripple observations. Compensate probes on the scope's
output **away from the board**; never feed that output into the RLC terminals.

## First five actions — no excitation

| # / required assembly | Exact check / CH1 tip / CH2 tip (grounds as above) | Initial setting, expected result | STOP / record / next permitted step |
| --- | --- | --- | --- |
| 1 — bare board/loose parts | DMM only, no scope. Rail/net continuity; dry-fit USTM32; numbered J_PWR/J_UART/J_TFT; loose K1 NC/NO | Discharged. Match A05 pads; no unexplained sustained rail short; R02 open, K2/TVS DNP, three links present. R05 header/regulator/HSE/SWD checked | Wrong pinout/unknown part/short → no fitting/power. Record board identity, fitted/DNP and module markings; then finish inspected assembly |
| 2 — assembled power/SAFE network, no display/buzzer/cell | DMM J_PWR.2→.3 and UFLASH.8→.4; CH1=COUT1.1 +5V_A; CH2=UFLASH.8 +3V3 | Supply set 5.0 V **before connection**, initial 100 mA limit (test setting, not budget). 1 V/div both,10 ms/div, ≥1 MSa/s, single CH1 rising 2 V,20%pretrigger. Independent DC windows:5 V rails 4.75–5.25 V,3V3 3.0–3.6 V fromA05 | Sustained current limit/hot part/oscillation/overshoot/backfeed → power down, diagnose, no automatic current increase. Save one rail ramp plus DMM/current/fitted state; then inspect decay |
| 3 — same assembly, adapters removed | CH1 +5V_A / CH2 +3V3 as #2, falling trigger; repeat at 100 ms/div if decay exceeds screen | Ordinary supply-off only; no deliberately forced 3V3-off/AFE-on fault. Look for rail order, plateau/rebound,3V3 powered by a signal. Normal/single, ≥100 kSa/s. CSV useful for these important ramps only | Unexpected 3V3 rise/plateau or analog remaining powered with invalid MCU rail → HOLD active tests/Y04 review. Record rails/start/end/time order; next VMID check if no STOP |
| 4 — both rails stable | CH1=U4.3 VMID_RAW; CH2=R_BYP_VM.2 VMID (U4.2), DMM cross-check | 500 mV/div,20 ms/div, ≥100 kSa/s; nominal (+3V3)/2, ~50 ms RC startup. U4.1 is pre-link output, **not U4.3**. OFF: VEXC at RADC1.2 should approach 0 V, not 1.65 V; RET may float | Oscillation/upper rail saturation/injection or settled VMID outside firmware 1.35–1.95 V → STOP. Record DC/ripple and OFF outputs; proceed to programmed safe boot |
| 5 — verified module/SWD, initial RK1B isolation if required | Program/readback **BRINGUP** with [B01.1 procedure](../../../Firmware/tools/PC_FIRMWARE_DEPLOY.md); controlled independent boot, UART 115200 8N1. CH1=USTM32.17 PB9 / CH2=.16 PB8, GND | 1 V/div,10 ms/div, ≥1 MSa/s; both LOW at boot/fault; no PWM on .5 PA8. SWD may reset/halt/freeze watchdogs; readback is not boot PASS. UART pin 2→adapter RX, pin 3←adapter TX, pin 1 GND; no power/DTR/RTS wires | Unexpected enable/PWM/reset loop/clock or supply fault → STOP. Record image SHA/boot banner and safe outputs. Then restore RK1B with power off and repeat safe boot/contact checks |

**Unknown initial module firmware:** do not let arbitrary code drive the first
board. Before first carrier power, keep RK1B open (RK1PD fitted), verify module NRST
and hold it LOW to GND through the inspected reset connection while checking rails.
A removable 10 kΩ from PA8/USTM32.5 (RF1.1) to GND defines filter input LOW in reset;
it draws only 0.33 mA if a 3.3 V output is accidentally driven. This is a temporary
bench pull-down, not an excitation command or permanent repair. Use the reviewed
SWD reset/halt/program/readback stage, not an unverified reset-pin jumper. After a
known BRINGUP image is installed, remove the temporary reset hold/pull-down with
power off, boot with RK1B still open and prove SAFE before restoring RK1B. Never
hold reset to conceal a power/clamp fault; even in reset the analog rail gate applies.
The external reset hold is only for the initial rail inspection, not a new tool
reset mode. Release it only in the separately reviewed SWD reset/halt stage with
RK1B still open and the rail monitor present. If halt/reset access cannot be
established, STOP; do not invent connect-under-reset overrides or power the target
from the debugger.

## Legitimate sensor/permission setup

5 V at J_PWR.2 alone does not supply .1: RBAT2 pulls ADC_BAT toward 0 and firmware
reports CRITICAL. **Do not feed 5 V directly into VBAT_PROT or spoof ADC/permit data.**
If a reviewed protected battery path is unavailable, use this separately recorded,
removable **BENCH_SENSE_SIMULATION**, no cell/charger connected:

```text
J_PWR.2 verified +5V_SYS -- external10kΩ1% --+-- J_PWR.1 VBAT_PROT
                                         |
                                  external33kΩ1%
                                         |
                                    J_PWR.3 GND
```

Build/check off-board first: unloaded node≈3.84 V at 5.0 V. With the existing 100 k/100 k
RBAT divider, node≈3.70 V and ADC_BAT/RBAT1.2≈1.85 V; source current≈0.130 mA,
resistor dissipation below 1 mW. The same main rail removes a separately powered
backfeed source. 1/4W resistors suffice. Inspect polarity/wiring and measure both
loaded nodes with the DMM; no jumper to PA6. Allow ≥2 s for settling/status refresh,
compare MCU estimate to actual DC values. `lab sensors status` must report fresh
valid battery OK (LOW is allowed by existing policy, but review a LOW bench setup).
This validates a sense input, **not cell protection/charging or regulator safety**.
Remove before fitting final power modules. J_PWR.4 remains unconnected/LOW with its
existing pulls only because no charger exists; never ground real charger VBUS to
obtain permission. NTC 100 kΩ/B3950 gives about 1.65 V at 25°C, not a measured temperature.

Read-only console: `lab fault status`, `lab charger status`, `lab sensors status`,
`lab safety status`, `lab range status`, `lab adc status`, `lab flash info`,
`lab mvp status`, `lab cal status`. Require no safety fault, charger ABSENT, fresh
safe residual/valid VMID/battery and no busy transaction. Disabled-range blocking
is **expected before a session**, not permission to force K1. W25Q EF4017 expected;
blank OSL is uncalibrated. Missing TFT may produce display diagnostics; inspect
safety faults rather than inventing a TFT requirement. Do not run flash selftest.

Before the first active run, check the charger gate with no charger/cell/DUT:
power off, wire a removable 1 kΩ from J_PWR.2 (+5V_SYS) to J_PWR.4 (CHG_VBUS).
This is **BENCH_VBUS_SIMULATION**, not the eventual charger wiring. On stable power,
CH1 tip=USTM32.10 PA15, CH2 tip=QUSB_INH.1 base, grounds GND;1 V/div DC,
10 ms/div ≥1 MSa/s. With nominal values/base≈0.7 V, predict VBUS≈4.43 V,
PA15≈2.41 V and total input current≈0.57 mA (resistor≈0.33 mW).
Require `lab charger status` PRESENT and `lab permit status` ineligible/CHARGER;
no active capture to test rejection. Check that K1_BASE remains LOW. If GPIO HIGH
is not established at the measured supply/actual divider tolerances, STOP and
review; do not override it. Remove the fixture with power off and verify ABSENT
again. Residual safety comes from the actual empty-terminal sensing network;
never connect a live test voltage to prove the residual gate in this procedure.

## Controlled analog progression — only after all earlier gates

| Step / assembly prerequisite | CH1 / CH2 tips; manual operation | Initial acquisition / expectation | STOP / evidence / next step |
| --- | --- | --- | --- |
| 6 — RK1B restored, all six switches identified; no DUT | PB8/U2.6 / U2.1(A0); then separate runs QSEL5.1 gate / .2 source, GND clips | Start 1 V/div,2 ms/div ≥1 MSa/s; manually `lab range 1k`, then `lab range off`. Verify disable-before-address, one selected gate, others VGS≈0. Later map six addresses with same gated range commands, no forced GPIO. Gate≈5 V is relative to source, not a 5 V VGS claim | Overlap/off path conducting/rail glitch → STOP. DMM unpowered K1 NC/NO first; record gate/source and decoder map. Next finite passive fixture |
| 7 — relay/contact and Y04 idle gates reviewed; fit 1 kΩ passive resistor with operation idle | Run A: PA8/USTM32.5 / VEXC RADC1.2. `lab metrology measure 100 100 1k` **once** | 1 V/div PA8,100 mV/div VEXC centered around 1.65 V;20 ms/div, ≥5 MSa/s normal/single; sine burst includes 80 ms settle+40 ms ADC block. If single trigger can't separate neutral/sine, use sequence once and inspect stored record; no endless retries | Fault, wrong condition, rail/current movement, clipping/teardown failure → STOP. Record burst/envelope; a separate authorized repeat at 1 µs/div, ≥50 MSa/s resolves the 450 kHz carrier. Low-risk first excitation uses matched LOAD, **not OPEN**. No qualified R claim yet |
| 8 — same resistor, no STOP | Run B: CH1 VEXC / CH2 VMID RADC2.2, both GND. Same one-shot command; CH1−CH2 math | 100 mV/div around common 1.65 V,5 ms/div ≥1 MSa/s. Vs≈101 mVrms/285 mVpp at 100 Hz predicted; Vm nominal 1.65 V. For permitted later 1 kHz/10 kHz runs use 500 µs/div ≥5 MSa/s /50 µs/div ≥20 MSa/s respectively (frequency tokens `1k`/`10k`, keep amplitude `100` and range `1k`); sine settle+capture windows 12/6.6 ms make single acquisition necessary | Unexplained bias/ripple/shape or saturation → HOLD. Record Vs frequency/pp/ACRMS, not total RMS of biased signal. Next RET overlap/clamp observations |
| 9 — same resistor, Y04 remains under observation | Separate run CH1=U5.1 RET_1X / CH2=U5.7 RET_HG, both GND; repeat fixed 100 Hz/100 mV/1 k once | 100 mV/div 1X,500 mV/div HG with DC centering;5 ms/div ≥1 MSa/s. Matched LOAD: ~50 mVrms 1X,~0.78 Vrms HG, both about 1.65 V; gain≈15.47 only a prediction. Compare AC amplitude/crossing delay after removing means, optionally swap probes/channels | HG rail, both paths invalid, gain unstable with rails/probes → STOP/review. Same-run phases only; probes not on RET high-Z node. Next Y04 active gate |
| 10 — approved low-energy matched LOAD, no intentional saturation | Clamp run CH1=RADC4.1/U5.7 / CH2=RADC4.2/DCL4.3/PA3. Then **another** run CH1=U5.7 / CH2=UFLASH.8 +3V3; same fixed command | Start 1 V/div,5 ms/div ≥1 MSa/s DC full waveform; if in range, zoom both channels equally. I_R=(CH1−CH2)/fitted 1 kΩ. Recheck other RADC pairs/idle/off as needed. Rails and output/ADC are separate 2CH runs, never 3 simultaneous traces | ADC outside 0..measured 3V3, significant drop associated with rail rise, phantom power or unrelated sensor shift → STOP all further captures. Record important transient CSV only if useful; estimator is not diode-current measurement. OPEN remains HOLD until electrical review |
| 11 — Y04 idle/ramp and matched LOAD at 10 kHz reviewed; OPEN 100 mV/1 kΩ/10 kHz specifically authorized | Empty passive terminals; `lab cal acquire open 10k 100 1k`. CH1=U5.7 / CH2=UFLASH.8 +3V3;1 ms/div ≥20 MSa/s, single CH1 rising 2.0 V during sine. Repeat output/ADC pair only after reviewing first run | After first measuring actual Vs at 10 kHz, predict HG≈1.12 Vpeak about VMID, within ADC; confirm actual headroom/no clipping. Unlike 100 Hz OPEN, attenuation makes this an initially lower-headroom-risk allowed key. `lab cal cancel`, then wait for CANCELED/SAFE/range OFF; no automatic retry | If headroom/rail gate unavailable, **do not run OPEN**. 100 Hz OPEN and 500 mV keys remain HOLD pending separate clamp review. Rejected acquisition stays rejected; no invented OPEN coefficient |
| 12 — no STOP, previously checked matched LOAD condition | Reconnect 1 kΩ resistor idle/SAFE. `lab metrology measure 100 100 1k` once, probes removed from RET/fixture | Read METROLOGY/DSP output to completion; condition 100 Hz/100 mV/1 k, selected path and complex R/X. Blank unit reports IDEAL/UNQUALIFIED; expect order-of-magnitude 1 kΩ, do not require a precision interval before OSL | Invalid/negative-loss/unexpected gain or unsafe teardown → HOLD. Record resistor ID/tolerance, R/X and status; decide electrical functionality versus full OSL readiness separately |

Fixed `lab metrology measure` has bounded FSM/DMA timeouts but **no dedicated text
cancel command**. `lab measure cancel` applies only to the different auto session;
do not present it as fixed-session cancellation. Observe the burst to completion;
an unsafe event uses the reviewed supply-off procedure, not a forced relay command.
The raw `lab metrology capture` keeps K1 SAFE but is **not the permit-gated DUT path**;
it is deliberately excluded here rather than used to evade interlocks. No DC pilot.

## Proceed only when evidence allows

Existing firmware is sufficient for the fixed matched-resistor experiment and
blank-device service acquisition **with real sensors/permits**. No new command or
firmware change is needed. Real R05/HSE/power/contact/clamp/observability can still
block it. No neutral-tone hold mode is invented; scope the existing finite bursts.

After authorized OPEN/SHORT/LOAD checks, deploy BRINGUP_CAL separately through SWD,
close the text terminal and use [identify/status/capture](../../../Firmware/tools/PC_CAL_CAPTURE.md)
or the [workbench](../../../Firmware/tools/PC_OSL_WORKBENCH.md). It is PLC1, not the
text console. Full 99 real observations/33 usable triplets are required; any physically
unobservable key blocks the unchanged full-set contract. Do not auto-start a full
OPEN-first campaign before individually reviewing conditions. Solve/validate, then
authorize [installation/readback](../../../Firmware/tools/PC_OSL_INSTALL.md)
separately; only then SWD deploy factory PRODUCT and manually click-to-measure.
Resource fallback does not waive OSL or safety gates. Never call an initial IDEAL
result calibrated or a complete OSL frame an absolute-accuracy qualification.
