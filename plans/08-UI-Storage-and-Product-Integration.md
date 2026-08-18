# 08 — UI, Storage, and Product Integration

STATUS: NOT_STARTED

## Goal

Turn the validated measurement engine and peripheral foundations into a coherent Rev.1 product experience without allowing UI/storage activity to compromise acquisition determinism, calibration validity, or safety.

## Prerequisites

- Phase 03 display/Flash/input peripherals validated;
- Phase 04 safety/range services integrated;
- Phase 05 acquisition stable;
- Phase 06 measurement result API stable;
- Phase 07 autorange/confidence/classification/calibration interfaces available;
- Phase 07 exposes a deterministic calibration-set validity result.

## In scope

- cooperative UI/application state machines;
- fixed three-button navigation contract;
- startup/self-test/calibration-gate presentation;
- main measurement/result pages;
- measurement-progress/partial-result presentation;
- detailed impedance/result pages;
- useful derived graphs/visualizations;
- Calibration, Display, Sound, Language, Debug, and About menus;
- calibration wizard UI;
- debug console as an optional result page;
- final external resource/font-pack format/tooling;
- localization/resource IDs;
- settings persistence;
- power/idle/backlight policy;
- buzzer patterns/settings;
- integration state-machine hardening;
- memory/performance optimization.

## Out of scope

- changing metrology equations for UI convenience;
- requiring the user to preselect resistor/capacitor/inductor for normal operation;
- adding Rev.2 hardware features;
- declaring final metrology qualification before Phase 09.

## Task 1 — Freeze cooperative UI/application state model

Implement navigation independently from raw GPIO and keep UI behavior in small cooperative state machines.

The UI/application layer consumes debounced button events and application/measurement events. It must not busy-wait for timers, measurements, storage, animations, or user actions.

High-level product states should support at least:

```text
STARTUP
SELF_TEST
CALIBRATION_CHECK
CALIBRATION_REQUIRED
READY/RESULT
MEASURING
MENU
CALIBRATION_WIZARD
FAULT
```

The measurement FSM remains subordinate and owns its detailed acquisition/refinement transitions.

Safety warnings and mandatory boot gates override ordinary navigation.

## Task 2 — Freeze button/navigation contract

Normal measurement/result context:

```text
OK short     request a new measurement
UP           previous result page
DOWN         next result page
OK long      open main menu
```

Menu context:

```text
UP           previous item / increase value
DOWN         next item / decrease value
OK short     select / confirm
OK long      back
```

Requirements:

- ordinary operation does not require button combinations, double-clicks, or triple-clicks;
- long UP/DOWN may use repeat when useful for value editing;
- navigation never directly manipulates K1, ranges, excitation, or ADC configuration;
- if the backlight is fully off due to inactivity timeout, the first button press wakes the display and is consumed without executing its normal action;
- button semantics have host tests at state boundaries.

## Task 3 — Normal measurement flow

The standard product flow is:

```text
connect DUT
    ↓
short OK
    ↓
automatic probing / autorange / classification
    ↓
result
```

Do not add a normal pre-measurement prompt asking the user to choose R, C, or L.

The UI requests a measurement and displays the measurement engine's result/model interpretation. It does not choose a different impedance formula based on the displayed component label.

If classification is mixed/unknown/low confidence, the UI must represent that honestly while still exposing useful complex-impedance data when available.

## Task 4 — Primary result page

The first result page is intentionally minimal and should emphasize readability.

Display at least:

- detected dominant component/model when valid;
- large primary value;
- SI unit/prefix;
- battery/status indicator as appropriate;
- small footer metadata identifying the excitation amplitude and frequency most directly associated with the displayed primary value.

Representative footer:

```text
100 mV · 1 kHz
```

Do not imply that only the primary frequency was measured if additional frequencies were used for refinement/classification. The technical result page exposes that context.

Do not overload this page with range, channel, raw diagnostics, or long confidence explanations.

## Task 5 — Measurement-progress and partial-result presentation

While the measurement engine is performing multiple attempts/refinements, show useful progress rather than a static screen.

The central large value may update when Phase 07 publishes a validated partial result. A waiting/progress message remains visible until the final result is accepted.

Example concepts:

```text
Please wait...

987.3 nF

refining range...
```

then:

```text
Please wait...

1.001 µF

checking ESR...
```

then final result.

Potential progress reasons include:

```text
probing
refining range
checking another frequency
checking ESR/Q/D
improving SNR
verifying classification
finalizing result
```

Requirements:

- only completed valid partial attempts are shown;
- no raw individual ADC sample is presented as a measurement result;
- TFT/Flash rendering occurs between critical acquisition windows;
- no continuous TFT updates during ADC/DMA quiet windows;
- the measurement engine remains authoritative for partial/final status;
- localization does not change state-machine semantics.

