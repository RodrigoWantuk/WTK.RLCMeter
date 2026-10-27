# Production completion software delivery — October 27, 2026

Status: **SOFTWARE_VERIFIED / REQUIRES_BENCH_VALIDATION**. This image is prepared, sealed and archived; **it has not been programmed into the instrument**. The owner is away and reports possible battery depletion. No fresh battery value, boot observation or physical measurement is claimed.

## Implemented behavior

- Settings now distinguish accepted asynchronous work from contention, retain edits made during a save, verify header/payload CRC and recover redundant committed records. Legacy settings remain readable.
- Product navigation, confirmations, save/error/measurement/fault feedback use a bounded cooperative buzzer service with mute and quiet ownership. Idle timeout, brightness/settings application and incremental UI status paths are covered by host tests.
- Fatal-output shutdown and checked watchdog startup retain fail-safe output ordering. Bounded interrupt UART supplies actual battery/NTC/result telemetry and guarded serial OK through the normal product controller.
- Production exposes **Calibration → Compensate leads (OPEN/SHORT)**. Each endpoint captures seven accepted coherent windows at 10 Hz, 1 kHz and 10 kHz. Candidate correction is reviewed before saving; enable/disable/confirmed clear use separate atomic lead records, bound to this UID and the factory OSL sequence/content. No factory LOAD resistor campaign is requested.
- Calibrated complex correction precedes derived R/X/C/L/ESR/Q/D. Unsupported/weak evidence remains unavailable. One production feature contract preserves the three-frequency survey, seven-point estimate and normal continuous relay ownership.
- Optional WCRV now uses a verified boot RAM cache rather than reading W25Q during quiet acquisition. Its sparse 36-position index safely represents all 33 supported keys. WCRV stays **OFF** in the delivered image; combining it with lead correction remains unavailable.

## Sealed image and memory

Extract [firmware.zip](firmware.zip) into a new directory; keep all five files together. The manifest binds the binary, ELF, generated configuration and linker script. Inspect it with `python Firmware/tools/pc_firmware_deploy.py inspect --manifest <extracted>/manifest.json`. This inspection does not access the target. Do not substitute a generic 64-KiB profile or bypass the UID check.

| Item | Value |
| --- | --- |
| Profile | `FACTORY_PRODUCT_UNIT128` / `stm32-unit-release` |
| Image source | `603f6715307c2b11f48b2f0470622139ca951728` (clean) |
| Expected UID | `54ff70068370525760380567` |
| Factory Flash register / explicit engineering capacity | 65,536 / 131,072 bytes |
| Used Flash / physically available remainder | 75,092 / 55,980 bytes |
| Flash hard-budget margin | 54,956 bytes |
| Static RAM + reserved stack | 13,156 + 2,048 = 15,204 bytes |
| RAM remaining after that reserve / hard-budget margin | 5,276 / 2,204 bytes |
| BIN SHA-256 | `0340b3395adb95794e1fc9de41692cb04c0716eeb88d5184bac09965a2f2f170` |
| Manifest seal | `7558170d3733bd9e76431e8b8faa10a77ac803285f84056a6044cc63f5f65c0f` |

The 128-KiB result applies only to the previously tested first MCU. Generic C8 bounds remain 64 KiB. It does not qualify undocumented upper-Flash endurance or temperature behavior. Compiler stack reports have a largest individual frame of 672 bytes; they do not establish transitive/interrupt high-water. Runtime high-water and worst cooperative latency are still unmeasured.

## Verification and traceability

[report.json](report.json) records hashes, memory, provenance, profile results and pending checks. [evidence.zip](evidence.zip) contains an indexed SHA-256 inventory of raw test/build logs, maps, size/stack reports, immutable source fingerprints and explicitly simulated programmer evidence.

- Host CTest: **59/59 factory**, **59/59 standard**, **62/62 optional curves**.
- Python with the real host C capture bridge: **531 cases, zero failures, 38 explicit optional skips** (desktop/runtime/artifact dependencies). An earlier bridge-disabled run with 113 skips is retained and separately labelled.
- Strict embedded matrix: **eight supported profiles pass**, plus the expected generic 64-KiB full-product memory stop. Serial ON/OFF, recovery and the restricted K1/external-supply diagnostics are covered. Those two optional diagnostics direct full factory campaigns to BRINGUP_CAL.
- Optional curves unit build: **77,976 Flash / 17,052 accounted RAM bytes**, within unchanged hard gates; excluded from the delivered image.
- A follow-up test-only commit `11add6a` makes the independent double oracle's float-to-double conversions explicit. All three new math/capture/cache test translation units pass GNU C17 warning-as-error syntax checks; the oracle executes successfully again. This is cross-compiler syntax evidence, not a claim that Linux CI executed locally. No embedded runtime code changed after the sealed image source.
- FakeSWD contract test only: `SIMULATED_VERIFIED`, 74 page erases, exact binary readback, outside-page preservation and wrong-UID rejection; **no USB/OpenOCD/hardware or W25Q access**. Simulation is not physical deployment.

## Preserved data and next physical session

Existing OSL sequence 1/all 33, owner resources/splash and external Flash were not accessed or rewritten. The NTC source is unchanged; compiled semantic profile SHA-256 remains `9b3826a20b3c621a9e92be0035431b140c45d7a23acaba8ec237a63e4e8cad44`. This semantic hash is distinct from the Windows file-byte hash. The previous physically installed image remains the [serial-control baseline](../serial-ok-control/README.md).

When the owner returns: confirm charged stable power and actual UART battery; preserve external records/assets before any migration/profile write; deploy the sealed UID-bound image with page/readback safeguards; check READY/SAFE and real telemetry; validate settings save/reboot, display/buttons/buzzer/idle behavior; then execute final-lead OPEN/SHORT and independent R/C/L holdouts. Physical save interruption, relay/contact/abort timing, quiet interference and stack/latency checks remain pending. Independent accuracy, polarized-capacitor envelope, 10-mF endpoint and thermal/endurance qualification are **REQUIRES_BENCH_VALIDATION**. Do not repeat the six factory LOAD groups for these application checks.
