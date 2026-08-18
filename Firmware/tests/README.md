# `tests`

Host-side tests and validation vectors for the firmware.

## Priorities

- complex mathematics;
- synchronous I/Q / DFT extraction;
- impedance equation;
- R/C/L derivation;
- calibration application;
- autorange;
- confidence gates;
- persistent record parsing/CRC;
- pure application state machine;
- asset manifest parsing.

## Test vectors

Maintain fixtures for ideal R, C, and L cases plus noise, clipping, offset, timing/phase error, and OPEN/SHORT-adjacent conditions.

Host tests do not replace bench qualification, but they prevent mathematical and state-machine regressions before flashing the MCU.
