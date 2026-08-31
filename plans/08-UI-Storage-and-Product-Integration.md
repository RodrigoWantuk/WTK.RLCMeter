# 08 — UI, Storage, and Product Integration

STATUS: IN_PROGRESS

## Stage 1 — Product controller, calibration gate, click measurement, and minimal TFT UI

STATUS: IMPLEMENTED_REQUIRES_BENCH_VALIDATION

Implemented software boundary:

- PRODUCT profile has a cooperative product application controller with explicit states:
  `STARTUP`, `SELF_TEST`, `CALIBRATION_CHECK`, `CALIBRATION_REQUIRED`, `READY`,
  `MEASURING`, `RESULT`, `SAFETY_BLOCKED`, and `FAULT`.
- The controller consumes debounced button events and never drives K1, K2, range GPIO,
  excitation, ADC, storage, or display hardware directly.
- Calibration is a mandatory product gate. Only `APP_CAL_SERVICE_ACTIVE_VALID` permits
  `READY`; missing, unavailable, incompatible, corrupt, or incomplete calibration remains
  in `CALIBRATION_REQUIRED`/storage-error presentation.
- PRODUCT click measurement uses the existing Phase 05 fixed-condition measurement
  transaction through `app_measurement_session_t`. Each attempt traverses the Phase 05
  safety/permit/range/excitation/K1/ADC/DMA teardown contract independently.
- PRODUCT DSP processing uses `measurement_cal_process_block()` with the active
  calibration set and `allow_ideal_fallback=false`; the UI cannot manufacture an ideal
  calibration fallback.
- OK press arms, OK release emits one short-click measurement request, OK long emits a
  menu-not-implemented no-op and suppresses the short click.
- UP/DOWN currently browse the bounded primary/details result pages. Full menus,
  settings, calibration wizard screens, localization, resource pack, and graph pages
  remain later Phase 08 work.
- A minimal no-framebuffer TFT renderer consumes a UI view-model snapshot, defers while
  quiet mode is active, redraws only on generation changes, and uses the built-in 5x7
  fallback font.
- The fallback font now covers A-Z, digits, and minimal punctuation; lowercase unit text
  maps to uppercase glyphs.
- SI/unit formatting is a pure host-tested helper and does not use target float `printf`.
- PRODUCT and BRINGUP profile composition is checked from the linked ELF with
  `tools/check_profile_symbols.py`; PRODUCT must not link bringup-console symbols and
  BRINGUP must not link product UI/controller symbols.
- STM32 Release and Bringup size gates are mandatory for budgeted profiles; missing
  Python is now a configuration error for those gates.

## Stage 1.1 — SRAM reclamation and cooperative renderer hardening

STATUS: IMPLEMENTED_REQUIRES_BENCH_VALIDATION

Implemented software boundary:

- The hidden BSP-owned Phase 05 raw ADC buffer was removed. Metrology raw capture now
  receives its 3072-byte buffer from the application layer.
- `app_io_workspace_t` provides one explicitly owned 3072-byte scratch arena shared
  between metrology raw capture and calibration-store serialization/verification. It
  is never borrowed by both at once.
- Calibration load/commit paths acquire the workspace as `CALIBRATION_STORE`. A commit
  keeps ownership through asynchronous erase/program/verify and releases it only after
  the terminal store state has been acknowledged.
- Metrology session paths acquire the workspace as `METROLOGY` before starting Phase 05
  capture and release it after the raw block is acknowledged.
- Product and Bringup profiles both use the same shared workspace. Bringup diagnostics
  no longer own independent raw or calibration-frame storage.
- Product UI state stores a compact `ui_product_measurement_t` snapshot instead of a
  full `measurement_session_result_t`. The product application reuses the Phase 07
  policy result as the authoritative partial/final measurement.
- The fallback TFT renderer now has a cooperative text operation that draws at most one
  scaled character per step. `ui_product_step()` coalesces pending view generations,
  pauses during quiet mode, avoids full-screen clear on same-page updates, and only
  starts text drawing after the current clear/fill primitive has completed.
