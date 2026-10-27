# Production firmware audit — October 27, 2026

STATUS: AUDIT_COMPLETE_IMPLEMENTATION_NOT_STARTED

The owner requested a complete firmware review, with particular attention to
PRODUCT, configuration persistence, audible feedback, genuine measurement data
and a native, quick OPEN/SHORT procedure after changing test leads. This report
records the current implementation and defects. It does **not** certify the
firmware as finished or claim that the proposed corrections are installed.

Baseline: repository `842276976c7f44a5fcb3b7ed287325428f01668d`, branch
`codex/trimmed-measurements`. Installed PRODUCT was built from
`9ecaa696ec785e6c15f240df23f75bd07d40d5ff`, with the first-unit experimental NTC
profile. No target access, measurement, erase, firmware deployment or production
C modification was performed for this audit. Earlier hardware observations keep
their original provenance and qualification limits.

The [execution plan](../../../plans/30-Production-Firmware-Audit-and-Completion.md)
defines the ordered correction work and acceptance tests. Evidence and host-only
reproduction instructions are in [evidence/README.md](evidence/README.md).

## Assessment

The core acquisition/calibration/impedance path is real, but the integrated
product is incomplete. The configuration write path has a reproducible driver
contract mismatch; rapid edits have another independent persistence defect.
The idle timeout is defeated by the normal SAFE idle range state. The buzzer
driver exists and passed basic bring-up, but PRODUCT has almost no event policy.
Production presets do not select the same feature set. Quick lead compensation
does not exist in the installed factory PRODUCT.

Passing unit tests and earlier R/C/L observations do not establish product
readiness. The audit reran 50 factory and 48 standard CTest entries successfully,
then reproduced four scenarios that those tests miss. Three concern settings;
one concerns idle timeout. There are also source-confirmed gaps and untested
fault/interrupt risks that need separate treatment.

Priority means: P0 blocks a release claim; P1 is required for the requested usable
product; P2 improves integrity, observability or completion. Evidence labels are
`REPRODUCED_HOST`, `CONFIRMED_SOURCE`, `STATIC_RISK_REQUIRES_TEST`,
`IMPLEMENTED_BASIC_BENCH_ONLY`, and `REQUIRES_BENCH_VALIDATION`.

## Findings

### F01 — P0: settings save stalls with the real W25Q start contract

**REPRODUCED_HOST + CONFIRMED_SOURCE.**

`app_settings_service.c:457–526` advances its erase/program START states only
after `BSP_STATUS_OK`. The actual `w25q_device_sector_erase_start()` and
`w25q_device_page_program_start()` return `W25Q_STATUS_BUSY` after successfully
starting an asynchronous transaction (`w25q_device.c:253–308`); the settings
adapter maps that to `BSP_STATUS_BUSY`. Consequently settings remain in
ERASE_START, repeatedly call start, and never call poll. The application blocks
generic polling while the settings owner is busy (`app_flash_access.c`).

The host reproduction runs the unchanged settings service with callbacks that
emulate that accepted-start contract: **128 erase calls, zero poll calls, still
ERASE_START**. Existing settings tests use callbacks returning OK on start and
therefore pass. This is a source/host-confirmed explanation of failed persistence;
the audit did not attempt a new physical save. A stalled owner can also block
subsequent measurements, resource reads and other storage owners.

Fix the start/ownership contract explicitly. BUSY also means not started because
another operation or bus owner is busy; changing every caller to treat every
BUSY as acceptance would introduce another bug. Add real adapter/driver contract
integration tests, transaction deadlines and deterministic recovery. Calibration
storage and resource update already advance to polling for accepted BUSY, but
all consumers need the same ownership distinction and regression tests.

### F02 — P1: an edit during save loses the newest value on reboot

**REPRODUCED_HOST.**

`app_settings_service_save_start()` serializes `current` into a frame. Further
edits are allowed while it writes. VERIFY_COMMIT compares the committed snapshot
against the newer `current` (`app_settings_service.c:529–544`). A valid older
snapshot has already been committed when that comparison reports failure.

