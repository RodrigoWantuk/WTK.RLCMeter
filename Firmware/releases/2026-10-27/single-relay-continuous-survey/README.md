# Single-relay continuous factory survey

Source: 40a0b420ef60003d302b5a8a79c3d53d8c097fc7.
Unit: 54ff70068370525760380567. Software checks, internal programming/readback
and clean READY/SAFE boot pass. A new owner-triggered measurement and physical
single-engagement/release witness remain REQUIRES_BENCH_VALIDATION.

Factory PRODUCT keeps K1 connected across the complete 1 kHz -> 10 kHz -> 10 Hz
search and seven-valid-sample primary estimate. This supersedes plan 26's repeated
SAFE/re-entry segments. One fresh initial residual/power permit is consumed;
neutral guarded range changes and final SAFE cleanup remain mandatory. The
seven-point estimator still trims two observations from each tail independently
for R and X and averages the three retained coordinates. Derived C/L uses that
complex mean at the selected primary frequency. No resistor campaign is repeated.

The continuous owner admits at most twelve search and eight extra statistical
captures within 30,000 ms; the outer app bound stays 60 s. The 1,000-ms battery
evidence limit is unchanged. About every 250 ms outside coherent DMA, the owner
restores the auxiliary ADC, collects four VMID/four BAT conversions and reacquires
the same metrology profile. Both handoffs and sampling have a combined 100-ms
deadline. Actual validated sample timestamps renew evidence. The source timer,
K1, ranges, quiet/workspace lease and original sine-settling deadline remain
owned. No concurrent power/metrology acquisition or fabricated residual/permit
evidence is allowed. Power/VMID/ADC/timestamp failure and existing charger,
fault, DMA, timeout, ownership and cancellation checks abort toward SAFE.

Host tests pass **49/49 factory and 47/47 standard**, including the actual
auxiliary FSM, four-sample averages, DMA exclusion, preserved low-frequency
settling, wraparound, capture budgets, condition locks and 21 failure variants.
All nine strict UID-bound embedded profiles pass. Legacy opt-out profiles retain
their existing Flash/RAM footprints and fixed-capture contracts. No BSP register,
DMA topology, coherent profile, DSP/calibration equation, persistent format,
OSL coefficient or experimental unit NTC constant changes are introduced.

The installed image is **61,136 B Flash / 17,104 B accounted RAM**, including
the 2,052-B stack reservation. Its verified SHA-256 is
`b51558edced7660528ae6dc9b898e0242e422ef57af8f013e44f9aa4575343a6`.
The unchanged hard budgets pass; preferred RAM remains exceeded by the existing
twelve-record traceability design. See [all profile sizes](memory-table.md).
COM4 captured exactly one SAFE_BOOT. Live nonhalting DAP checks found READY,
zero sessions/captures, K1 SAFE, range/excitation OFF, quiet/workspace free,
no fault and active OSL sequence 1/all 33 records. No external-Flash erase/write,
numeric battery read via SWD or remote DUT measurement was requested.

The live factory-size register at **0x1FFFF7E0 returned 0x0040: 64 KiB**.
The programmer independently reported the same geometry. This matches the
[ST C8 device specification](https://www.st.com/resource/en/datasheet/stm32f103c8.pdf).
The current image uses 93.286% of 65,536 B, leaving **4,400 B** physically within
the declared capacity and **3,376 B** below the existing product hard budget.
Two read-only probes beyond 64 KiB returned all FF; this does not prove writable
128-KiB capacity. No erase/program test outside the guaranteed range was done.
The earlier capacity snapshot matched the preceding 60,136-B image; current
usage is independently updated from the new verified deployment.

The archives contain all nine images/maps/configurations, guarded deployment,
complete readback, passive UART boot, live metadata, capacity evidence, host
logs, build matrix and the exact procedures. Checksums are in
[checkpoint.json](checkpoint.json); capacity details are in
[flash-capacity.json](flash-capacity.json). The result reader is prepared for a
future owner-triggered test; its presence is not evidence that a new C/L capture
or power refresh occurred. Boot correctly has zero refreshes and no automatic
measurement. Previous capacitor/inductor observations remain in the October 26
delivery and are not relabeled as validation of this image. Independent accuracy,
polarized envelope, actual 10-mF endpoint, contact/abort timing, stack high-water
and final TFT appearance remain pending.
