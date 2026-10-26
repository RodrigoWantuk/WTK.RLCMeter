# Bounded low-impedance automatic search

Source 80193b99e6ae2edc440e24c10512f4e6e515963b adds a supporting 10-kHz attempt
after an apparently resistive 1-kHz result on RREF 10/100 ohm with a usable,
unclipped selected RET path. Existing ratio-based range/SNR/clipping steps run
first. The amplitude and RREF are retained for the frequency check. Finite
negative estimated R may also be explored; unchanged publication guards still
reject unsupported negative resistance/loss. No R/C/L threshold is reduced.

The policy retains six attempts, four range transitions, one frequency
refinement, no repeated conditions and the forbidden 10-ohm/500-mV rule.
Hard OPEN/SHORT, invalid DSP, safety abort and cancellation retain priority.
Each capture still uses a fresh single-use safety permit and returns K1 SAFE.
Both attempts remain in history; contradictory resistive/reactive evidence
can finish MIXED/UNKNOWN, and the secondary does not acquire special primary
priority simply because it is reactive. Quality-based primary selection stays.

All 46 standard and 47 factory host CTests pass with explicit Debug configuration.
The regression includes the actual pre-change 0.47-uH R/X pair, zero-reactance
resistor, conflicting reactive evidence, negative estimated R, selected-path
exclusions, terminal priorities and the global bounded sequence. All nine
strict UID-bound embedded profiles pass. The experimental unit NTC profile is
preserved. No acquisition, ADC, hardware, calibration model or coefficient is
changed, and no resistor campaign is repeated.

PRODUCT is 54,340 bytes, fully programmed/readback verified and booted READY.
OSL sequence 1/all 33 records remain active, resources OK, K1 SAFE, range and
excitation OFF, no faults. TFT normal-clock policy and 75% saved brightness
are retained. Only internal MCU Flash was programmed; W25Q calibration/assets
were not erased or rewritten. Battery voltage was not read via SWD.

The first diagnostic build exceeded physical Flash by 12 bytes. Reviewed
compaction of this frequency policy plus removing the unnecessary R-sign search
filter brought final full BRINGUP to 65,528 bytes, only 8 bytes below silicon
capacity. This is a maintenance constraint, not extra available Flash. Other
profiles and all size gates pass; failed/intermediate builds are not delivered.

Physical extra-frequency capture on the same nominal 0.47-uH part is pending
an owner-triggered Click. The earlier unsuccessful observation is preserved
in ../inductor-047-limit-check. Extra search does not establish a hardware
lower limit or sub-microhenry accuracy. Independent phase/parasitic qualification
remains REQUIRES_BENCH_VALIDATION.

The owner's proposed single K1 activation for a whole automatic session is
reviewed but not implemented here. Current residual/ADC requalification and
single-use permits assume SAFE between captures. Session-wide ownership,
intentional range transitions and emergency cleanup require a separate reviewed
safety contract. No consumed permit is reused or cleanup skipped as a shortcut.

See firmware-builds.json, checkpoint.json, firmware-images.zip and
deployment-evidence.zip for exact identities, size reports, passive UART boot,
compiler-derived metadata and bounded deployment procedures.