## Task 6 — Electrical-details page

Display only derived values that are meaningful for the current model/result.

Potential fields:

```text
R
X
|Z|
phase
ESR
Q
D
series/winding resistance
series-equivalent C or L
confidence/status
```

For capacitor/inductor/resistor results, adapt the visible fields rather than forcing identical rows for every model.

Derived values that are invalid or not applicable must use an explicit unavailable state rather than displaying infinities or misleading numbers.

## Task 7 — Measurement-information page

Provide a technical page showing how the final result was obtained.

Potential fields:

```text
primary frequency
primary excitation amplitude
RREF
RET channel strategy
confidence class/reasons
attempt count
frequencies used for refinement/classification
rerange/retry reasons where useful
calibration record/set identifier
```

Lab/Debug builds may expose more detail than Release.

## Task 8 — Graph/result pages

UP/DOWN continues through zero or more useful graph pages after the textual result pages.

Candidate pages:

- impedance magnitude versus frequency;
- phase versus frequency;
- complex/vector R+jX visualization;
- equivalent electrical model drawn procedurally;
- comparison of 100 Hz / 1 kHz / 10 kHz results;
- inductor/coils phase behavior;
- other derived visualizations supported by real measurement data.

Requirements:

- graphs communicate measured or explicitly derived data, not decoration;
- only display graphs when enough valid data exists;
- procedural primitives are preferred for lines, axes, symbols, vectors, equivalent circuits, and curves;
- a calculated capacitor charge/discharge curve, if retained, is labeled/treated as derived rather than captured time-domain data.

## Task 9 — Dynamic result-page sequence

Build the normal page sequence dynamically from available result capabilities.

Conceptual order:

```text
1 Primary result
2 Electrical details
3 Measurement information
4..N Useful graph pages with valid data
N+1 Debug console, only when enabled
```

UP/DOWN browsing must remain deterministic when some optional graph pages are absent.

The selected page may be remembered as a normal user preference only if that behavior proves useful; it must never affect measurement math.

## Task 10 — Main menu

Long OK from normal READY/RESULT opens the Rev.1 main menu:

```text
Calibration
Display
Sound
Language
Debug
About
```

Do not add a normal R/C/L component-type selection menu.

Do not add manual measurement complexity to the ordinary product menu merely because internal debug APIs can support it. A future Lab/Debug manual-measurement tool may be introduced separately if justified.

## Task 11 — Mandatory calibration boot gate

On every boot, after basic safe initialization/self-test allows storage access, request Phase 07 calibration-set validation.

Flow:

```text
BOOT / SELF_TEST
    ↓
CALIBRATION_CHECK
    ↓
valid? ── yes ──> READY
   │
   no
   ↓
CALIBRATION_REQUIRED
    ↓
Calibration wizard
    ↓
new candidate persisted
    ↓
read-back / CRC / compatibility / completeness verification
    ↓
activate
    ↓
READY
```

Requirements:

- missing calibration forces the wizard;
- corrupt calibration forces the wizard;
- incompatible schema/model/hardware revision forces the wizard or an explicit non-recoverable service state if recalibration cannot solve it;
- incomplete required calibration forces the wizard;
- the user cannot long-OK/back out into normal measurement-ready operation while the mandatory gate is unresolved;
- safety/FAULT states still override the wizard;
- the gate decision itself is host-testable.

Calibration persistence uses W25Q. Do not refer to STM32F103C8T6 internal EEPROM.

## Task 12 — Calibration menu and wizard UI

After a valid calibration exists, the user reaches Calibration only through the normal menu unless a later validation failure reasserts the boot gate.

Provide at least:

```text
Calibration status/info
Full calibration wizard
OPEN
SHORT
LOAD
```

The exact presentation may combine OPEN/SHORT/LOAD into the full wizard while retaining service-level individual operations where useful.

Requirements:

- show required fixture/action clearly;
- refuse progress if charger/residual/safety conditions are invalid;
- show stability/progress;
- identify current frequency/range/amplitude condition;
- confirm persistence success;
- a manually started recalibration may be cancelled safely;
- cancellation/failure/power interruption must preserve the previous valid calibration whenever Phase 07 storage semantics permit;
- never erase the active valid calibration at wizard start.

## Task 13 — Display menu

Implement persistent settings for at least:

```text
Brightness
Backlight timeout
```

Backlight timeout may expose practical choices such as always-on and timed values.

Turning off the backlight does not automatically imply powering down the instrument.

The first button press after full backlight-off wakes the display and is consumed.

Brightness/backlight behavior must respect quiet-mode noise policy during active measurement.

