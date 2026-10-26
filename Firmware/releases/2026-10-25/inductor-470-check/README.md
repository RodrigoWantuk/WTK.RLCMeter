# Known 470-uH PRODUCT check

The owner fitted only a nominal 470-uH choke after removing the capacitor.
Read-only preflight verified current source 3052bdde, active OSL sequence 1 with
33 records, K1 SAFE, range/excitation OFF and no fault. Two owner-triggered short
OK presses produced completed INDUCTIVE results with valid published L:

| Session | Primary L | Frequency | RREF / return | Attempts |
| --- | ---: | ---: | --- | ---: |
| 3 | 461.356 uH | 1 kHz | 10 ohm / HG | 4 |
| 4 | 472.200 uH | 1 kHz | 10 ohm / HG | 2 |

The second session reused the previous condition hint, then checked 10 kHz.
The full attempt history remains inspectable: first-session estimates span
425.389–463.528 uH across 1/10-kHz and 10/100/1000-ohm conditions; the repeat's
secondary 10-kHz/10-ohm result is 435.878 uH. This frequency/range spread must not
be hidden or reclassified as a qualified accuracy bound. The nominal DUT value
and unknown tolerance are not an independent impedance reference. Resistance,
Q/D, phase, temperature effects and lower-L limits remain unqualified.

Final physical metadata confirms K1 SAFE, range/excitation OFF, quiet released,
zero safety faults, resources OK, display normal-clock policy/75% brightness,
and the same complete active calibration. No firmware, calibration, resource or
settings write occurred, and no resistor test was repeated. Software tests were
not rerun because no firmware code changed. Exact firmware/ABI verification,
actual completed measurements and SAFE teardown are the relevant checks here.

This confirms basic inductive measurement and classification at 470 uH. It does
not validate 2.2 uH or 0.15 uH, or resolve the earlier unidentified small choke's
resistance-like indication. Those very small values require separate sensitivity,
fixture/parasitic and frequency checks. All observations and bounded read-only
procedures, including the raw product context and original transcripts, are in
physical-evidence.zip; no battery voltage was read via SWD.