- `tools/firmware_size.py` now distinguishes PRODUCT and BRINGUP RAM gates. PRODUCT has
  a 16 KiB preferred accounted-RAM target and a 17 KiB hard gate; BRINGUP has an 18 KiB
  hard gate. The legacy `release` budget name remains an alias for PRODUCT.

Measured software evidence:

- PRODUCT Release accounted RAM was reduced from 19924 B to 16348 B, with the 2048 B
  reserved stack/heap floor included.
- PRODUCT Debug accounted RAM was reduced from 19932 B to 16336 B.
- BRINGUP accounted RAM was reduced from 18164 B to 15116 B.
- The largest remaining RAM objects are the 6048-byte calibration service, the
  3076-byte shared I/O workspace, and the 1560-byte product controller.
- The cooperative renderer keeps the existing small SPI pixel chunk buffer rather than
  increasing display scratch RAM before physical timing data is available.

Remaining Phase 08 work:

- complete menu tree and normal calibration wizard UI;
- persistent settings schema;
- external resource/font pack and localization resources;
- dynamic graph/result pages beyond the primary/details pair;
- display/sound/debug/about menus;
- TFT debug console page;
- physical UI timing and quiet-mode validation.

## Stage 3A — external resource pack core, text catalogs, localization runtime, and flash-access policy

STATUS: IMPLEMENTED_REQUIRES_BENCH_VALIDATION

Implemented software boundary:

- Resource Pack v2 is defined as an explicit little-endian wire format with header,
  entry table, payload CRCs, entry-table CRC, and header CRC. Runtime code decodes
  fields by width and offset rather than treating compiler-dependent C structs as
  storage records.
- Resource Pack v2 currently supports UTF-8 text-table resources. Each language is one
  resource with stable resource IDs and a sorted stable text-ID index.
- The deterministic host builder emits English and Portuguese (Brazil) text catalogs
  from JSON sources and rejects incomplete required text catalogs.
- PRODUCT settings schema is now version 2 and stores the selected language as a stable
  language ID. Older settings records are intentionally treated as incompatible and
  defaulted.
- PRODUCT UI includes a Language menu with English, Portuguese (Brazil), and Back.
  Normal product menu/status labels resolve through the external text provider. The
  internal fallback retains only emergency/minimal diagnostic text.
- UTF-8 decoding is bounded and host-tested. The fallback renderer advances by
  codepoint and renders unsupported/invalid codepoints as `?`.
- Resource access obeys the shared W25Q policy: reads are deferred during acquisition
  quiet windows and while calibration/settings mutation owns the Flash device.
- Missing, corrupt, or incompatible Resource Pack v2 data enters the emergency
  `RESOURCE_ERROR` product screen. This blocks normal PRODUCT operation without
  latching a safety fault or weakening K1/range safety behavior.
- Product Release verbose boot/product diagnostics are compiled out at the default
  diagnostic level to reclaim internal Flash for the Stage 3A resource-admission path.

Host tooling:

- `Firmware/tools/build_resource_pack.py` builds a deterministic binary pack from
  `Firmware/assets/resource_manifest.json`.
- `Firmware/tools/inspect_resource_pack.py` validates and prints the pack header and
  entry table.
- `Firmware/tools/resource_pack_format.py` holds the independent Python wire-format
  encoder/decoder used by tooling tests.

Measured software evidence:

- Generated Resource Pack v2 size: 2212 B.
- Host Debug CTest: 32/32 passed.
- Host Release CTest: 32/32 passed.
- Python tooling unittest: 42 passed.
- Wokwi lint-only with CLI `0.26.1 (9d71b975b7eb)`: custom-chip compile passed and
  `wokwi-cli lint` reported no issues.
- PRODUCT Debug build: 64504 B Flash, 16884 B accounted RAM.
- PRODUCT Release build: 57216 B Flash, 16872 B accounted RAM.
- BRINGUP build: 54212 B Flash, 15116 B accounted RAM.

Budget notes:

