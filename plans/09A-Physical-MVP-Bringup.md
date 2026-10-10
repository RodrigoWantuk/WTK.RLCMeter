# Phase 09A: Physical MVP Bring-up

STATUS: IMPLEMENTED_REQUIRES_BENCH_VALIDATION

## B01.0 first-startup host toolkit — 2026-10-10

[Executable read-only evidence collection](../Firmware/tools/PC_BRINGUP.md)
supports offline preparation, manual provenance, BRINGUP serial snapshots and
deterministic simulated replay. It requires a passive BRINGUP identity banner and
uses only nine audited status/info commands. PRODUCT/BRINGUP_CAL text targets,
active commands and unknown identity are refused. R02, R05 and A05-Y04 remain
explicit pending physical gates. No firmware or PCB change is introduced.
See [test and memory evidence](../docs/review/b01/README.md). B01 physical safe
boot, contact/GPIO behavior and electrical qualification remain unexecuted.

Phase 09A freezes feature expansion and turns the existing firmware into a bench-usable Rev.1 MVP. It does not qualify accuracy, finish visual polish, add Live mode, or start later UI/resource work. Its purpose is to let a freshly assembled Rev.1 board boot safely, expose a compact readiness snapshot, run the real automatic measurement chain from UART, and support the first known passive DUT measurements.

## Implemented Software Boundary

- BRINGUP exposes `lab measure auto`, which reuses `app_measurement_session_t`, `measurement_auto_session_t`, the Phase 05 fixed-condition hardware transaction, Phase 06 DSP, Phase 07 autorange/confidence/classification, and calibration resolution.
- Each automatic attempt still traverses the Phase 05 safety transaction: permit issue/validation, range preparation, K1 ownership, excitation, ADC1/ADC2 DMA capture, return to SAFE, auxiliary ADC restore, and raw-block publication.
- BRINGUP automatic processing allows ideal calibration fallback when no persisted condition exists. UART output reports this as `calibration=IDEAL_UNQUALIFIED` with the calibration resolve status.
- PRODUCT processing still calls calibration resolution without ideal fallback; missing or invalid active calibration remains a normal PRODUCT measurement blocker.
- BRINGUP exposes `lab mvp status` as a compact readiness snapshot over existing services.
- PRODUCT no longer treats missing/corrupt external visual resources as an operational blocker. It keeps safety/calibration blockers intact and uses internal fallback text/rendering for core screens.

## Intentionally Frozen

These remain deferred and must not be pulled into 09A:

- Stage 3B.2 fonts/icons/splash/graphs;
- final typography or animation polish;
- TFT debug console page;
- new localization features;
- Live continuous measurement UX;
- new frequencies, models, calibration domains, or Rev.2 hardware;
- virtual-hardware expansion unrelated to a real regression.

## BRINGUP Commands

```text
lab mvp status
```

Reports clock source/frequency, Flash detection, display readiness, charger state, safety blocker, K1 command state, range state, calibration service state, sensor summaries, resource relevance, and last metrology error.

```text
lab measure auto
```

Runs one click-style automatic measurement session through the real Phase 05/06/07 path. Output includes `AUTO_BEGIN`, `ATTEMPT_BEGIN`, optional `PARTIAL_RESULT`, `AUTO_RESULT`, and `AUTO_END`.

```text
lab measure cancel
```

Requests cancellation of an active automatic BRINGUP measurement. If a hardware transaction is active, cancellation uses the Phase 05 abort/safe cleanup path.

Existing fixed-condition commands remain available:

```text
lab metrology capture <freq> <amp> <range>
lab metrology measure <freq> <amp> <range>
lab cal ...
```

## Automatic Result Format

`AUTO_RESULT` is one parseable line:

```text
AUTO_RESULT status=<auto_status> class=<interpretation> primary_attempt=<index> attempts=<n> value_kind=<R|C|L|Z> value=<float> r_ohm=<float> x_ohm=<float> z_ohm=<float> phase_deg=<float> freq_hz=<hz> amp_mvrms=<mV> range=<range> channel=<RET_1X|RET_HG> calibration=<PERSISTED|IDEAL_UNQUALIFIED|NONE> cal_status=<resolve_status> quality=<quality> qualification=<qualification> confidence=<confidence> reason_flags=<hex>
```

The first-board expectation before real OSL calibration is `calibration=IDEAL_UNQUALIFIED`. That is useful for gross hardware debugging only; it is not accuracy evidence.

## Flash/RAM Policy

PRODUCT now enforces a 60 KiB project hard Flash gate during the final integration stretch. BRINGUP enforces the physical 64 KiB STM32F103C8T6 Flash limit plus the existing 18 KiB accounted-RAM hard gate. This is deliberate: the bench image now links the automatic measurement engine and diagnostic output so the first board can measure through the same high-level path as PRODUCT.

The BRINGUP image is not a product release artifact. If PRODUCT exceeds its hard gate, defer cosmetic/UI/resource functionality before touching safety, calibration, acquisition, DSP, autorange, or basic UI.

## Bench Validation Status

Prepared but not executed:

- K1 SAFE at boot and after every failure path;
- RANGE_EN disabled at boot and during range-address changes;
- excitation inactive at boot/fault and valid at 100 Hz, 1 kHz, and 10 kHz;
- ADC1/ADC2 DMA timing and channel packing;
- automatic BRINGUP measurement on known resistor, capacitor, inductor, OPEN, and SHORT;
- real 33-condition OSL calibration;
- PRODUCT calibrated measurement after persisted calibration is active.

## First Board Sequence

Use [`../docs/16-Rev1-MVP-Bench-Guide.md`](../docs/16-Rev1-MVP-Bench-Guide.md) for the step-by-step procedure.
