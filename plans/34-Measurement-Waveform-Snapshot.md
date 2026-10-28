# 34 — Measurement waveform snapshot

STATUS: IN_PROGRESS

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