## Task 14 — Sound menu

Implement a persistent ordinary sound enable/disable setting.

Examples of ordinary feedback that may obey the setting:

- button confirmation;
- successful measurement;
- invalid action;
- calibration completion.

Create non-blocking buzzer patterns.

The policy for mandatory safety-critical audible alerts when Sound=Off must be explicitly decided and documented before release. Do not assume either behavior silently.

Buzzer activity is suppressed during critical metrology acquisition.

## Task 15 — Language menu and localization

Initial planned choices:

```text
Português
English
```

Requirements:

- UI code uses stable text/resource IDs rather than scattered literal strings;
- localized resources may live in the W25Q resource pack to reduce MCU internal-Flash pressure;
- fundamental boot/safety/Flash-failure messages have a minimal internal fallback path;
- selecting a language must not require a firmware rebuild;
- language persistence is versioned/CRC-protected like other settings.

Host-test formatting/resource lookup boundaries where practical.

## Task 16 — Debug menu

Provide at least:

```text
Console enabled
Log level
```

Additional Lab/Debug options may include:

```text
measurement dump
live/system diagnostics
storage/peripheral status
```

Release exposure may be reduced if product policy later requires it, but architecture should not require recompiling screen logic simply to enable bounded diagnostics.

## Task 17 — Event/debug console page

Implement bounded ring-buffer logging for TFT display and UART output.

When `Console enabled` is true, the on-screen console becomes the last additional page in the normal UP/DOWN result-page sequence.

Representative on-screen content:

```text
DEBUG CONSOLE
128.341 MEAS start
128.342 RREF=1k
128.344 FREQ=1kHz
128.392 ADC done
128.398 Z=0.83-j158.9
128.399 class=CAP
128.401 conf=0.98
```

Requirements:

- fixed memory usage;
- no heap;
- bounded line length/count;
- timestamps from monotonic time;
- filtering by log level;
- quiet-mode suppression of high-volume output;
- no verbose logging from critical ISR paths;
- UART remains the higher-volume/full diagnostic stream;
- enabling the TFT console does not force persistent storage of every message.

Measurement-dump UART output may include attempt conditions, raw statistics, phasors, complex Z, classification evidence, calibration IDs/coefficients, confidence reasons, and retry/refinement reasons.

## Task 18 — About

Display useful traceability information:

```text
WTK.RLCMeter
Developer/project identification
firmware semantic version
source/build commit identifier
hardware revision compatibility
calibration schema/model version
resource-pack version
```

Do not hard-code values that already exist in build metadata when they can be consumed from the version API.

## Task 19 — Rendering engine

Implement:

- RGB565 procedural primitives;
- text/glyph rendering;
- dirty-region or explicit-region updates;
- no full-screen framebuffer;
- clipping/bounds checks;
- predictable memory usage;
- external resource/font streaming through a fixed scratch buffer;
- custom large numeric typography using W25Q-resident glyph data.

Profile large screen transitions and keep them outside quiet mode/acquisition.

## Task 20 — Final external resource/font pack

Freeze a resource-pack format with:

```text
header/version
resource count
entries
blob payloads
CRC/integrity
```

Entries should support stable ID, offset, length, format/flags, dimensions where relevant, font/glyph metrics where relevant, and integrity information.

Host tooling must:

- convert TTF/OTF authoring fonts into compact rasterized MCU font packs;
- support multiple font sizes including large numeric glyphs;
- include required measurement glyphs such as Ω, µ, °, ±;
- convert icons/optional bitmaps;
- generate the pack deterministically;
- validate bounds/CRC;
- optionally generate a C header of stable resource IDs.

The STM32 firmware must not parse TTF/OTF or embed FreeType.

Installed resource size must not produce proportional SRAM use. Resource streaming uses a fixed small scratch buffer, ideally hundreds of bytes unless profiling proves a larger buffer is necessary.

The firmware degrades to its minimal internal emergency font/resources if the external pack is absent/corrupt.

## Task 21 — Settings schema

Define versioned CRC-protected persistent settings including at least:

```text
backlight brightness
backlight timeout
sound enabled
language
debug console enabled
log level
```

Optional remembered result-page preference may be added only if useful.

Safety interlocks and mandatory calibration validity are not user-configurable.

Compiled safe/default settings must exist even if the external settings record is invalid.

## Task 22 — Application integration

Harden full orchestration around subordinate state machines.

Representative high-level flow:

```text
BOOT
SELF_TEST
CALIBRATION_CHECK
  ├─ invalid -> CALIBRATION_REQUIRED -> CALIBRATION_WIZARD
  └─ valid   -> READY
READY/RESULT
  ├─ short OK -> MEASURING
  ├─ UP/DOWN  -> result pages
  └─ long OK  -> MENU
MEASURING
  -> partial result / UI update points
  -> final RESULT
FAULT overrides ordinary states
```

