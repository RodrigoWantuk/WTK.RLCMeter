# C03.1 — Factory PRODUCT AC results

Status: **IMPLEMENTED_TESTED_HOST / COMPILES_TARGET / REQUIRES_BENCH_VALIDATION**.
The PCB remains unassembled. This change does not authorize assembly, power-on,
accuracy qualification or retirement of the legacy calibration wizard.

Starting main: `607bfef43b282f7a954f9eba07cdc7a88f6bf32f`; PR #14 was verified
merged before implementation. Implementation measured at
`ad405e934d69d0287482d605ef80a5379dc353df`, branch `codex/c03-ac-results-ui`.
Tools: Arm GCC 14.2.1 (20241119), binutils 2.43.1.20241119, CMake 4.4.4,
Ninja 1.13.2.git.kitware.jobserver-pipe-1, MSVC 19.44.35228, Python 3.9.5.

## Implemented behavior

Only `WTK_PRODUCT_FACTORY_PROVISIONED=ON` receives the new result view and
interaction rules. Legacy PRODUCT remains the default, with its full wizard.
Previously Q/D and applicability/provenance were dropped between DSP and UI;
previous snapshots could remain available during subsequent or interrupted work.
The factory view now carries these fields and session/OSL sequence, selected
RREF, return path, confidence and attempt count. It consumes existing DSP results
and the existing multi-frequency model classifier; it does not fit calibration.

| Quantity | Publication / display |
| --- | --- |
| R and X (`R + jX`) | Separate details rows; finite valid complex impedance, R >= 0 |
| Magnitude and phase | Details; magnitude >= 0, finite; phase within [-pi, pi] |
| Resistance / series loss | Primary for a resistive model; `R(AC)` in details |
| C | Primary only for the accepted capacitive model, X < 0, valid finite positive C |
| L | Primary only for the accepted inductive model, X > 0, valid finite positive L |
| ESR | Details only for valid capacitive series C; explicitly `ESR(AC)` |
| Q and D | Details only for accepted reactive models, positive loss and valid finite positive DSP derivations |
| Frequency / excitation | Actual selected primary-attempt profile, not a guessed condition or a measured amplitude certificate |
| Error characterization | Explicit status; no numerical accuracy bound exists |

All numerical publication also requires successful final/partial status,
non-rejected mathematical confidence, usable/calibrated/unclipped selected return,
successful acquisition/DSP, admissible condition, persisted compatible OSL and
matching condition/session/calibration identities. A clipped unused HG path can
retain good 1X evidence; both unusable paths cannot publish values. Mixed/ambiguous
impedance does not gain C/L/Q/D merely because individual formulas are finite.
Negative loss invalidates numerical publication. Unavailable values display `n/a`.
OPEN/SHORT-like classifications report a range limit rather than precision values.
No guard threshold, excitation timing, sampling order, safety permission or OSL
coefficient/schema/model was changed.

The factory phase helper fixes a discovered numerical defect: the existing atan
polynomial was evaluated at arbitrary `y/x`, outside its supported |ratio| <= 1
domain. Complementary-angle/quadrant reduction reuses that polynomial. Forty-nine
deterministic quadrant/axis vectors agree with host `atan2f` within 0.002 rad;
this is a software approximation check, **not a physical error bound**. The legacy
phase path is unchanged by this scoped factory change. Float-to-integer formatting
also rejects nonfinite/overflow input and nonzero values below displayed resolution.

The typed result-level error contract distinguishes `NOT_CHARACTERIZED`,
`QUALIFIED_BOUND_AVAILABLE`, `NOT_APPLICABLE` and `INVALID_RESULT`. The qualified
state is reserved: no evidence reader or persistent error format is invented.
Even an injected reserved status cannot create a numerical error claim. Valid
results show `Max error: / not characterized` or
`Erro max.: / nao caracterizado`, together with an explicit AC provisional label.
OSL correction and signal confidence are not accuracy certificates or DC winding R.