The isolated reproduction deliberately uses the existing tests' accepted-OK
callback, to get past F01: brightness starts at 60, changes to 65 during save,
the save reports failure, and reboot loads **60**. Fix this independently of F01
using an immutable write snapshot, generation tracking and a bounded latest-value
dirty retry. Coalesce held-button edits; do not erase a sector for every repeat.

### F03 — P2: settings sequence metadata is outside the CRC

**REPRODUCED_HOST.**

The schema-3 frame CRC covers the payload, but not the sequence header used for
slot selection (`app_settings_service.c:187–251`). Flipping the sequence byte at
offset 12 changes sequence 1 to 33; reload still accepts that record as valid.
This is corruption tolerance, not an authentication issue.

Introduce a versioned header-plus-payload checksum, excluding the last-written
commit marker, and validate reserved fields. Read existing supported versions;
upgrade through the inactive slot without destroying the previous valid copy.

### F04 — P1: backlight idle timeout is suppressed in ordinary READY

**REPRODUCED_HOST + CONFIRMED_SOURCE.**

`update_backlight_idle()` forces the display awake whenever
`safety_result.measure_allowed` is false (`app_product.c:920–944`). Ordinary SAFE
idle intentionally has ranges disabled and a RANGE blocker. Scheduling correctly
permits READY in this state, but the timeout treats it as an emergency and resets
`last_activity_ms` every loop.

With actual idle semantics, READY and a 15-second timeout, a step at 20,000 ms
leaves `sleeping=0` and sets `last_activity=20000`. Existing ready fixtures use
`measure_allowed=true` and miss this. Fix UI wake policy using actual critical
states/meaningful blockers; retain the strict hardware measurement permit.
Brightness preview and the PB0 PWM output path already exist; they are not mocks.

### F05 — P1: buzzer hardware exists; product feedback is incomplete

**CONFIRMED_SOURCE; driver IMPLEMENTED_BASIC_BENCH_ONLY.**

`hw_buzzer.c`, its policy and TIM4/PB1 generation are real. In PRODUCT,
`tone_sequence` increments only when sound is re-enabled in the menu
(`app_product.c:1304`); outputs otherwise describe the same 2-kHz/30-ms tone.
Navigation, accepted/rejected actions, measurement completion, save completion
and faults do not have a complete event-to-pattern policy.

`product_apply_outputs()` consumes the sequence before checking whether playback
was accepted and ignores the return status (`app_shell.c:1274–1281`). Implement a
small bounded event/pattern state machine, mute handling, priorities and explicit
quiet-window suppression. Play completion feedback after hardware SAFE teardown.
Do not turn a UI event into direct range/relay control or add blocking delays.

### F06 — P1: pending TIM4 interrupt can defeat buzzer stop

**STATIC_RISK_REQUIRES_TEST.**

`bsp_timer4_buzzer_stop()` clears CEN/UIE and drives PB1 low, but does not clear
UIF/pending NVIC state. `TIM4_IRQHandler()` tests UIF without an armed/CEN/UIE
guard and can toggle the pin after stop (`bsp_timers.c:135–149`). Shared pin state
also needs explicit ISR/main ownership. This interleaving was not physically
observed during the audit.

Make stop and ISR admission deterministic, clear stale interrupt state and test
late IRQ arrival around mute, duration expiry and quiet acquisition. Merely
adding `volatile` does not resolve the ordering problem.

### F07 — P0: fault vectors rely on watchdog reset instead of immediate SAFE

**CONFIRMED_SOURCE; physical timing REQUIRES_BENCH_VALIDATION.**

HardFault and other unhandled vectors alias an infinite `Default_Handler`
(`startup_stm32f103c8tx.c`). There is no immediate emergency GPIO/source shutdown
in that handler. The watchdog is intended to reset after roughly two seconds,
but `app_shell_run()` discards `bsp_watchdog_start()` status (`app_shell.c:1647`).
A failed watchdog start can leave no reset fallback. Do not claim this has caused
an observed energized fault; it is a release-blocking failure-path gap.

Provide a minimal BSP emergency shutdown with no UI, Flash, locks, formatting or
heap dependency. Check watchdog initialization before admitting measurement.
Verify safe output levels even if a fault occurs before complete initialization,
then inject controlled faults on the bench and measure release/source timing.

