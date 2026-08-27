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
- `app_calibration_*`: product-owned calibration runtime, store lifecycle, OSL workflow, and campaign state.
- `app_bringup_console.c/.h`: bring-up-profile UART command surface for physical board diagnostics.

`app_bringup_console.c/.h` is compiled only for `WTK_FIRMWARE_PROFILE=BRINGUP`. It
does not own product calibration validity or automatic measurement-session state; host
tests/tools remain the place for rich policy analysis and verbose presentation.

## Invariants

- every path entering MEASURE has an explicit return-to-SAFE path;
- critical faults stop excitation and request SAFE immediately;
- no state transition relies on a blocking delay;
- current state and fault reason remain observable through diagnostics.
