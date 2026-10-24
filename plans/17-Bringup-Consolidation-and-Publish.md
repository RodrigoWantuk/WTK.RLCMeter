# 17 — Consolidate first-board findings and publish current sources

Status: IN_PROGRESS. Explicit owner assignment, 2026-10-24, extending plans03/08/09A/12.

Integration verified: shared mounted TFT init/clock, current hardware0x00010002
in C/PC tools and old-topology rejection, October24 source/fabrication audit and
review matrices.44/44 CTests,486 Python tests with99 environment skips,18 PCB tests
and nine embedded profiles/variants passed. Final clean-source build identity and
Git publication are the remaining delivery actions. See
[the consolidation report](../docs/review/bringup-2026-10-24/README.md).

Consolidate the bench-proven acquisition/DSP/relay changes across firmware profiles,
promote the working portrait180 display initialization/clock into normal PRODUCT
and BRINGUP, separate RGS-to-GND calibration identity, reconcile the owner's new
CAD/BOM/Gerber/PDF exports, update current documentation and push the full change
set to the existing branch. Review CHG_VBUS up to5.5V from actual connectivity and
primary component specifications. No production deployment or calibration creation
is requested here; the current safe peripheral image may remain on the board.

Prerequisites: existing shared safety/acquisition/DSP and product calibration gate,
owner-confirmed flat solder repair/display/button test, currentOctober24 CAD exports.
Expected files: shared TFT/SPI initialization and regressions, calibration hardware
identity in C and PC tools/fixtures, PCB audit paths/net assertions/review matrices,
current architecture/pinout/assembly/build docs, owner PCB replacements and a
consolidated report/build manifest. Preserve historical bench records as history.

Acceptance: current source/fabrication reconciliation including all six RGS and
CHG input, host/Python/PCB checks, embedded PRODUCT debug/release/factory and
BRINGUP/BRINGUP_CAL plus isolated diagnostic builds within hard memory/composition
limits; traceable source/build identity; pushed branch containing owner exports.
Do not disable charger/residual/battery/calibration/quiet safety gates or copy
bench-supply exceptions into PRODUCT. Invalid old-topology calibration must be
rejected. Do not claim calibrated accuracy, battery/charger qualification,
polarized-capacitor support or full release qualification from these builds.

The owner requests final-product readiness. Deliver buildable integrated sources
and a precise remaining physical-release list; unexecuted physical acceptance
remains REQUIRES_BENCH_VALIDATION, never fabricated PASS.
