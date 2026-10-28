# Firmware Architecture

## October24 integration baseline

The shared acquisition/DSP/30ms relay fixes apply to all metrology profiles. All TFT profiles use the working common DCS initialization, portrait180 and slower TFT clock. BRINGUP_CAL has only the shared optional boot splash, then returns to its binary serial service; PRODUCT retains real safety and valid-calibration gates.

See [current evidence, input limits and release status](review/bringup-2026-10-24/README.md).

## Principles

The firmware is organized as instrument firmware, not as a microcontroller demo:

- SAFE is the default state;
- acquisition is deterministic;
- UI and storage do not block measurement;
- DSP is isolated from device-specific peripherals;
- failure of any required precondition returns K1 to SAFE;
- settings and calibration are versioned and CRC-protected;
- command-line CMake builds are canonical;
- Visual Studio Code is the primary supported editor workflow;
- the implementation language is C17;
- Arduino/INO and C++ are outside the current baseline.

## Main state machine

```text
BOOT
  |
  v
SELF_TEST
  |
  v
SAFE_CHECK <-----------------------------+
  |                                      |
  +-- residual/charger/fault --> WAIT ---+
  |
  v
READY
  |
  v
PREPARE_RANGE
  |
  v
PRE_EXCITATION
  |
  v
K1_MEASURE
  |
  v
SETTLING
  |
  v
ACQUIRE
  |
  v
K1_SAFE
  |
  v
PROCESS
  |
  +--> RETRY / RERANGE
  |
  v
RESULT
```

The DUT should remain connected to the measurement AFE only for the time required to settle and acquire.

## Boot sequence

1. clock and watchdog;
2. safe GPIO defaults;
3. disable JTAG while keeping SWD to free PA15/PB3/PB4;
4. diagnostic UART;
5. SPI bus;
6. W25Q external Flash;
7. ILI9341 display;
8. ADC/DMA/timers;
9. persisted settings/calibration validation;
10. self-test;
11. `SAFE_CHECK`.

A display or external-Flash failure must not bypass safety policy.

PRODUCT application scheduling is separate from hardware permission. An actual
disabled idle range can schedule preparation; it is not relabeled READY for
`hw_safety_evaluate` or the relay. Expected residual refresh and an owned
transaction's ADC/range transitions do not self-cancel that hardware FSM. Hard
charger/residual/supply/fault blockers remain. At PERMIT_ISSUE only, an attempt
can wait cooperatively for real refreshed evidence for at most 1,000 ms from
its start, then falls through to the unchanged strict permit check. Normal TFT
steps yield while the polled auxiliary group is active or residual safety is
requalifying; fresh actual fault/unsafe evidence still allows warning rendering.
See [the physical READY checkpoint](../Firmware/releases/2026-10-25/calibrated-product-ready/README.md).

## BSP

`bsp` encapsulates:

- clock tree;
- GPIO;
- SPI2;
- USART1;
- ADC1/ADC2;
- DMA;
- TIM1/TIM2/TIM3/TIM4;
- watchdog;
- monotonic time;
- reset reason.

Higher layers must not write STM32 registers or arbitrary GPIOs directly.

## Drivers

### ILI9341

Minimum API responsibilities:

```text
init
reset
read_id/status
set_rotation
set_window
fill
write_pixels_rgb565
draw_bitmap primitive
draw_glyph primitive
```

There is no 240×320×16-bit framebuffer in MCU RAM.

### W25Q

Generic API responsibilities:

```text
init / read_jedec_id
read
fast_read
write_enable
page_program
sector_erase
read_status
wait_ready
```

The driver should recognize compatible 3.3 V W25Q16/32/64/128 densities rather than hard-code W25Q64 behavior across the firmware.

### Buttons

Debounce produces events:

```text
PRESS
RELEASE
LONG_PRESS
REPEAT
```

The button driver does not own UI navigation policy.

## Hardware services

Higher layers use semantic services rather than GPIOs:

```text
relay_set_safe()
relay_set_measure()
range_disable()
range_select(range)
excitation_configure(freq, amplitude)
excitation_stop()
charger_connected()
battery_read()
temperature_read()
buzzer_play(pattern)
backlight_set(percent)
```

## Acquisition

