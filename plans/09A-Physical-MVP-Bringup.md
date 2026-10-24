# Phase 09A: Physical MVP Bring-up

STATUS: IMPLEMENTED_REQUIRES_BENCH_VALIDATION

## External-supply diagnostic — 2026-10-24

The instrumented 30 ms attempt on `3ce13efa97dc` also timed out. Actual pre-stop
trace: DMA CNDTR=765/768, no complete/error, TIM2 enabled with compare flags set,
ADC1 CR2=0x00160101, ADC2 CR2=0x00000001. Evidence:
`physical-20261024-relay-30ms/load-10k-001/`. This proves only three transferred
words, not a valid block or resistor result. Cleanup was SAFE/disabled/faults0.
Inspection against RM0008 section 11.9 and the pinned ST HAL
`HAL_ADCEx_MultiModeStart_DMA` found the slave trigger setup violated the documented
contract: both ADCs must enable EXTTRIG and the slave must select software trigger.
ADC2 now uses ADON | EXTTRIG | EXTSEL (111), with no SWSTART write. Timer mapping,
PA1 analog/CC2E=0, ADC schedule, sample rate, DMA width/count and safety sequencing
are unchanged. This is a Phase 05 BSP configuration correction; the physical
before/after capture is its required hardware regression, still pending here.