### F08 — P1: production presets have different measurement contracts

**CONFIRMED_SOURCE.**

`config/wtk_build_config.h.in:23–32` ties continuous ownership, seven-point
trimming, three-frequency survey, row caching and other product behavior to
`WTK_PRODUCT_FACTORY_PROVISIONED`. Installed `stm32-factory-release` enables
these. Ordinary `stm32-release`/`stm32-debug` do not, and retain legacy behavior
and a full calibration wizard. The name PRODUCT alone does not identify the
current measurement algorithm.

Define one supported production feature contract. Separate factory calibration
provisioning from unrelated measurement/UI features. Keep recovery/diagnostic
profiles explicit and preserve host tests of the legacy full OSL workflow where
it remains supported. Record all effective flags in manifests and test both
provisioning variants without silently removing recent fixes.

### F09 — P1: UART reception is fragile; product telemetry is limited

**CONFIRMED_SOURCE + previously recorded bench observation.**

`bsp_uart_try_read_byte()` polls RXNE/DR; there is no bounded RX interrupt ring or
explicit overrun/framing recovery (`bsp_uart.c`). Slow SPI/UI or other cooperative
work can lose a burst. The preceding capacity experiment recorded two burst
status-query timeouts and a successful paced query. That observation does not
prove every timeout has the same cause, but is consistent with this design.

Use bounded RX buffering and error counters with ISR priority below acquisition,
plus parser resynchronization and explicit stale/busy command rejection. Buffered
OK must never become a deferred activation after leaving a quiet/busy state.
Add paced PC transmission as a documented interim mitigation. Do not automatically
retry an activation with ambiguous delivery.

The installed PRODUCT status packet reports state/session/blockers, not full
numeric battery/NTC readings or complete RLC results. Implement versioned fresh
sensor/result telemetry before promising those readings over PRODUCT UART. The
BRINGUP capture protocol has a different capability set. No numeric battery
voltage was read over SWD for this audit.

### F10 — P1: native OPEN/SHORT lead compensation is absent

**CONFIRMED_SOURCE / NOT_IMPLEMENTED.**

Factory PRODUCT only presents calibration status and directs the operator to
PC/BRINGUP_CAL provisioning. It has no separate lead profile, no endpoint-only
wizard and no runtime fixture correction. The full factory OSL procedure includes
LOAD and is not a substitute for the requested quick lead change.

Implement the two-action workflow and complex correction in plan 30. Preserve
the existing 33-record factory OSL set. SHORT addresses series resistance and
inductance; OPEN addresses parallel leakage/capacitance. A scalar resistance
subtraction would not correctly compensate C/L measurements.

### F11 — P0/P1: memory capacity is available, but build and RAM contracts lag

**PHYSICALLY_VERIFIED_UNIT_CAPACITY + LINKER/ARM_ABI_EVIDENCE.**

| Quantity | Bytes | Interpretation |
| --- | ---: | --- |
| Factory size register | 65,536 | Declared C8 capacity; register reads 0x0040 KiB |
| Experimentally usable Flash on this UID | 131,072 | Full upper-half patterns/reset/readback plus code execution near the end |
| Installed image | 61,596 | Exact BIN, not a map estimate |
| Remaining physical Flash to 128 KiB | **69,476** | Approximately 67.85 KiB |
| Free inside current 64-KiB linker | 3,940 | Current build still cannot use the upper half |
| Remaining current PRODUCT hard budget | 2,916 | Hard limit is 64,512 B |
| Static SRAM | 15,068 | Current factory release |
| Reserved stack | 2,048 | Not disposable feature memory |
| Accounted RAM | 17,116 | Current hard limit is 17,408 B |
| Remaining RAM hard-budget margin | **292** | Immediate integration constraint |
| Physical SRAM less static and reserved stack | 3,364 | Includes intentional margin; not all allocatable safely |

Unit UID: `54ff70068370525760380567`. Installed image SHA-256:
`5dd9a53f4117a5239e2ec0769e0560c10ce4114789f7782ef929da4b684aba7d`.
See the [physical capacity evidence](../../../Firmware/releases/2026-10-27/extended-flash-capacity/README.md).
This establishes at least 128 KiB on this unit, without qualifying all C8 devices,
retention, temperature or endurance. The audit does not repeat that experiment.