The acquisition module delivers synchronized raw sample blocks. It does not compute impedance.
The raw block storage is supplied by the application-owned I/O workspace so that the
same 3072-byte arena can be reused by calibration-store serialization when no capture is
active.

DSP consumes buffers plus metadata such as:

```text
frequency
sample_rate
range
excitation amplitude
channel timing/skew
RET channel
calibration key
```

and returns complex phasors, statistics, and quality information.

## Quiet mode

During critical acquisition windows:

- buzzer is always off;
- large TFT transfers are suspended;
- external Flash is not accessed unless explicitly required;
- backlight PWM remains stable or may be frozen to a qualified condition if bench tests show coupling;
- high-volume UART logging is suspended.

Results are preferably rendered after K1 returns to SAFE.

## TIM3 and buzzer

PB0 and PB1 map to TIM3 channels, therefore two hardware-PWM outputs on those pins would share the same timer base frequency.

Recommended baseline:

- TIM3_CH3: continuous backlight PWM on PB0;
- TIM4: buzzer timebase;
- PB1: GPIO toggled according to the requested tone.

This keeps backlight and audible-tone frequencies independent. The piezo remains silent during acquisition.

## Persistence

External Flash is logically divided into:

- asset pack;
- calibration records;
- settings;
- optional diagnostic/event data.

No filesystem is planned initially.

Persistent records contain fields equivalent to:

```text
magic
schema_version
hardware_revision
record_type
sequence
payload_length
crc32
payload
```

Settings/calibration must use redundant slots or a small journal strategy to survive interrupted writes.

Asynchronous services have one scheduler owner. In PRODUCT, `app_shell.c` advances the
calibration service/store FSM exactly once per superloop; higher-level flows such as the
product calibration wizard start operations and observe state rather than stepping the
same W25Q transaction internally. Fatal application presentation must not stop stepping
the runtime that is returning hardware to SAFE.

## Diagnostics

A compact ring buffer may be displayed on TFT and streamed through UART.

Log levels:

```text
ERROR
WARN
INFO
DEBUG
TRACE   # bring-up profile only
```

The firmware should remain useful for bring-up without relying on breakpoints.

## Firmware profiles and host tooling

Optimization build type and firmware feature profile are separate concerns:

```text
CMAKE_BUILD_TYPE      = Debug | Release | MinSizeRel
WTK_FIRMWARE_PROFILE  = PRODUCT | BRINGUP
```

`stm32-debug` and `stm32-release` build the PRODUCT profile. `stm32-bringup` builds the
size-optimized BRINGUP profile for physical-board UART diagnostics. The old
`CMAKE_BUILD_TYPE=Lab` feature-selection model is obsolete.

The bring-up console exists only for operations that need the STM32 board: safe status,
charger/sensor/range checks, W25Q/TFT/buzzer/backlight checks, fixed-condition
metrology capture/measure, and OSL calibration acquisition/solve/commit. Rich
formatting, calibration-record inspection, synthetic policy analysis, raw-capture
replay, and coefficient comparison belong in host tools and tests where they are not
limited by the 64 KiB MCU Flash budget.

STM32 Release and Bringup builds report Flash/RAM budgets after linking. PRODUCT keeps
a temporary 63 KiB Flash hard gate, with a 48 KiB soft target and the physical silicon
limit remaining 64 KiB. The gate was raised from 60 KiB for the supplementary curve
runtime; the measured margin to physical Flash is about 1.5 KiB and must be recovered
before adding further PRODUCT features. Rich visuals, fonts, and catalog data still
live in W25Q resources rather than internal Flash. BRINGUP uses the physical 64 KiB
Flash limit so the bench image can link the automatic measurement engine and diagnostic
output needed for first-board measurements. RAM reports distinguish static
`.data/.bss/.noinit` from the linker-reserved stack/heap floor. PRODUCT builds use a
16 KiB preferred accounted-RAM target and a 17 KiB hard gate; BRINGUP uses an 18 KiB
hard gate. Accounted RAM includes the linker-reserved stack/heap floor.

