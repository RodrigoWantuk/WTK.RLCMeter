# Measurement Operating Modes

This document defines the user-facing operating semantics for the two normal measurement modes of WTK.RLCMeter Rev.1: **Click** and **Live**.

These modes change *when* complete measurement transactions are requested. They do not create different metrology equations, safety rules, calibration models, or hardware-control paths.

## Design principle

The instrument always measures complex impedance first and interprets the DUT afterward. Neither Click nor Live mode asks the user to preselect resistor, capacitor, or inductor type.

Both modes use the same measurement engine:

```text
user/application request
    ↓
pre-measure safety validation
    ↓
range / excitation selection
    ↓
measurement permit
    ↓
K1 MEASURE window
    ↓
deterministic ADC/DMA acquisition
    ↓
K1 SAFE
    ↓
DSP / impedance / derived quantities
    ↓
classification / confidence
    ↓
result publication
```

A measurement result is published only from a completed, valid acquisition/processing transaction. Raw ADC samples are never presented directly as a measurement value.

## Mode 1 — Click measurement

**Click mode is the deliberate, one-request/one-result operating mode.**

In the normal measurement/result screen, a short press of **OK** requests one complete automatic measurement.

Conceptually:

```text
READY / LAST RESULT
    ↓
OK short
    ↓
MEASURE ONCE
    ↓
valid result or explicit error
    ↓
SAFE
    ↓
LAST RESULT
```

### Click-mode behavior

- One short OK press requests one complete automatic measurement transaction.
- Autorange, channel selection, frequency selection/refinement, classification, and confidence processing are internal to that transaction.
- The instrument may perform multiple internal acquisition attempts to produce the final result; these are still considered one user-requested measurement.
- The display may show valid intermediate/refined results between critical acquisition windows.
- When the transaction finishes, the final result remains on screen until the user requests another measurement, changes page, or enters another UI context.
- UP/DOWN continue to navigate result pages and do not alter metrology parameters in normal operation.
- Click mode is the preferred mode for a stable DUT, deliberate measurements, documentation, comparison, and situations where the user wants an explicit measurement boundary.

### Retriggering

While a Click measurement is already active, additional short-OK events must not create overlapping metrology sessions.

The UI/application may either ignore the additional request or report that measurement is busy. It must not queue an unbounded number of future measurements.

## Mode 2 — Live measurement

**Live mode continuously refreshes the displayed result by scheduling repeated complete measurement transactions.**

It is intended to behave like the continuously updating display of a conventional handheld multimeter while preserving the RLC meter's stricter measurement and safety sequencing.

Conceptually:

```text
LIVE ENABLED
    ↓
request measurement
    ↓
complete SAFE → MEASURE → SAFE transaction
    ↓
process / publish result
    ↓
UI update
    ↓
if Live still enabled and no blocker exists:
request next measurement
    ↓
...
```

Live mode is therefore **repeated measurement**, not one indefinitely open acquisition session.

## Critical Live-mode safety rule

Live mode must **not** mean keeping K1 continuously energized in MEASURE.

Every Live refresh remains a bounded measurement transaction and must use the same safety ownership and permit rules as an ordinary measurement.

At a minimum, each transaction must preserve the normal rules for:

- charger/interlock state;
- latched safety faults;
- selected range validity;
- measurement permit issue/validation;
- controlled excitation;
- K1 operate/release sequencing;
- ADC/DMA ownership;
- fail-safe abort;
- return to SAFE after the acquisition window.

The implementation may optimize non-safety work between Live iterations, but it must not optimize away a required safety invariant merely to increase refresh rate.

## Live refresh semantics

Live refresh rate is **transaction-driven**, not a fixed UI-frame rate.

The next measurement should start only after the previous transaction has:

1. completed its hazardous acquisition window;
2. returned K1 toward SAFE according to the metrology contract;
3. completed required cleanup/recovery;
4. produced or rejected the result;
5. allowed any required fresh safety evidence to be reacquired;
6. yielded sufficiently for UI/application work.

This prevents the user interface from imposing an arbitrary refresh frequency on the analog/safety subsystem.

The actual observable update rate will therefore vary with:

- selected RREF/range;
- excitation frequency;
- settling time;
- number of autorange attempts;
- confidence/refinement requirements;
- additional frequencies required for classification;
- rejected/invalid attempts;
- safety reacquisition time.

A later qualification phase may place a minimum inter-measurement interval or adaptive refresh cap if required for relay life, power consumption, thermal behavior, noise, or usability.

## Live-result publication

During Live operation:

- the last valid result remains visible while a new transaction is in progress;
- a new value replaces it only when a new valid partial/final result is available;
- the UI should visually indicate that Live mode is active;
- stale data must not be visually indistinguishable from a freshly confirmed result after a measurement blocker or error;
- invalid acquisitions do not silently overwrite a valid value with a plausible-looking number;
- repeated failures should surface an explicit status such as blocked, unsafe, out of range, disconnected/open, or measurement error as appropriate to the final product policy.