Primary references: [ST RM0008 section 11.9](https://community.st.com/ysqtg83639/attachments/ysqtg83639/stm32-mcu-products-forum/80598/1/13902_STM32_ReferenceManual_11.pdf)
and the pinned `Firmware/third_party/st/stm32f1xx_hal_driver/Src/stm32f1xx_hal_adc_ex.c`
multimode start validation. ARM strict bench build/size/profile checks pass at
64372 B Flash / 14852 B accounted RAM. The host FSM/RAW guard tests remain the
previous 41 CTests + 8 collector tests; they do not emulate this hardware defect.

First reference attempt: the owner attached an isolated nominal 10 kOhm 1%
resistor, reporting a 10000 Ohm DMM indication (instrument uncertainty not
provided). Actual boot `8c13549485fa` and source confirmation passed. The first
documented command spelling was refused before capture; examples now use the
actual compact `1k 100m 10k` tokens. A corrected single DUT attempt started,
reached ADC/DMA TIMEOUT and returned K1 SAFE/range DISABLED with zero latched
faults. No RAW frame/resistance result exists. Evidence is under the local
`physical-20261024-bench-supply/load-10k-001/` and `load-10k-002/` directories.

The owner subsequently requested 25–30 ms relay settling; the DUT operate guard
is now 30 ms with excitation settling still separate and release guard unchanged.
Host regression checks the deadline before/at expiry across uint32 wrap, and PC
RAW validation accepts legacy 10 ms and current 30 ms metadata. An isolated bench
ADC trace now captures status/control, DMA remaining count and TIM2 state before
stop/restore on completion/timeout, then exposes it with `lab adc capture status`
after cleanup. It reads no ADC data registers, performs no UART in ISRs, and is
excluded from default/PRODUCT images. The timeout cause is unresolved; neither
the timer change nor traces constitute physical timing/accuracy qualification.

Software checks for this follow-up: six relevant host CTests and all eight RAW
collector tests pass, including 30 ms/legacy 10 ms metadata validation. ARM bench
warnings-as-errors build passes at 64368 B Flash / 14852 B accounted RAM including
2048 B reserved stack. The trace adds 72 B static RAM, excluded from ordinary
profiles. Physical timeout diagnosis remains pending the instrumented attempt.

The owner explicitly requested bench measurements without a battery or valid
calibration before connecting isolated 1% resistors. The bounded sub-phase adds
an opt-in BRINGUP-only source policy with explicit RAM-only operator confirmation,
reset-to-battery defaults, real unchanged ADC evidence, bound/revocable permits,
nominal 100 mVrms limit and the existing complete DUT acquisition/abort sequence.
No later UI, calibration model or DC feature is included. Expected files are
CMake/build configuration, safety/permit/measurement services, BRINGUP console and
shell integration, deployment identity, host regressions and this contract:
[BENCH_SUPPLY.md](../Firmware/tools/BENCH_SUPPLY.md).
Prerequisites are the existing Phase 04/05 safety/acquisition path and Phase 09A
ideal/unqualified measurement fallback. This explicit owner request extends the
earlier battery-only assumption; default and PRODUCT battery gates are retained.
Acceptance requires host guard/revocation/cleanup tests, ARM build/size gates,
profile isolation and traceable deployment preparation. Physical OPEN/SHORT/LOAD,
supply integrity, waveforms and reference accuracy remain REQUIRES_BENCH_VALIDATION.

Initial software verification passed: 41/41 host CTests (MSVC Release, project
warnings as errors), five relevant tests repeated after final initialization
changes, 49 deployment-tool tests passed with three environment-dependent skips,
and eight profile-tool tests passed. ARM project warnings-as-errors builds pass:
bench image 63748 B Flash / 14780 B accounted RAM including 2048 B stack;
PRODUCT 62040 B Flash with bench flag OFF. Default BRINGUP is byte-identical to
the earlier image after normalizing only the embedded Git label. LTO serial-job
and existing soft Flash-target advisories remain; hard memory/profile gates pass.
Physical reference captures and source-revocation/interlock checks remain pending.

## Opt-in empty-fixture K1 pulse — 2026-10-24

The owner requested a one-shot physical relay test after confirming that the
single 1 kHz/200 ms buzzer command was audible. The
[separate diagnostic contract](../Firmware/tools/K1_BENCH_TEST.md) permits a
250 ms K1 pulse with no DUT under the reviewed bench supply, real fresh residual
SAFE/VMID, no charger/fault, range disabled, excitation off and no competing work.
Battery sensing remains unmodified and still blocks ordinary measurements.
The diagnostic uses an explicit hardware-service state machine, one attempt per
boot, cancellation, wrap-safe deadlines, LOW retries after I/O failure and the
8 ms release guard; it never creates `measure_allowed` or bypasses normal permits.
The application services its ownership before safety arbitration and slow work,
bounds UART input, and requalifies residual sensing after release. This is a
cooperative deadline with the existing IWDG/reset fallback, not a hardware cutoff.

`WTK_BRINGUP_K1_TEST` defaults OFF, is allowed only for BRINGUP, and excludes the
experimental DC pilot in that explicit variant. PRODUCT/BRINGUP_CAL exclude the
API/commands; profile-symbol checks enforce this. Deployment tooling explicitly
builds and seals the variant identity. Initial verification: all 41 host CTests
pass, including the new guard/failure/wrap/cancel tests; default BRINGUP remains
byte-identical to the original image at the same Git identity, and PRODUCT Release
builds with no relay-test symbols. The new BRINGUP variant fits 64 KiB Flash and
20 KiB RAM with the reserved stack preserved. Physical pulse/contact/current/
reset/interlock acceptance remains REQUIRES_BENCH_VALIDATION until actual evidence.

## First-board preparation — 2026-10-24

The owner reports an assembled board, a repaired COUT solder bridge, stable supply
rails, K1 off, BOOT0/BOOT1 at zero, TFT intentionally disconnected and a CH340 on
COM3. These statements are not numeric rail/current measurements or completed
physical acceptance. The actual ST-Link/V2 USB descriptor uses the legacy 26-byte
serial representation already handled by the pinned OpenOCD alternate-serial
callback. Its exact hexadecimal representation passes the existing deployment
dry run and OpenOCD configuration without `init`; no programmer code change or
serial-filter bypass is needed. The earlier assumption that this descriptor alone
made the adapter unsupported was incorrect. Probe communication, MCU identity,
program/readback and controlled boot remain REQUIRES_BENCH_VALIDATION; the physical
evidence and authorization gates remain in force.

The first attempted probe stopped before adapter/target initialization because
the launcher selected `hla_swd` with the pinned native `st-link` interface. The
launcher now selects the script's supported `dapdirect_swd` transport. A regression
test first reproduced that failure, then checks the full actual interface/target
configuration under the pinned OpenOCD with shutdown before `init`. This changes
only PC deployment configuration; firmware and physical qualification are unchanged.
The first actual target connection reported the expected Cortex-M3/64 KiB identity,
but Windows CRLF output exposed a target-marker parser defect. Parsing now normalizes
CRLF without changing retained transcript bytes or duplicate-marker rejection; a
regression reproduces the actual format. Identification is not program/boot or
analog qualification evidence.

Physical programming then succeeded on the owner's board: BRINGUP 0.1.0 from clean
commit `87339a054a6f2b82e1c3d16e93e6c3c7cd684664`, 65248 image bytes verified
byte-for-byte against internal Flash. A complete 64 KiB backup of the previous
PC13 LED test application was saved before erase; W25Q was not accessed. The
unchanged page-by-page erase and UID/geometry/protection checks ran. Deployment
proceeded on the owner's explicit setup attestation; numerical DMM readings and
photographs were not transmitted, and no archival B01 PASS evidence was invented.
Local evidence is under `Firmware/build/physical-20261024-bringup/`, especially
`physical-program-001/deployment.json` and `PREPARATION.md`. The target was left
halted with a passive COM3 listener prepared. Boot/UART identity, electrical SAFE
behavior and metrology/calibration remain pending. The deployment tests with the
pinned executable enabled pass: 45 passed, 3 skipped; phase status is unchanged.

The owner then pressed RESET. Actual UART evidence confirms BRINGUP 0.1.0,
Git `87339a054a6f`, PIN_RESET, HSE_PLL 72 MHz and ADC clock 12 MHz. Nine audited
status-only commands report no latched faults, charger absent, K1 SAFE and range
disabled. W25Q32 (JEDEC EF4016, 4 MiB) is detected; no valid calibration is present.
Auxiliary ADC estimates report VMID 1658 mV, residual SAFE and valid NTC telemetry.
Battery reports UNKNOWN at 10 mV (raw 6); the owner confirms J_PWR.1/VBAT_PROT is
floating. Safety flags 28 therefore include SENSOR_INVALID, SUPPLY and the idle
RANGE blocker. No acquisition, range enable or calibration operation was attempted.
The missing battery input must be resolved through a reviewed power arrangement;
do not spoof the ADC or bypass safety permits. UART status is commanded/reported
evidence, not physical relay/contact, rail, waveform or accuracy qualification.
The disconnected TFT still reports READY, which is not proof of physical display
presence or rendering. Actual boot and status logs are in the same local evidence
directory (`first-boot-passive-001/`, `first-status-001/`, `observed-boot.json`).

At the owner's request, after the owner identified the fitted buzzer as passive
5 V, one `lab buzzer 1000 200` command was accepted with `lab buzzer: OK`.
Pre/post status reports K1 SAFE, range DISABLED and zero latched faults. Audible
sound, waveform, current and transducer suitability remain owner/bench observations;
command acceptance does not qualify them. `lab permit status` explicitly returned
`eligible=0 reason=BATTERY`; K1 was not requested or forced. No standalone K1 pulse
command exists in the deployed console. Resolve the real battery-sense/power setup
before the existing permitted acquisition sequence can exercise K1. Evidence:
`buzzer-physical-001/` in the local first-board directory.

## B01.1 SWD deployment toolkit — 2026-10-11

[Safe image preparation and SWD handoff](../Firmware/tools/PC_FIRMWARE_DEPLOY.md)
builds/inspects the three exact deployment profiles, uses explicit physical
debug/program authorization and a pinned OpenOCD backend, and verifies internal
Flash bytes. Fake orchestration, the actual offline Tcl interpreter, real linked
images and C-backed calibration transition tests pass. No physical SWD, boot or
W25Q preservation is claimed. Probe/verify may reset/halt and freeze watchdogs;
independent SAFE/power/assembly gates precede any target connection. No firmware
or PCB changes; all 12 ARM variants retain their memory sizes, including BRINGUP's
88-B margin. [Evidence](../docs/review/b01/B01.1-report.md). R02/R05/A05-Y04 and
the existing physical acceptance criteria remain REQUIRES_BENCH_VALIDATION.

## B01.0 first-startup host toolkit — 2026-10-10

[Executable read-only evidence collection](../Firmware/tools/PC_BRINGUP.md)
supports offline preparation, manual provenance, BRINGUP serial snapshots and
deterministic simulated replay. It requires a passive BRINGUP identity banner and
uses only nine audited status/info commands. PRODUCT/BRINGUP_CAL text targets,
active commands and unknown identity are refused. R02, R05 and A05-Y04 remain
explicit pending physical gates. No firmware or PCB change is introduced.
See [test and memory evidence](../docs/review/b01/README.md). B01 physical safe
boot, contact/GPIO behavior and electrical qualification remain unexecuted.

Phase 09A freezes feature expansion and turns the existing firmware into a bench-usable Rev.1 MVP. It does not qualify accuracy, finish visual polish, add Live mode, or start later UI/resource work. Its purpose is to let a freshly assembled Rev.1 board boot safely, expose a compact readiness snapshot, run the real automatic measurement chain from UART, and support the first known passive DUT measurements.

## Implemented Software Boundary

- BRINGUP exposes `lab measure auto`, which reuses `app_measurement_session_t`, `measurement_auto_session_t`, the Phase 05 fixed-condition hardware transaction, Phase 06 DSP, Phase 07 autorange/confidence/classification, and calibration resolution.
- Each automatic attempt still traverses the Phase 05 safety transaction: permit issue/validation, range preparation, K1 ownership, excitation, ADC1/ADC2 DMA capture, return to SAFE, auxiliary ADC restore, and raw-block publication.
- BRINGUP automatic processing allows ideal calibration fallback when no persisted condition exists. UART output reports this as `calibration=IDEAL_UNQUALIFIED` with the calibration resolve status.
- PRODUCT processing still calls calibration resolution without ideal fallback; missing or invalid active calibration remains a normal PRODUCT measurement blocker.
- BRINGUP exposes `lab mvp status` as a compact readiness snapshot over existing services.
- PRODUCT no longer treats missing/corrupt external visual resources as an operational blocker. It keeps safety/calibration blockers intact and uses internal fallback text/rendering for core screens.

## Intentionally Frozen

These remain deferred and must not be pulled into 09A:

- Stage 3B.2 fonts/icons/splash/graphs;
- final typography or animation polish;
- TFT debug console page;
- new localization features;
- Live continuous measurement UX;
- new frequencies, models, calibration domains, or Rev.2 hardware;
- virtual-hardware expansion unrelated to a real regression.

## BRINGUP Commands

```text
lab mvp status
```

Reports clock source/frequency, Flash detection, display readiness, charger state, safety blocker, K1 command state, range state, calibration service state, sensor summaries, resource relevance, and last metrology error.

```text
lab measure auto
```

Runs one click-style automatic measurement session through the real Phase 05/06/07 path. Output includes `AUTO_BEGIN`, `ATTEMPT_BEGIN`, optional `PARTIAL_RESULT`, `AUTO_RESULT`, and `AUTO_END`.

```text
lab measure cancel
```

Requests cancellation of an active automatic BRINGUP measurement. If a hardware transaction is active, cancellation uses the Phase 05 abort/safe cleanup path.

Existing fixed-condition commands remain available:

```text
lab metrology capture <freq> <amp> <range>
lab metrology measure <freq> <amp> <range>
lab cal ...
```

## Automatic Result Format

`AUTO_RESULT` is one parseable line:

```text
AUTO_RESULT status=<auto_status> class=<interpretation> primary_attempt=<index> attempts=<n> value_kind=<R|C|L|Z> value=<float> r_ohm=<float> x_ohm=<float> z_ohm=<float> phase_deg=<float> freq_hz=<hz> amp_mvrms=<mV> range=<range> channel=<RET_1X|RET_HG> calibration=<PERSISTED|IDEAL_UNQUALIFIED|NONE> cal_status=<resolve_status> quality=<quality> qualification=<qualification> confidence=<confidence> reason_flags=<hex>
```

The first-board expectation before real OSL calibration is `calibration=IDEAL_UNQUALIFIED`. That is useful for gross hardware debugging only; it is not accuracy evidence.

## Flash/RAM Policy

PRODUCT now enforces a 60 KiB project hard Flash gate during the final integration stretch. BRINGUP enforces the physical 64 KiB STM32F103C8T6 Flash limit plus the existing 18 KiB accounted-RAM hard gate. This is deliberate: the bench image now links the automatic measurement engine and diagnostic output so the first board can measure through the same high-level path as PRODUCT.

The BRINGUP image is not a product release artifact. If PRODUCT exceeds its hard gate, defer cosmetic/UI/resource functionality before touching safety, calibration, acquisition, DSP, autorange, or basic UI.

## Bench Validation Status

Prepared but not executed:

- K1 SAFE at boot and after every failure path;
- RANGE_EN disabled at boot and during range-address changes;
- excitation inactive at boot/fault and valid at 100 Hz, 1 kHz, and 10 kHz;
- ADC1/ADC2 DMA timing and channel packing;
- automatic BRINGUP measurement on known resistor, capacitor, inductor, OPEN, and SHORT;
- real 33-condition OSL calibration;
- PRODUCT calibrated measurement after persisted calibration is active.

## First Board Sequence

Use [`../docs/16-Rev1-MVP-Bench-Guide.md`](../docs/16-Rev1-MVP-Bench-Guide.md) for the step-by-step procedure.