The subordinate measurement flow retains the safe sequence:

```text
SAFE_CHECK
PREPARE_RANGE
PRE_EXCITATION
K1_MEASURE
SETTLING
ACQUIRE
K1_SAFE
PARTIAL_PROCESS
UI_UPDATE_POINT
RETRY/RERANGE/REFINE or FINAL_PROCESS
DONE
```

Add host tests for user actions during transitions, long-OK behavior, cancellation, faults, charger insertion, low battery, missing/corrupt calibration, missing Flash/resource pack, TFT failure, backlight wake consumption, and console-page enable/disable.

## Task 23 — Memory/size review

Use link map/size reports to record:

- MCU Flash usage;
- RAM usage;
- acquisition buffers;
- DSP working buffers;
- external-resource scratch buffer;
- UI state/cache;
- debug-console ring buffer;
- stack margin strategy if measurable;
- W25Q space used by fonts/resources/calibration/settings.

Do not allow UI feature growth to consume metrology buffer margin silently.

Large fonts/icons/localization belong in W25Q rather than internal MCU `.rodata` unless a specific fallback resource is intentionally internal.

## Task 24 — Responsiveness and quiet-mode review

Measure or instrument worst-case `*_step()` latency.

Ensure:

- button handling remains responsive outside acquisition;
- large rendering/storage tasks are chunked;
- watchdog service remains reliable;
- acquisition start cannot be delayed unpredictably by a UI operation once scheduled;
- TFT/Flash/buzzer activity is suppressed during critical acquisition windows;
- partial-result rendering occurs between attempts without contaminating the next acquisition;
- backlight PWM behavior follows the validated quiet-mode policy.

## Automated acceptance criteria

- host tests for navigation/button semantics/state transitions pass;
- normal measurement requires no R/C/L user selection;
- calibration-invalid boot cannot reach READY/measure states;
- manually cancelled/failed recalibration preserves prior valid calibration according to Phase 07 storage guarantees;
- progress UI only consumes validated partial results and has no acquisition-critical rendering path;
- dynamic result-page sequence behaves correctly with optional graphs/console absent or present;
- first button after backlight-off is consumed as wake-only;
- resource/font pack generator produces deterministic validated output;
- localized text lookup is stable and testable where pure;
- settings corruption/recovery tests pass;
- no full framebuffer exists;
- no screen code drives hardware safety GPIOs;
- no large font pack is linked into MCU internal Flash accidentally;
- embedded builds fit within documented Flash/RAM budgets;
- previous DSP/safety/calibration tests remain green.

## Bench acceptance criteria

1. boot with valid calibration and enter READY directly after normal self-test;
2. boot with missing/corrupt/incomplete calibration and verify forced Calibration flow with no path to normal measure;
3. complete calibration, power-cycle, and verify READY entry;
4. start manual recalibration, cancel/interfere safely, and verify previous valid calibration survives where applicable;
5. short OK starts measurement;
6. observe partial/refined values and waiting/progress states without TFT activity during critical acquisition windows;
7. UP/DOWN navigates primary/details/technical/available graph pages;
8. verify primary footer shows the primary amplitude/frequency and technical page exposes additional refinement frequencies;
9. long OK opens menu; long OK backs out from normal submenus;
10. navigate Calibration/Display/Sound/Language/Debug/About;
11. verify backlight timeout and wake-only first button behavior;
12. verify sound setting for ordinary UI feedback;
13. switch Português/English without rebuilding firmware;
14. enable debug console and verify it appears as an additional normal result page;
15. compare TFT recent-event console with the higher-volume UART log;
16. boot with missing/corrupt external resource pack and verify emergency fallback diagnostics;
17. render custom W25Q-resident fonts/large numeric glyphs with bounded SRAM;
18. test charger insertion/residual faults during UI operation;
19. review diagnostics against DMM/scope/UART values;
20. verify repeated measurement/UI cycles remain responsive and watchdog-safe.

## Handoff

Report:

- final application/UI state-machine structure;
- exact button/navigation behavior;
- final result-page sequence and optional-page rules;
- measurement-progress/partial-result UX;
- calibration boot-gate behavior;
- menu tree;
- localization/resource strategy;
- resource/font format/tool version;
- settings schema version;
- debug-console RAM footprint and UART/TFT behavior;
- measured MCU Flash/RAM usage;
- W25Q resource/storage usage;
- worst observed UI/update latencies;
- fallback behavior;
- known UX limitations;
- readiness for formal Phase 09 qualification.