The display must not redraw continuously during critical ADC/DMA acquisition. Rendering remains scheduled between acquisition windows, following the existing quiet-mode contract.

## Leaving Live mode

Disabling Live mode means **stop scheduling future measurement transactions**.

If the user exits Live while a transaction is already inside a measurement cycle, the application should request a controlled stop at the earliest safe boundary. Safety/fault handling always takes precedence over UI responsiveness.

The mode transition must never leave:

- K1 in MEASURE;
- excitation active without ownership;
- the range decoder in a transitional state;
- ADC/DMA resources orphaned;
- quiet mode permanently asserted.

After Live is stopped, the last valid result may remain available for ordinary result-page navigation.

## Button and navigation relationship

The established normal result-screen contract remains:

```text
OK short     request a single measurement in Click mode
UP           previous result page
DOWN         next result page
OK long      open main menu
```

The exact user gesture/menu setting used to switch between Click and Live is a UI policy decision and must not be implemented by stealing the already-defined long-OK menu gesture or by introducing undocumented double/triple-click behavior.

Once the selector is implemented, the active mode should be obvious on the measurement screen and persistent only if product UX explicitly chooses persistence. The metrology layer itself must not depend on how the UI selected the mode.

## Backlight-off behavior

The existing wake policy applies in both modes: when the backlight is fully off, the first button press wakes the display and is consumed.

In Click mode, that wake press must not also start a measurement.

In Live mode, backlight timeout and measurement activity are separate policies. Turning the backlight off must not implicitly change the safety state or invent a new measurement mode. Product power policy may later choose whether Live measurement continues with the display off, but that decision must be explicit and qualified.

## Relationship to autorange and multi-frequency measurement

Click/Live and autorange are orthogonal concepts.

One user-visible refresh may internally contain several attempts:

```text
request
  ↓
try range A
  ↓
rerange
  ↓
try range B
  ↓
primary result
  ↓
optional second/third frequency
  ↓
confidence/classification refinement
  ↓
publish
```

In Click mode this entire sequence belongs to the single requested measurement.

In Live mode this entire sequence belongs to one Live refresh; the next refresh starts only after the current transaction is closed safely.

## Relationship to result pages

Both modes use the same result model and result pages.

Click mode naturally leaves a stable result for browsing.

Live mode may continue refreshing the underlying result while the user remains on the primary measurement screen. If the user navigates to detailed/graph pages, the final UI implementation must clearly choose whether Live continues in the background or is temporarily suspended. It must not allow concurrent UI behavior to bypass quiet-mode or measurement ownership rules.

## Application architecture

The modes belong above the low-level metrology FSM.

A suitable conceptual ownership split is:

```text
UI
  └── selects CLICK or LIVE / emits user intent

application measurement controller
  ├── CLICK: schedule exactly one transaction per request
  └── LIVE: schedule next transaction after previous completion

metrology / hardware safety stack
  └── executes one bounded safe measurement transaction
```

The low-level measurement engine should not contain separate "unsafe fast path" logic for Live mode.

A useful future application state model is conceptually:

```text
MEAS_MODE_CLICK
MEAS_MODE_LIVE

IDLE
REQUESTED
RUNNING
RESULT_READY
BLOCKED_OR_ERROR
```

Live scheduling is an application policy around `RUNNING → RESULT_READY → REQUESTED`, not a special permanent K1 state.

## Fault behavior

Any safety fault immediately dominates the selected operating mode.

In both Click and Live modes:

```text
fault / charger / invalid safety condition
    ↓
abort current measurement as required
    ↓
excitation safe/off
    ↓
K1 SAFE
    ↓
range safe/disabled as required
    ↓
stop automatic measurement scheduling
    ↓
report status
```

Live mode must not repeatedly hammer a blocked safety condition in a tight loop. Re-entry/retry policy must be bounded and event/state driven.

## Diagnostics

Debug/Bringup diagnostics should identify the operating mode independently from the metrology transaction mode.

Recommended fields include:

```text
user_measure_mode=CLICK|LIVE
refresh_sequence=<n>
attempt=<n>
measurement_state=<...>
result_valid=<0|1>
result_age_ms=<...>
```

This distinction is important because a single Live refresh can contain multiple internal acquisition attempts, while `DUT_MEASURE` remains the low-level metrology mode.

## Non-goals

Click and Live do not change the following Rev.1 rules:

- no preselection of R/L/C in ordinary use;
- no measurement of energized mains/high voltage;
- no CAT rating claim;
- no UI ownership of relay/range GPIO;
- no bypass of calibration validity;
- no raw-sample display masquerading as a final measurement;
- no overlapping measurement sessions;
- no continuous K1 MEASURE merely because Live is enabled.

## Implementation handoff

When these modes are implemented in Phase 08/product integration, the developer must preserve the following frozen semantic distinction:

```text
CLICK = one user request schedules one complete automatic measurement transaction.

LIVE  = the application repeatedly schedules complete automatic measurement
        transactions, publishing each valid result, while Live remains enabled.
```

The distinction is scheduling/UX policy. The safety and metrology transaction underneath both modes remains the same.