- PRODUCT Release is below the hard 57344 B Stage 3A gate but above the preferred
  56320 B target and the older 49152 B soft target.
- PRODUCT accounted RAM remains below the hard 17408 B Stage 3A gate.

Remaining Stage 3B/product work:

- external font/glyph/icon/splash resources;
- physical W25Q resource-pack programming and boot validation;
- product graph/result pages and polished calibration UI;
- TFT debug console page;
- final installer/resource flashing flow;
- physical SPI/resource-read timing and quiet-mode validation.

## Stage 2A — product calibration wizard and active-calibration gate

STATUS: IMPLEMENTED_REQUIRES_BENCH_VALIDATION

Implemented software boundary:

- Product calibration validity is now split between active calibration validity and
  calibration-service operational status. A dirty manual candidate, active workflow, or
  store-busy status no longer invalidates a previously active verified calibration.
- Missing active calibration remains a mandatory gate: short OK from
  `CALIBRATION_REQUIRED` enters the mandatory calibration wizard, while long OK cannot
  bypass the gate into normal measurement.
- Storage-unavailable calibration status blocks wizard acquisition start and remains a
  storage-error calibration-required presentation.
- The product application now has explicit `MENU`, `CALIBRATION_STATUS`, and
  `CALIBRATION_WIZARD` states. The implemented Stage 2A product menu is intentionally
  narrow: `Calibration` and `Back` only. Display/Sound/Language/Debug/About remain
  later Phase 08 work.
- Product runtime storage is a union of measurement session and calibration wizard
  contexts because automatic measurement and product calibration are mutually exclusive.
  Both reuse the existing Phase 05 capture path and the single 3072-byte shared
  metrology/storage workspace.
- `app_calibration_wizard_t` owns the product full-calibration campaign. It prompts once
  per fixture per range (`OPEN`, `SHORT`, `LOAD`), then auto-advances through all
  calibratable conditions for that fixture/range.
- The wizard enumerates calibratable keys dynamically through
  `measurement_condition_calibratable()`. The current Rev.1 domain is exactly 33
  conditions: all six ranges, three frequencies, and two amplitudes except the forbidden
  `10 Ohm + 500 mVrms` combination.
- The wizard batches one range at a time and caches only compact OPEN and SHORT
  standards for the current range, up to six per range. It does not retain all raw
  evidence for all 33 conditions.
- LOAD fixture values are supplied through a fixture profile callback. The default
  Stage 2A profile uses nominal pure-real RREF values and remains
  `REQUIRES_BENCH_VALIDATION`; there is no numeric-entry UI in this stage.
- Evidence-to-solver-standard conversion is reusable outside the bring-up campaign
  path. The campaign can now accept compact OPEN/SHORT/LOAD standards directly, letting
  the product wizard solve each condition without duplicating solver math.
- Candidate creation starts at wizard start. Save is explicit; the candidate is not
  committed to W25Q until the user confirms `CONFIRM_SAVE`. Commit success activates the
  verified set; commit failure preserves the previous active calibration and leaves the
  candidate retry/discard path visible.
- Calibration wizard UI state is a compact snapshot rendered by the existing fallback
  product UI. Implemented screens cover intro, OPEN/SHORT/LOAD prompts, capture
  progress, range complete, save confirmation, committing, complete, safety-blocked,
  canceling/canceled, and failed states.

Software evidence:

- Host tests cover dynamic 33-condition enumeration, safety-blocked wizard acquisition,
  full range-batched wizard progression through 33 solved conditions before explicit
  save, active-calibration validity separate from dirty candidate state, mandatory gate
  entry, and the Stage 2A menu/status flow.
- Final STM32 PRODUCT Debug/Release builds use the PRODUCT size-first compile policy
  for large application/UI/metrology units and report 55096 B Flash, 14480 B static RAM,
  and 16528 B accounted RAM. This is below the project hard gates but above the soft
  Flash target and preferred PRODUCT accounted-RAM target.