`BOOT -> READY -> OK -> MEASURING -> RESULT -> DETAILS -> READY` is exercised
through the actual C controller and engine. UP/DOWN wrap primary/details; short OK
in details returns READY without acquisition. Short OK on READY/primary starts one
measurement; during acquisition it cancels. Partial values remain visibly PARTIAL.
New starts clear the prior view. Cancellation, charger/residual denial, faults,
resource failure and calibration revocation/replacement preempt publication and
use existing cooperative cancellation/teardown. DMA ownership is released by its
hardware owner. Boot does not start measurement. Invalid/missing prerequisites
retain existing calibration/resource recovery gates.

The primary page shows model/value, selected condition, confidence/provisional
status and unknown error. Details show R/X/Z/phase, conditional ESR/Q/D, condition
and range/path/attempt count. No framebuffer or additional font/image asset was
added. The existing external large numeric face has an unusable Omega shape, so
primary resistance reuses enlarged emergency text. Emergency rendering spells SI
prefixes and units out to avoid the font's uppercase m/M ambiguity; the comma is
derived from its existing dot. PT-BR decimal commas and readable units survive
missing fonts. All Resource Pack text IDs, payloads and wire contracts are unchanged.

## Reproducible validation

```powershell
cmake --preset stm32-release -S Firmware -B Firmware/build/c031-factory `
  -DWTK_PRODUCT_FACTORY_PROVISIONED=ON -DWTK_ENABLE_SUPPLEMENTARY_CURVES=OFF
cmake --build Firmware/build/c031-factory
python Firmware/tools/product_factory_matrix.py --host --out Firmware/build/c031-host
python Firmware/tools/product_factory_matrix.py --out Firmware/build/c031-final
python Firmware/tools/verify_product_ac_ui.py `
  --controller Firmware/build/c031-host/host-debug-factory-ON-curves-OFF/tests/Debug/wtk_app_product_test.exe `
  --preview Firmware/build/c031-host/host-debug-factory-ON-curves-OFF/tests/Debug/wtk_ui_product_preview.exe `
  --out Firmware/build/c031-views
```

Host Debug/Release × legacy/factory × curves OFF/ON: legacy CTest **40/43** and
factory **41/44**, **336 successful test invocations**, zero final failures.
Full Python suite with the C fixture active: **203 tests per configuration**,
1624 successful invocations, zero skips/failures. Golden vectors, PC/C solving,
calibration application, W25Q A/B/replay/interruptions and Resource Pack regression
remain included. Existing installation tests sweep 66 C store transition points
and 125 partial-NOR cuts.
Hardware audit/document reconciliation and its 17 tests also pass without PCB edits.

New scenarios cover R/C/L, mixed impedance, low ESR, near-zero X, negative loss,
high Z/OPEN, near-SHORT, clipped HG with usable 1X, both clipped, NaN/overflow,
invalid individual derived quantities/provenance, zero loss, failed consecutive
capture, partial cancellation, charger/residual interruption, OSL loss/replacement
and resource corruption. Existing missing/incompatible OSL boot tests remain.
Known DUT fixtures mock acquisition at the existing interface, derive through real
C DSP and drive the real engine/controller. A separate actual ADC/DSP fixture tests
clipped-HG fallback. The existing C serial installation-to-PRODUCT runtime test
continues to exercise persisted coefficients and real calibration acceptance.

The new CTest renders 19 native controller snapshots through actual UI/font/
format/fallback code: **76 EN/PT-BR × resource/fallback images** per factory host
configuration, bounded to 240×320. Exact text tests verify units, locale, Q/D,
ESR, partial and unknown-error labels. Fresh versus refreshed pixels match with
and without resources. Native snapshots are ephemeral same-build test data,
not a new persistent/device format. Raw maps/logs/views remain ignored build output.

All **12 ARM builds** pass size and source/symbol composition gates: PRODUCT
Debug/Release × legacy/factory × curves OFF/ON, BRINGUP and BRINGUP_CAL OFF/ON.
Wokwi file checks, custom-chip compilation and CLI lint pass; smoke scenarios were
attempted but **not executed because WOKWI_CLI_TOKEN is absent**. Static checks
and synthetic host VCD fixtures do not constitute scenario or physical execution.
PR #15's hosted Flash-forensics and Virtual-Hardware jobs did not start: both
annotations report **account locked due to a billing issue**. This external CI
blocker is separate from local code/test results and the absent local Wokwi token.

## Linked memory evidence