Create an explicit UID-bound 128-KiB engineering build/deployment profile, with
matching linker, programmer and size gates. Keep the generic C8 profile at
64 KiB. If preserving the current 1,024-B hard-budget reserve, the new hard
limit would be 130,048 B and its available budget **68,452 B**; adoption is an
explicit plan decision, not an already changed setting.

RAM has promising duplication: Arm ABI reports `app_calibration_service_t=5776`,
including a 2,576-B active calibration set and store scan scratch holding another
set. Its factory workflow is 376 B. Investigate a load-only factory service and
scratch lifetime reuse before adding rings/profiles; do not count all those
bytes as recovered until map, stack and regression evidence proves it.

The actual W25Q is EF4016 / 4 MiB. It stores assets and calibration, not executable
MCU instructions or extra SRAM. Its existing mutable tail is 32 KiB and already
assigned; lead-compensation storage requires an explicit non-destructive layout.

### F12 — P2: status screens overstate or omit actual state

**CONFIRMED_SOURCE.**

The resource page always renders a `CRC_OK` label and a bare PACK_API label,
even though a separate numeric status can indicate failure
(`ui_product.c:1258–1293`). PC LINK shows constant READY/PC VIA COM/SEND PACK
instructions, rather than negotiated host presence or transfer progress. Those
labels should describe only actual validated state/capabilities.

Diagnostics do not expose numeric battery/NTC freshness, save phase/error,
calibration provenance/coverage, lead compensation or UART error counters.
`calibration_active_valid` means usable records are loaded, not that physical
accuracy is independently qualified. Keep that distinction visible.

Result details append range, channel, attempts, `AVG7/3` and frequency coverage
into a 32-byte buffer, ignoring append failures (`ui_product.c:593–608`). For
example `100K / RET 1X / 12 att. AVG7/3 F3/3` needs 36 bytes including NUL;
metadata is truncated. Split compact provenance into bounded rows and test both
languages and largest legal values. Do not lose coverage/compensation indicators.

### F13 — P1/P2: measurement capability still needs physical qualification

**IMPLEMENTED_BASIC_BENCH_ONLY / REQUIRES_BENCH_VALIDATION.**

Real nominal 10-nF and 470-uH results and earlier resistive engineering calibration
are recorded. There is no qualified full R/C/L accuracy envelope, sub-microhenry
resolution, full 10-mF endpoint, loss/phase floor, ADC calibration, thermal model
or polarized-capacitor operating envelope. A successful 4,700-uF AC reading is
not certification of polarized use or of a 10,000-uF upper limit.

The factory survey covers the **three implemented frequencies** 1 kHz, 10 kHz
and 10 Hz. It does not sweep continuously from 2 Hz to 10 kHz or every reference
at every frequency; reference selection is adaptive and bounded. Seven accepted
final samples use independent R/X trimming (two low and two high per coordinate,
average three), then derive C/L. This is not seven independent full surveys and
not a paired-observation vector estimator; preserve/document that distinction.

Independent ESR/Q/D, reference parasitics, interleaved power timing, connected
abort/contact timing and stack high-water still need bench evidence. Keep
`REQUIRES_BENCH_VALIDATION` statuses. Do not broaden classifications or publish
nominal values to make unresolved components look successful.

### F14 — P2: nominal sensor assumptions and defensive checks need visibility

**CONFIRMED_SOURCE / HARDENING.**

Battery/residual/VMID/NTC samples are actual ADC conversions. Voltage conversion
uses nominal 3.3-V VDDA. Generic NTC uses nominal parameters; this UID has an
owner-authorized experimental two-point fit from an unspecified 100-kohm NTC,
not an independently characterized sensor. Preserve that unit profile.

Add explicit finite/range/freshness tests at numeric policy boundaries, including
battery inputs; an externally marked-valid NaN must not compare as normal power.
This is defensive pure-logic hardening, not an observed NaN from physical ADC.
Do not require another manual voltage/resistor campaign just to correct these
application defects; schedule independent qualification separately.

## What is genuine, incomplete, illustrative or outside scope

