# `app`

Application layer for the firmware.

## Responsibilities

- global state machine;
- orchestration between safety, measurement, storage, and UI;
- event dispatch;
- retry/rerange policy;
- global fault handling;
- firmware/hardware version information exposed to diagnostics/UI.

## Must not

- access GPIO directly;
- call ILI9341/W25Q drivers directly when a higher-level service exists;
- calculate phasors or impedance;
- energize relays or alter ranges without going through `hardware` services.

## Implemented application services

- `app_shell.c`: product boot/superloop orchestration and global fail-closed handling.
- `app_safety_fault.c/.h`: latched internal safety-fault bitmask.
- `app_measurement_session.c/.h`: automatic measurement-session controller used by product policy and host tests.
- `app_io_workspace.c/.h`: one explicit 3072-byte scratch arena with exclusive owners
  for Phase 05 raw metrology capture and calibration-store frame serialization.
- `app_calibration_*`: product-owned calibration runtime, store lifecycle, OSL workflow,
  and calibration wizard/session state. The product service does not own campaign
  aggregation state; BRINGUP owns that helper explicitly when its diagnostic commands
  need it.
- `app_bringup_console.c/.h`: bring-up-profile UART command surface for physical board diagnostics.

`app_bringup_console.c/.h` is compiled only for `WTK_FIRMWARE_PROFILE=BRINGUP`. It
does not own product calibration validity or automatic measurement-session state; host
tests/tools remain the place for rich policy analysis and verbose presentation.

The calibration service borrows `app_io_workspace_t` only while loading/scanning or while
an asynchronous commit is in progress. Commit ownership is released after the terminal
store state is acknowledged. Metrology sessions borrow the same workspace only while a
raw block is active and release it after the block is acknowledged. A busy workspace is
reported as `BSP_STATUS_BUSY`; neither side silently allocates fallback storage.

The product controller publishes compact UI snapshots instead of duplicating complete
Phase 07 session results inside the display model. The Phase 07 policy remains the
authoritative source for partial/final measurement data.

PRODUCT owns measurement-session and calibration-session callback tables as static
immutable shell objects. `app_product_t`, `app_measurement_session_t`, and
`app_calibration_session_t` keep references to those tables; the referenced IO table
lifetime must exceed the context lifetime. Host tests use persistent fake IO tables to
exercise the same contract. This avoids duplicating immutable function-pointer tables in
the hot application/session contexts.

Product resource health is owned at application level. Fatal normal resource failures
from text/font resolution or resource admission latch PRODUCT `RESOURCE_ERROR` and
preempt new settings persistence, but they do not become hardware safety faults.
Deferred W25Q access caused by quiet mode or active settings/calibration mutation
remains transient backpressure. Phase 08 Stage 3B.1 admission validates both text
catalogs and all three external A1 font roles before normal PRODUCT UI rendering is
allowed; emergency presentation remains internal and W25Q-independent.

Phase 08 Stage 2A.1 hardens asynchronous ownership:

- `app_shell.c` is the single PRODUCT owner that calls
  `app_calibration_service_step()` once per superloop.
- The calibration wizard starts candidate commits and observes candidate/service state;
  it does not step the storage service itself.
- Product fault or calibration-gate preemption separates presentation from physical
  teardown. The UI may show `FAULT` or `CALIBRATION_REQUIRED` immediately, while the
  active measurement or calibration runtime continues receiving `step()` calls until
  Phase 05 has acknowledged abort/cleanup and the shared workspace is free.
- The measurement/calibration union is not reinitialized for a different runtime while
  the current runtime is active or draining.
- Wizard cancellation reports `CANCELED` only after candidate discard succeeds.

## Invariants

- every path entering MEASURE has an explicit return-to-SAFE path;
- critical faults stop excitation and request SAFE immediately;
- no state transition relies on a blocking delay;
- current state and fault reason remain observable through diagnostics.