Fresh before/after measurements are recorded in [sizes.csv](sizes.csv). A01
attribution and the existing physical/project gates run for every ARM image.

| Image / curves | Before Flash / RAM B | After Flash / RAM B | Flash delta B |
| --- | ---: | ---: | ---: |
| Legacy Debug OFF | 58,712 / 14,536 | 58,700 / 14,536 | -12 |
| Legacy Debug ON | 61,320 / 14,600 | 61,312 / 14,600 | -8 |
| Legacy Release OFF | 62,052 / 14,984 | 62,044 / 14,984 | -8 |
| Legacy Release ON | 64,476 / 15,048 | 64,468 / 15,048 | -8 |
| Factory Debug OFF | 47,584 / 14,784 | 51,728 / 14,832 | +4144 |
| Factory Debug ON | 50,080 / 14,840 | 54,208 / 14,896 | +4128 |
| Factory Release OFF | 48,496 / 14,984 | 52,648 / 15,040 | +4152 |
| Factory Release ON | 50,992 / 15,048 | 55,124 / 15,104 | +4132 |
| BRINGUP OFF/ON | 65,448 / 14,596 | unchanged | 0 |
| BRINGUP_CAL OFF/ON | 31,944 / 12,744 | unchanged | 0 |

Legacy small layout/LTO differences are measured, not a Flash-cleanup claim.
Factory Release has **12,888 B OFF / 10,412 B ON** physical Flash headroom;
the stricter unchanged 64,512-B PRODUCT gate leaves 11,864/9388 B. Accounted RAM
increases 56 B in Release; context/view growth adds no DMA/staging buffer.
Shared workspace remains 3072 B + 4-B owner and calibration service 5776 B.
PRODUCT context is 1600 B (+24), UI context 368 B (+28). No heap or float printf
was added. Reserved stack remains 2052 B factory/Release and 2048 B BRINGUP.
Largest factory Release OFF individual frame is 480 B (`app_step`, previously
424); these are compiler frames, not call-chain maxima or measured high water.

Largest final Flash symbols include `app_step` 8668 B, `app_shell_run` 5052 B,
`prepare_line` 3556 B, acquisition processing 2642 B and acquisition stepping
1848 B. The remaining mandatory software estimates in the A06 feature budget
must not charge this measured result/status/lifecycle implementation again;
bench-discovered safety corrections and engineering contingency remain estimates.
A physically qualified error-bound source needs B04 evidence and a separately
approved compatible reader contract; it is not required to display unknown status.
The updated [remaining feature budget](../a06/A06.2c-feature-budget.csv) now removes
the completed result/lifecycle estimates. Still-open MUST allowances total
1664/3200/6784 B optimistic/central/conservative, including qualified-bound reader,
bench corrections, integration reserve and contingency. These are estimates, not
linked implementations: conservative factory ON would reach 61,908 B, leaving
3628 B silicon / 2604 B project-gate margin. Re-measure as approved work lands.

## Physical acceptance still open

- Close A05 Blue Pill/header equivalence, PB0 backlight load, buzzer drive and
  charger/battery/boost/power-sequencing inspection before controlled assembly/power.
- Measure 5-V MCP6002 saturation and BAT54S current into 3.3 V; firmware cannot
  eliminate this unmeasured electrical effect. Keep the fixed PCB and approved
  K2/driver DNP, R0_BANK/buffer bypass population and TVS DNP decisions.
- Verify safe boot, charger/residual permissions, K1/range disable-before-address,
  cancellation/fault teardown, DMA timing, quiet behavior, watchdog and stack high water.
- Run real BRINGUP_CAL captures/install/readback/reset and safe W25Q interruption
  tests; deploy PRODUCT via SWD and confirm resource/calibration independence.
- Verify independent R/C/L holdouts, repeatability, contact/probe/leakage/HG/clipping
  limitations and actual primary conditions. Thirty-three OSL records do not prove
  thirty-three physically usable conditions or absolute accuracy.

Next smallest step: resolve the module/backlight/power inspection gates, then perform
the ordered current-limited digital bring-up in A05 before any passive OSL fixture
capture. The wizard retirement and numerical accuracy qualification still need
real-board evidence and owner approval.