- Final STM32 BRINGUP reports 53564 B Flash, 13068 B static RAM, and 15116 B accounted
  RAM with the BRINGUP profile symbol check passing; product wizard/UI symbols are not
  linked into the bring-up profile.

Remaining Phase 08 work:

- complete full menu tree beyond Calibration/Back;
- persistent settings;
- localization/resource text IDs and external fonts;
- graph pages and debug console page;
- physical calibration workflow validation on the Rev.1 board;
- fixture/load-standard value qualification.

## Stage 2A.1 — calibration wizard lifecycle and product Flash hardening

STATUS: IMPLEMENTED

Implemented software boundary:

- PRODUCT has one calibration-service scheduler owner: `app_shell.c` calls
  `app_calibration_service_step()` once per superloop. The wizard starts candidate
  commits and observes candidate/service state; it does not step the W25Q store FSM.
- Product runtime preemption now separates presentation state from teardown ownership.
  Fatal product faults may show `FAULT` immediately, and calibration-validity loss may
  show the mandatory gate, but an active measurement/calibration runtime continues to
  receive cooperative steps until Phase 05 abort/acknowledge cleanup has completed.
- The product measurement/calibration union is not overwritten while the current
  runtime is active or draining. Runtime switches occur only after the active session or
  wizard is terminal/inactive.
- Wizard cancellation enters `CANCELING` while Phase 05 is still active and reports
  `CANCELED` only after candidate discard succeeds. Discard `BUSY` remains canceling;
  discard errors surface as failed storage state.
- Wizard retry handling is error-specific. Phase 05 and solver failures may retry the
  current condition. Commit failures retry the commit after the store terminal state is
  acknowledged by the calibration service. Candidate-incomplete, fixture/condition, and
  storage/candidate-busy failures do not start a generic capture or advance into an
  invalid range.
- Calibration temperature provenance is sampled immediately before each exact
  OPEN/SHORT/LOAD workflow starts. Temperature remains fixed for that workflow's six
  accepted samples, while the next condition may use a newer NTC snapshot. The wizard
  exposes O/S/L min/max/span diagnostics for the last solved condition.
- Host tests cover measurement and calibration fault preemption, user cancel during
  calibration capture, workspace ownership during abort, candidate-incomplete retry,
  complex LOAD fixtures, per-standard temperature provenance, and full 33-condition
  save/verify/activate/reboot through a fake NOR store.
- PRODUCT CMake policy was cleaned so size optimization is target-wide instead of a
  redundant per-source list. Product Debug intentionally uses `-Os` plus symbols.
  Product Release enables LTO for deterministic Flash recovery.

Flash/RAM evidence from local STM32 builds:

```text
                       BEFORE      AFTER
PRODUCT Debug Flash     55096      55756
PRODUCT Debug RAM       16528      16528
PRODUCT Release Flash   55096      51132
PRODUCT Release RAM     16528      16508
BRINGUP Flash           53564      53648
BRINGUP RAM             15116      15116
```

The Release image recovered 3964 B of Flash relative to the Stage 2A baseline, leaving
6212 B below the 56 KiB project hard gate. `.rodata` is about 2976 B after LTO; the
largest remaining internal text data are the fallback glyph table and small metrology
lookup tables, not a full localization/resource pack.

Phase 08 code-only Flash forecast before Stage 2B:

```text
Current PRODUCT Release:        51132 B
Settings store/controller:    +  900..1400 B
Display menu:                +  500..900 B
Sound menu:                  +  300..600 B
Language/resource resolver:  +  700..1200 B
About/debug page skeleton:   +  500..1000 B
Graph primitives:            +  900..1600 B
Projected code image:          53932..57832 B
```

Stage 2B should therefore add resource/text IDs and keep normal menu strings/assets in
W25Q where practical. Emergency fault, storage, calibration gate, and safety text must
remain internally available.

## Stage 2B — settings and normal menu expansion

STATUS: IMPLEMENTED_REQUIRES_BENCH_VALIDATION

Implemented software boundary:

- Product settings are represented by `app_settings_t` with only the Rev.1 Stage 2B
  fields: display brightness percent, backlight inactivity timeout, and sound enabled.
  Defaults are 25%, 60 s, and sound enabled.
- Settings validation is semantic rather than raw-struct based: brightness must be
  5..100%, timeout must be one of OFF/15/30/60/120/300 s, and sound is serialized as a
  strict boolean.
- W25Q mutable layout is rebalanced without changing the total reserved region or the
  existing calibration/bringup anchors:

```text
calibration A: 1 sector
calibration B: 1 sector
settings A:    1 sector
settings B:    1 sector
diagnostics:   3 sectors
bringup test:  1 sector
```

- `app_settings_service_t` owns two-slot transactional settings persistence. The frame
  is portable little-endian and includes magic, schema version, frame/payload size,
  sequence, payload CRC32, payload, and a final commit marker programmed last.
- Settings load independently classifies both slots and chooses the newest valid
  sequence with wrap-safe ordering. If no valid settings exist, PRODUCT uses defaults;
  settings storage failure is nonfatal and does not affect the mandatory calibration
  gate.
- Settings save erases/programs/verifies the inactive slot before programming the final
  commit marker. A failed replacement leaves the previous valid slot untouched, keeps
  the in-RAM setting applied and dirty, exposes save failure, and allows retry.
- PRODUCT storage ownership now blocks generic W25Q polling while calibration or settings
  owns a mutation. Boot load order after W25Q probe is calibration first, then settings.
- `app_product_cancel()` was removed from the public API because it did not preserve the
  Stage 2A.1 safe-drain lifecycle. Cancellation remains owned by the active measurement
  or calibration wizard flow.
- Product UI calibration validity now treats `calibration_active_valid=true` as ACTIVE
  even while the calibration service reports active workflow, store busy, or dirty
  candidate states.
- `app_product_t` exposes desired hardware effects through `app_product_outputs_t`.
  `app_shell.c` applies backlight and buzzer settings; the view renderer remains
  presentation-only and does not manipulate hardware.
- Main menu now contains Calibration, Display, Sound, About, and Back. Display contains
  Brightness, Backlight Timeout, and Back. Sound contains Sound On/Off and Back. Language
  and Debug menus remain out of this Stage 2B implementation.
- Brightness editing uses 5% steps with live backlight preview. Changes are dirty in RAM
  while editing; the persistent write is requested only on explicit confirmation, not on
  every button repeat. Long OK cancels and restores the entry value.
- Backlight inactivity uses wrap-safe millisecond comparisons. OFF timeout never blanks;
  otherwise timeout drives desired backlight to 0%. The first physical button press wakes
  the backlight and consumes the whole gesture.
- Sound enable/disable is loaded from settings and applied by the shell. Quiet mode
  remains authoritative over the buzzer.
- A small About page reports firmware version, hardware compatibility, short git commit,
  and calibration schema.
- `ui_text_id_t` and `ui_text_fallback()` provide stable text IDs and internal fallback
  strings for normal menu/settings/About text. External W25Q resource text remains a
  later Stage 3 concern.

Software evidence:

- Host tests cover the rebalanced W25Q layout, settings defaults/validation, A/B
  save/load, wrap-safe sequence selection, failed replacement preserving the previous
  valid slot, display brightness preview without immediate persistence, and wake-consume
  behavior.
- PRODUCT Release remains below the Stage 2B preferred final budgets from the Stage
  2A.1 forecast at 55164 B Flash and 16724 B accounted RAM in local STM32 Release build.
- STM32 Debug remains a non-LTO diagnostic build and reports size/profile information
  without enforcing the PRODUCT Flash hard gate. PRODUCT Release and BRINGUP continue to
  enforce the existing budget gates.

Remaining Phase 08 work:

- external resource/font pack and localization storage integration;
- graph/result pages beyond the current primary/details pages;
- debug console page if retained for PRODUCT;
- physical validation of buttons, backlight timeout/wake behavior, sound setting, and
  W25Q settings power-loss behavior on Rev.1 hardware.

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
