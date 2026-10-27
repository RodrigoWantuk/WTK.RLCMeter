# Serial short-OK control

STATUS: IN_PROGRESS

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