| Area | Actual state | Required next action |
| --- | --- | --- |
| TIM1 excitation, TIM2 dual ADC/DMA | Real hardware acquisition | Preserve timing/ownership; qualify physically |
| Complex phasor and OSL processing | Active; PRODUCT calls calibration with ideal fallback disabled | Regression vectors and bench qualification |
| R/X, magnitude, phase | Computed from acquired calibrated phasors | Keep status/provenance/conditioning explicit |
| C/L and ESR(AC)/Q/D | Derived equivalent models, published only when admissible | Do not label as independent measurements |
| Nominal RREF in search | Selection estimate, not substituted published impedance | Retain calibrated result path |
| Three-frequency autorange / seven samples | Active in factory PRODUCT; legacy PRODUCT differs | F08 unification |
| SAFE relay/range FSM, interlocks, residual checks | Implemented; physical coverage incomplete | F07 plus fault/abort qualification |
| Display, splash, buttons, row cache | Hardware works; cache enabled in factory PRODUCT | Regress incremental navigation/edits |
| Brightness/language/sound settings in RAM | Implemented controller paths | F01–F04 persistence/timeout integration |
| Buzzer | Real driver; incomplete PRODUCT events | F05/F06 |
| Factory OSL A/B storage | Implemented; first-unit set 1/all 33 installed | Preserve exact records, fix shared contracts carefully |
| Quick user lead OPEN/SHORT | Absent | F10 / plan 30 |
| Settings A/B persistence | Serialization exists; actual start contract broken | F01–F03 |
| Assets / W25Q / versioned pack | Real provisioned resources | Accurate status and preservation tests |
| PRODUCT resource upload | Build-option dependent; not always compiled in | Advertise actual capabilities, recovery procedure |
| PRODUCT serial short OK | Implemented optional guarded UI command | F09 burst handling, no deferred activation |
| PRODUCT full numeric telemetry | Absent from current short status packet | Versioned genuine sensor/result messages |
| Diagnostics/self-test | Basic initialization/status, not exhaustive electrical self-test | Describe coverage honestly |
| WCRV supplementary correction | Optional, default OFF | Do not imply active without matching records/flag |
| DC pilot / large polarized capacitor mode | Experimental BRINGUP option, not qualified PRODUCT mode | Separate characterization/architecture work |
| LIVE/continuous UI measurement | Engine concepts exist; PRODUCT uses CLICK | Explicit future feature, not current behavior |
| Waveform/graph images | Illustrative source assets, not measured traces | Label examples; only render actual trace data as measured |
| K2 | Intentionally unpopulated / fixed link on this board | No firmware claim of independently switched hardware |
| Native USB / four-wire Kelvin | Unavailable in Rev.1 topology/pin allocation | Outside this completion plan |

No active production path publishing fabricated RLC readings was found. In
particular, `measurement-capacitor-ai.png` contains illustrative 1.002-uF artwork
but is absent from the seven-entry active resource manifest. RESULT renders
dynamic numeric text; `state_has_image()` uses READY artwork. A synthetic test
vector or a function named `measurement_dsp_config_ideal()` is not evidence of
synthetic samples being used in PRODUCT. The audit does not claim an exhaustive
formal proof that every possible build/path is free of defects.

## Validation and limits

Audit host runs: **50/50 factory CTest entries and 48/48 standard entries pass**.
The standalone audit probes confirm F01/F02/F03/F04 against unchanged source;
they intentionally assert the faulty behavior and are not future regression
tests expecting correct behavior. Convert them during the respective fixes.

The preceding delivery has nine strict embedded profile builds plus a serial-OFF
factory variant; their sizes and links are recorded in the evidence README.
They were **not rebuilt for this documentation-only audit**. Arm GCC ABI size
inspection was performed for memory planning. A separate 108-case complex
arithmetic prototype checks the proposed correction composition; it supplies no
physical accuracy or endpoint-observability evidence.

No firmware defect above is marked fixed by this change. No new resistor campaign
is requested. The final quick lead workflow will need actual lead-end OPEN/SHORT
and limited independent holdout checks, with no factory LOAD recapture unless a
separate discovered metrology defect makes it necessary.
