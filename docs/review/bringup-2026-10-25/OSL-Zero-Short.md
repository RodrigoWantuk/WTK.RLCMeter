# Corrected SHORT checkpoint — October 25, 2026

Status: SHORT_COMPLETE; SAME_IMAGE_OPEN_AND_LOAD_PENDING; NOT_QUALIFIED.

All 33 SHORT conditions were accepted on firmware source `7017e5e0bc6e7188de3c6b270d49267f322c096d`.
33/33 conditions accepted both return paths, including the three earlier
conflict keys, which were deliberately acquired first. Each condition retained
six stable repeats, SAFE teardown and unchanged protocol-error count. The
owner-installed jumper remained on the direct DUT terminals; length was not
reported. No calibration candidate was generated or installed.

The 35864-byte BRINGUP_CAL image was programmed and read back byte-for-byte,
then identified over COM4 before capture. Nine embedded builds and all 44 CTests
passed. The zero-return raw-block regression failed before the fix and passed
afterward; absent source, clipping and zero OPEN/LOAD remain rejected. HG gain
overlap retains its nonzero signal requirement. Safety gates, ADC timing,
excitation levels and schema/model are unchanged.

The prior source-063c7378 OPEN/SHORT data are retained as historical evidence,
not mixed into this cohort. OPEN and LOAD must now be acquired under this same
image. Hantek-dependent checks, independent holdouts, uncertainty and production
qualification remain pending. No accuracy or release certificate is implied.

See [structured checkpoint](OSL-Zero-Short.json) and
[current firmware/evidence bundle](../../../Firmware/releases/2026-10-25/zero-short-bringup/README.md).

A post-SHORT no-reset RAM snapshot read battery 3.7263 V (nominal ADC scale,
not independently calibrated), state OK, sample age 265 ms and zero sensor
faults. The COM4 status was SAFE/idle before and after SWD halt/resume.
