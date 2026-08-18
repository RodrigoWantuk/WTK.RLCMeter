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

## Planned files

```text
app_state_machine.c/.h
app_events.c/.h
app_context.c/.h
app_scheduler.c/.h
app_faults.c/.h
app_version.c/.h
```

`app_lab_console.c/.h` is a narrow Lab-build-only diagnostic hook for virtual/bench bring-up. It currently accepts only quiet-mode, buzzer, and reserved-sector W25Q commands. It is not the Phase 08 product debug console and must not grow relay, range, excitation, or safety-bypass commands.

## Invariants

- every path entering MEASURE has an explicit return-to-SAFE path;
- critical faults stop excitation and request SAFE immediately;
- no state transition relies on a blocking delay;
- current state and fault reason remain observable through diagnostics.
