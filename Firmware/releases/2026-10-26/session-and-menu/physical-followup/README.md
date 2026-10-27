# Physical follow-up to the session/menu delivery

This records owner-triggered completed measurements on the verified factory
PRODUCT image SHA-256 1fcf311bad8d9bd2da6a363ca5aeb5751cbdf566970f484d910cd9e9c34a69d0,
before the seven-sample filter. The passive isolated inductor was suspected to
be 470 uH, but its marking was not confirmed. No accuracy qualification follows.

Session 3 reported 466.997 uH at 1 kHz with two internal captures. Session 4
reported 468.831 uH at 1 kHz with four internal captures. Both finished normally,
with calibration sequence 1/all 33 records, K1 SAFE, excitation/ranges OFF,
hardware IDLE, quiet released, workspace free and no hardware fault.

For the single OK click that produced session 4, the owner confirmed one relay
engagement and one release. This is basic physical evidence for continuous
session ownership across four captures. Earlier owner-reported measurements
were overwritten and are not reconstructed or claimed. Detailed switching
transients, phase/accuracy, abort/deadline timing and physical incremental menu
appearance remain REQUIRES_BENCH_VALIDATION. No battery voltage was read via SWD.

checkpoint.json and evidence.zip preserve compiler-derived completed-result
reads, exact running-image checks and postflight metadata. No reset, acquisition
request, debugger state injection or external-Flash write was used for these
reads. The archive contains the original scripts/layouts for this exact image;
do not reuse that ABI against a newer firmware.
