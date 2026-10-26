# Same-image OPEN/SHORT checkpoint — October 25, 2026

Status: OPEN_SHORT_COMPLETE_LOAD_PENDING; NOT_QUALIFIED.

The owner removed the SHORT jumper and confirmed completely empty DUT terminals
("está open"). The new OPEN sweep accepted all 33 keys with six stable repeats
per condition, SAFE teardown and no new protocol errors. It shares the exact
source-7017e5e0 identity and nominal ADC calibration with the corrected 33-key
SHORT sweep. All 66 original CCO1 frames, canonical metadata and wire logs are
preserved. The old source-063c7378 captures are historical only.

The strict per-key audit found 33/33 common OPEN/SHORT return paths;
0 conditions have an empty intersection. This checks acquisition
compatibility, not OSL solvability or accuracy. HG-only triplets additionally need
actual same-key HG overlap from an accepted standard. No nominal gain is relabelled
as observed. LOAD captures, serialized solver validation and independent holdouts
remain pending. No candidate was generated or installed; scope-dependent checks
remain NOT_TESTED. Final leads will require calibration in their final geometry.

See [structured checkpoint and path audit](OSL-Same-Image-Endpoints.json),
[physical evidence](../../../Firmware/releases/2026-10-25/same-image-osl-endpoints/README.md)
and [the matching nine-firmware/deployment bundle](../../../Firmware/releases/2026-10-25/zero-short-bringup/README.md).