Product Debug is intentionally size-first (`-Os` plus debug symbols) with compact
diagnostic logging because the full product profile must fit the guaranteed C8 Flash.
Product Release also enables GCC LTO
from its preset to recover dead-code headroom before the remaining Phase 08 resource/UI
features are added. Phase 08 Stage 3A.3 removes PRODUCT calibration-campaign
orchestration, compacts fixed Rev.1 calibration-domain validation, and brings PRODUCT
Release below the 55296 B Stage 3B handoff target. Phase 08 Stage 3B.1 then adds the
external A1 font runtime. Phase 08 product integration may now use the 60 KiB PRODUCT
gate while keeping normal text, font, and rich image resources external.

Product settings use a transactional two-slot W25Q record. Schema v3 adds the
menu-selected calibration LOAD preset; schema v2 records remain readable and default
that field to `NOMINAL`.

## Build and editor policy

The repository must eventually support:

```bash
cmake --preset <preset>
cmake --build --preset <preset>
ctest --preset <preset>
```

or equivalent documented CMake commands.

Visual Studio Code integrates through the checked-in workspace and CMake Tools, but no VS Code-specific metadata may become required for non-editor builds.

Detailed module contracts are documented in [`13-Detailed-Firmware-Design.md`](13-Detailed-Firmware-Design.md), while the execution sequence is defined in [`../plans/`](../plans/).


## PC-provisioned session and menu ownership (2026-10-26)

The earlier per-capture main-state diagram remains the fixed-condition/legacy
path. PC-provisioned PRODUCT additionally supports:

```text
fresh SAFE checks -> consume initial permit -> K1 MEASURE / 30-ms guard
  -> guarded range + excitation settling -> acquire -> stop DMA -> neutral
  -> supervised HOLD -> bounded DSP/policy -> another guarded range/capture
  -> FINISHING -> K1 SAFE / release guard -> ADC restore -> final result
```

hw_metrology_measure owns the continuous session, six-capture/500-ms deadline
and original battery-evidence expiry. No application/UI code writes relay or
range GPIOs. App FINISHING defers final-result availability until cleanup; late
cleanup faults/cancellation invalidate provisional policy success. Quiet and the
same exclusive metrology workspace lease span held captures. A lost lease cannot
be silently reacquired; it aborts the session. The consumed initial permit is
retained for traceability, not revalidated as a new range permit. Fixed captures,
BRINGUP, BRINGUP_CAL and full embedded-wizard PRODUCT keep their former ownership.

WTK_ENABLE_MEASURE_SESSION_HOLD and WTK_ENABLE_UI_ROW_CACHE are generated from
the PC-provisioned PRODUCT profile. The legacy full-wizard PRODUCT keeps its
existing footprint gate and bounded renderer; its common language/error recovery
and evidence-admission fixes still apply. Full BRINGUP now omits unsolicited
verbose boot/button logs (INFO default), preserving commands and safety logs to
make room for the common measurement policy within guaranteed C8 Flash.

## Post-search statistical capture role

WTK_ENABLE_MEASUREMENT_TRIMMED is enabled only with PC-provisioned PRODUCT.
The app adapter explicitly opts into collecting seven valid impedances after
primary selection. The hardware owner separately bounds search captures (six)
and statistical continuations (eight extra), for at most 14 captures under the
same consumed initial permit. The first statistical continuation may return
from the secondary frequency to the primary; subsequent requests are locked
to that range/frequency/amplitude, and search cannot resume. Same-condition
statistical repeats skip redundant range addressing while retaining ADC and
excitation settling, neutral inter-window ownership and all dynamic aborts.
The original 500-ms connected limit and 1,000-ms battery-evidence expiry are
unchanged and are never refreshed by these extra captures.

The fixed-size pure estimator stores seven complex points and coordinate masks;
there are no seven ADC buffers, heap or new persistent records. App FINISHING
still withholds publication until complete SAFE cleanup. An incomplete or late
faulted estimate is rejected. Calibration coefficients, assets and NTC profile
are unchanged. Legacy/full-wizard PRODUCT and fixed diagnostics are not enlarged
by enabling this feature.

## Three-frequency factory orchestration

