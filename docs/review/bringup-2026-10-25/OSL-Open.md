# First-board OSL OPEN checkpoint — October 25, 2026

Status: OPEN_COMPLETE; SHORT_AND_LOAD_PENDING; NOT_PHYSICALLY_QUALIFIED.

All 33 OPEN conditions were accepted on the real board, with at least six stable
blocks per condition and verified SAFE teardown. The first accepted artifact was
preserved; the remaining 32 used a 21-second UART quiet window. No new protocol
errors occurred in that resumption; the earlier lost-frame count is retained.
The board remains BRINGUP_CAL, no calibration is installed, and the observed
storage successor remains 1. This is acquisition evidence, not an accuracy or
production-release certificate. Hantek-dependent checks remain NOT_TESTED.

Firmware source: `063c7378319f293640d6d19d5fadcb3fa3ad686f`. Its nine embedded builds passed;
the 35804-byte BRINGUP_CAL image was programmed and verified byte-for-byte before
boot identification. Hardware identity is 0x10002, model 5. All 44 CTests, 44
GUI/model tests and the latest 16 capture-client tests passed. See
[structured checkpoint](OSL-Open.json) and [firmware/evidence bundle](../../../Firmware/releases/2026-10-25/osl-open-checkpoint/README.md).

The owner requested proceeding using prior DC observations while the Hantek was
unavailable. No missing physical test was relabelled PASS; the desktop B01 gate
remains unchanged. Embedded residual, charger, battery, range, clock, fault and
teardown gates remained active. Source fixes cover sample freshness, drained
aborts, arbitrary common phase, near-zero endpoint statistics, deferred fresh
safety publication and host quiet time. Rejected attempts remain in the archive.

The fresh battery checkpoint before this sweep was 3.774652 V nominal ADC scale,
state OK, age 33 ms, zero faults. No state-of-charge/runtime claim is made.
Board NTC estimates during OPEN ranged from 22.641 to 24.854 C;
these are provisional sensor estimates, not DUT temperature qualification.

Fixture is direct terminal attachment. The six proposed LOAD anchors remain
nominal 10 / 100 / 1k / 10k / 100k / 1M ohm with 1% marked/inferred tolerance;
uncertain band identifications and DMM readings are preserved as owner evidence.
Ask for a fresh resistor reading immediately before each LOAD attachment.
The SHORT link has not yet been attached or characterized. Final test leads
require calibration in their final geometry. No calibration candidate has been
generated, committed or installed, and W25Q calibration writes were not requested.

A fresh no-reset battery read after OPEN returned 3.753700 V nominal,
state OK, age 299 ms, zero faults. The core was resumed.
