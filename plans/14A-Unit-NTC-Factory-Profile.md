# 14A — Experimental per-unit NTC factory profile

STATUS: IN_PROGRESS — OWNER_AUTHORIZED_ENGINEERING_CALIBRATION

## Scope and authority

The owner explicitly requested using the reported 30.1 degC reference despite
uncertain finger/probe contact. This authorizes an experimental calibration, not
physical accuracy qualification. The cold bead-contact reference is 23.5 degC
at ADC 1952; the sustained warm reference is 30.1 degC at ADC 1856. The earlier
warm ADC 1877 sample is transient diagnostic evidence and is not a fit anchor.
The purchased NTC is described only as 100 kohm; intrinsic Beta is unknown.
Do not request or record the contact thermometer model.

## Explicit implementation decision

Use an optional, versioned **build-time factory profile** for this unit, rather
than inventing a mutable NTC storage partition or changing the UI settings ABI.
The JSON profile contains the MCU UID, two raw-ADC/reference points and explicit
unqualified/contact-uncertain provenance. Fit an effective R25/Rtop ratio and
Beta on the PC. These describe the supplied observations, not verified intrinsic
component characteristics. Generate a 21-point constant lookup table, retaining
the existing bounded linear interpolation and -20..80-degC nominal domain.
Include both reference temperatures exactly as table nodes. Extrapolation is an
engineering model; it is not qualified beyond the supplied points.

Default builds retain the original 100-kohm/B3950 table exactly. Selected unit
builds replace only table data, avoiding a runtime log/heap/new buffer and keeping
the tight 64-KiB BRINGUP budget. The profile is persistent in the verified MCU
image; recalibration requires rebuild/reflash. This is not a runtime trim API.

The build configuration and deployment manifest carry the profile hash and UID.
Manifest validation must verify that the expected float32 lookup-table bytes
exist in the linked binary. Programming must reject a mismatched/missing explicit
target UID before any target access. Generic firmware is still uncalibrated for
this replacement NTC; document the required factory-profile option on rebuild.

## Expected files

- Firmware/tools/ntc_factory_profile.py and meaningful generator/C-runtime tests.
- Firmware/config/units/<UID>-ntc-engineering.json.
- Firmware/cmake/modules/FactoryNtc.cmake, CMakeLists and build-config metadata.
- Firmware/src/hardware/hw_power.c: optional generated table, unchanged conversion.
- Firmware/tools/pc_firmware_deploy.py: profile provenance and UID binding.
- Firmware/tests/CMakeLists.txt and relevant deployment tests.
- Build/deployment instructions and bench evidence, including uncertainty.

## Validation and delivery

1. Validate finite inputs, monotonic raw response, reference separation, UID,
   unqualified status and float32-monotonic table generation.
2. Test actual C interpolation at the supplied anchors and default fallback;
   reject invalid profile data and wrong-unit deployment before target access.
3. Run host tests and all nine embedded profiles with strict warnings, preserving
   C8 memory gates and safety invariants. Bind final artifacts to committed source.
4. Read fresh SAFE/idle/battery state, program only internal Flash, read back,
   re-identify and verify plausible NTC/battery status. Preserve external W25Q.
5. Save/push profile, sources, builds and evidence. Do not mark OSL installed:
   three high-Z/10-kHz fits remain blocked. Preserve all original observations.

Independent thermal validation, thermometer uncertainty, a third held-out
temperature, broader NTC curve qualification and RLC blocked-key resolution remain
REQUIRES_BENCH_VALIDATION. No owner override is represented as qualification.

## Implementation checkpoint

The optional generator/CMake path, fixed-size table selection and schema-2
deployment binding are implemented. Default host tests passed 46/46, including
the actual C anchor conversion. Generator tests reject invalid/qualified data;
deployment tests exercise wrong UID before target access and simulated-only
programming. A dirty-source BRINGUP size probe is 65,512 bytes, unchanged from
the prior image; its manifest verified the linked LUT bytes. Final committed
nine-profile builds, physical programming/readback and post-boot verification
are still pending. Do not treat the size probe as the final deployed artifact.