WTK_ENABLE_FREQUENCY_SURVEY is generated only for PC-provisioned PRODUCT and
its adapter opts into measurement_auto_start_t.frequency_survey. Preserve the
legacy six-attempt policy for other callers/profiles. Factory history capacity
is twelve (six extra 156-byte raw attempt records = 936 B); the active survey
bounds each frequency to four conditions. This deliberate traceability allocation
can exceed the preferred 16-KiB accounted-RAM budget but must stay below the
existing 17,408-B hard budget including the reserved 2-KiB stack. No extra ADC
buffer or heap is introduced.

App DRAINING is a nonpublishing cleanup/reauthorization handoff, distinct from
terminal FINISHING. Cancel, late cleanup error and the 60-s overall deadline
revoke the transaction. Fixed 10-Hz captures retain their existing owner and
SAFE teardown. Statistical collection after SAFE is allowed to acquire a new
ordinary permit, then repeat at one exact primary condition. Acquired DSP
rejection can inform another condition; missing/invalid processing output and
hardware faults still fail closed. BSP, drivers, timer/ADC profiles, calibration
model/storage formats and unit NTC coefficients are unchanged.

## Continuous factory power handoff (plan 27)

The factory adapter now opts into WTK_ENABLE_CONTINUOUS_SURVEY and sets
continuous_survey on held requests. It bypasses plan 26's DRAINING segmentation
while preserving terminal FINISHING and all seven-point admission/trim rules.
The hardware owner admits twelve search captures plus eight statistical extras
under one consumed initial permit, with a 30-s connected bound. Legacy opt-out
requests retain their previous limits and SAFE transactions.

POWER_RESTORE, POWER_WAIT and POWER_ACQUIRE lend the stopped metrology ADC to
hw_aux_sensors_power_begin/status while normal auxiliaries stay paused. The
existing main loop advances four VMID/four BAT conversions, excluding OV/NTC;
residual evidence remains UNKNOWN. ADC restoration/reacquisition uses existing
BSP paths and the identical coherent profile, without changing excitation,
K1, ranges, quiet or workspace ownership. Sampling is scheduled about every
250 ms outside DMA, bounded by 100 ms including both handoffs. Validated actual
timestamps renew the existing 1,000-ms battery deadline. Settling resumes at its
original deadline. Failure always enters normal SAFE cleanup.

The plan-27 factory image accounts for 17,104 B RAM including its 2,052-B
stack reservation; the subsequent serial-control image accounts for 17,116 B
including a 2,048-B stack reservation. The existing 17,408-B hard gate remains enforced; inspect
the release matrix for final sizes. No additional ADC buffer, heap, calibration
record or persistent-format change is introduced.

## Serial UI adapter

app_serial_ui validates bounded PRODUCT control frames and request identity,
then delegates a single short OK to app_product_handle_button_event. It has one
32-bit boot-local request counter, no queue or peripheral ownership. The board
adapter supplies actual SAFE/power/ownership/physical-button admission. Existing
PLC1 decoding/resource frame boundaries remain authoritative; no byte-sniffing
multiplexer or alternate measurement engine is introduced. The PC client never
retries activation after an uncertain reply. See plan 28 for the profile and
validation contract.


## Volatile waveform ownership (plan 34)

The measurement_waveform module prepares physical ADC pairs without UI/storage
dependencies. app_waveform_store owns candidate/published slots in the cooperative
foreground. The session stages only repeats admitted by its existing statistical
predicate, before raw ownership is released. It publishes only at successful
terminal SAFE, binding session, condition, channel and factory/lead sequences.
A missing snapshot never changes a valid numerical result or authorizes hardware.

The product view carries only a const reference and generation. The renderer
abandons a previous generation before reusing a published slot. The dedicated
ui_waveform column operation has bounded stack scratch and no ISR/shared-DMA
ownership. Its 12-byte context plus references and 2176-byte store fit the approved
2304-byte incremental static budget; linker maps are authoritative for padding.

The explicit first-unit 128-KiB product uses a preferred accounted RAM ceiling of
18 KiB and a hard ceiling of 19 KiB, including >=2 KiB stack, leaving >=1 KiB
outside the accounted allocation. Build and package inspection share
firmware_size.ram_limits_for_budget; generic product/bringup gates stay at
17/18 KiB. WTK_STACK_WATERMARK=ON is a diagnostic reset-time paint of the top
2 KiB; surviving prefix evidence measures only exercised paths, not all possible
stack depths. No production heap or stack reduction is introduced.
