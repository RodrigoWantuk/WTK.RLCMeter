# Serial short-OK control

STATUS: IMPLEMENTED_DEPLOYED_SERIAL_OK_BASIC_CHECK_PASS_REQUIRES_PHYSICAL_QUALIFICATION

The owner confirms TTL COM4, ST-Link and only the previously tested 470-uH
inductor remain connected, but cannot operate the physical button. Add an
explicit serial option to request one short OK through the ordinary UI path,
then use it for the pending continuous-survey functional check. Do not write
button/state memory through SWD or bypass measurement permission.

Reuse each profile's existing UART framing/ownership. PRODUCT uses a distinct
CRC-checked PLC1 command with strict payload identity, request sequence and
expected measurement-session sequence. Query state before activation; reject
duplicates/stale commands, busy/quiet/resource transfer, physical-button overlap,
menus/calibration/fault or unsafe power/hardware state. Accepted control performs
one short PRESS/RELEASE, with no long press, repeat, automatic retry or boot
measurement. Report acceptance separately from completed acquisition. A sleeping
UI retains its ordinary wake-only first click semantics.

Expose a CMake feature option and document profile support explicitly. Production
profiles must support the UI control; diagnostic profiles retain their existing
commands and must not map an OK into a raw relay/capture shortcut. Preserve
exclusive calibration-service protocol and resource payload framing; do not sniff
or strip arbitrary byte sequences inside another protocol's data.

Add pure protocol/UI tests and a bounded PC tool that records request/reply and
does not retry activation on timeout. Keep all safety checks, seven-point trim,
three-frequency policy, OSL/assets/NTC and memory hard budgets. Run relevant host
tests and strict embedded profiles on clean source, deploy after live SAFE checks,
capture UART boot, then perform one explicitly authorized unchanged-DUT trial.
Live completed-result/health reads must not halt or reset acquisition and must
not read numeric battery voltage through SWD. Physical contact count cannot be
claimed from software metadata while the owner is absent.

Acceptance checkpoint: 50/50 factory and 48/48 standard host tests, all nine
strict unit-bound embedded profiles and the serial-OFF factory variant pass.
Source 9ecaa696ec78 was programmed/read back exactly; controlled UART boot had
one SAFE_BOOT, READY/SAFE, zero sessions and all six flags. Active OSL 1/all 33,
assets/settings and unit NTC were preserved. Current usage is 61,596 B Flash /
17,116 B accounted RAM; the factory-size register declares 64 KiB.

One owner-authorized serial activation produced a final 472.656-uH result on the
unchanged nominal 470-uH inductor. Seven admitted primary points, masks 69/38,
coverage 7/7, five actual power refreshes and complete SAFE cleanup are recorded.
A read-only query and an exact stale-packet replay left session/capture counts
unchanged; the replay received STALE_REQUEST. The initial direct replay had no
reply and remains recorded as ambiguous, without inferring its cause.

A Windows report-file replacement lock stopped the first pre-program attempt.
Recovery confirmed the unchanged old image and known halted SAFE-reset state
before the successful retry. Tooling fix 42cd8d8 adds bounded PermissionError
retries while preserving atomic records; 61 persistence-suite cases pass with
five artifact-only skips. Embedded source remains 9ecaa696ec78.

Owner absence leaves physical relay contact count, TFT appearance, independent
metrology and switching/abort timing unqualified. No new capacitor or actual
10-mF/polarized qualification is claimed. See the
[delivery and evidence](../Firmware/releases/2026-10-27/serial-ok-control/README.md).
