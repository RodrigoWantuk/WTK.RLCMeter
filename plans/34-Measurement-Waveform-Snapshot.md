# 34 — Measurement waveform snapshot

STATUS: IMPLEMENTED_REQUIRES_BENCH_VALIDATION

## Authorized scope and prerequisites

Implement the owner's approved third result page on the existing C17 product,
after plans 31–33. Preserve safety, calibration, settings schema 5, the splash
payload/orientation and its three-second completed-image hold. No new DUT capture
is authorized by this plan: reuse accepted statistical repeat blocks.

## Implementation sequence

1. Add pure waveform preparation, two application-owned static slots and tests.
   Preserve 256 real ADC codes per channel. Select simultaneous ranks EXC1/RET1X
   or EXC2/RETHG; retain affine ADC scales and the corresponding mean VMID.
2. Stage only an accepted statistical repeat before acknowledging its block.
   Publish only after successful seven-sample aggregation and SAFE cleanup;
   bind session, block, condition and factory/lead calibration sequences.
   Invalidate on new measurement, failure, cancellation or calibration change.
3. Add bidirectional three-page navigation, a read-only snapshot reference and
   a cooperative column renderer. Rotate to 3 (320x240), restore 2 on every exit.
   Use yellow EXC/cyan RET, independent 1–2–5 mV/div scales with 10% headroom,
   eight time divisions, six voltage divisions and five signed time labels.
   Trigger both channels together on a positive EXC zero crossing near the
   block center. Label physical ADC phase separately from impedance phase.
4. Append text IDs 0x008F–0x0096 (EN/PT-BR), resource API 7, 150 labels per
   language. Keep images unchanged and snapshots volatile.
5. Centralize the explicit unit128 RAM ceiling at 19 KiB including at least
   2 KiB reserved stack, target 18 KiB, preserve 1 KiB physical headroom. Keep
   generic/diagnostic gates unchanged. Incremental static allocation <=2304 B.
6. Run pure/session/UI tests, real-resource/fallback previews, Python suite,
   embedded profile matrix and largest unit128 supplementary-curve profile.
   Inspect maps and stack evidence. Deploy firmware/resources with readback,
   preserve mutable Flash, archive evidence and update the existing PR.

Expected files: measurement waveform module, application waveform store/session/
product/shell, dedicated UI renderer/product view, geometry-aware preview mocks,
resource catalog/fonts, memory tooling, tests and relevant documentation.

## Acceptance and evidence

- Quantized 10 Hz/1 kHz/10 kHz signals, unequal amplitudes, both rank pairs,
  positive/negative/zero phase, trigger failure and clipping.
- Rejected repeats cannot replace accepted candidates; earlier autorange winner
  is followed by matching final-condition repeats; cleanup failure, cancellation,
  calibration mismatch and new sessions cannot publish stale arrays.
- Quiet pauses all drawing/rotation; generation cancels obsolete drawing; each
  graphics step transfers at most 64 pixels. No framebuffer/heap/new DMA/ISR UI.
- Validate portrait/landscape navigation, fault/menu exits, SPI failures, all
  bounds and EN/PT-BR labels using real assets and fallback.
- Software and deployment evidence will be recorded here as completed. Physical
  appearance, timing, amplitude/phase and resistor/103J100/470 uH checks remain
  REQUIRES_BENCH_VALIDATION until exercised with the owner. Stack high-water must
  leave >=512 B in exercised diagnostic scenarios; unexercised paths cannot be
  claimed qualified.


## Implemented and delivered evidence (2026-10-28)

Production source 3f9e1a65ca0a and the matched API 7 resources are installed.
See [the delivery](../Firmware/releases/2026-10-28/waveform-snapshot/README.md)
for sealed artifacts, exact readbacks and 266 indexed evidence files.

- 67/67 host entries, 532 Python cases (38 optional skips), four provisioning-OFF
  focused entries and 28 strict EN/PT-BR real-resource/fallback previews pass.
- Nine supported embedded profiles pass, including supplementary curves and
  stack instrumentation; generic full product stops at its expected 64-KiB gate.
- Default Flash/RAM/stack: 79,644/17,496/2,048 B. Incremental accounted RAM:
  2,208 B. Largest optional-curve RAM: 19,328 B, below the 19-KiB hard gate.
- Actual diagnostic stack observation first failed the requested margin at
  252 B. Separating initialization/control/render/serial temporary lifetimes
  under LTO recovered 940 B after boot and 908 B after resource update, without
  increasing static storage or reducing stack. The failed checkpoint is retained.
- Complete internal programming/readback and all 911 resource ACKs pass. Full
  external pack readback, both prior image payload identities and the entire
  fresh 40-KiB mutable tail match exactly. OSL sequence 1/all 33, lead slots,
  schema-5 settings and compiled experimental NTC identity are preserved.
- Final boot is READY/SAFE, fault-free, calibration active, settings loaded and
  zero acquisition sessions. No DUT capture, serial OK or new LOAD campaign ran.

The foreground-only stack correction follows the host-checked source without
changing pure/session/product/UI logic or assets. Its full embedded matrix and
actual diagnostic boot/update/high-water observations are included. Initialization
IO records copy their callbacks/context, so returning from initialization does
not retain pointers to its stack objects.

Remaining REQUIRES_BENCH_VALIDATION: resistor, 103J100 and 470-uH appearance,
voltage/phase sign, axes, timing, navigation, actual session-to-graph/SAFE behavior,
and >=512-B diagnostic stack margin during acquisition, waveform rendering, menus
and broader faults. The owner was absent with UART occupying the display connector.
Software and installation are complete; those physical acceptance criteria remain
open. No numeric battery reading was made through SWD.

Plan clarifications: generic 64-KiB and BRINGUP profiles retain their old gates and
do not allocate sample slots; the instrumented unit128 PRODUCT does allocate them.
Snapshot-only validation rejects nonfinite/implausible ADC-plane coordinates before
rasterization, leaving a valid numeric result untouched. No safety, calibration
math, measurement permission, splash or settings-schema deviation was required.
