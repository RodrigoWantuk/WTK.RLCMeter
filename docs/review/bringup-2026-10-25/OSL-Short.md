# First SHORT checkpoint — October 25, 2026

Status: SHORT_33_ACCEPTED; THREE_PRELOAD_PATH_CONFLICTS; NO_CALIBRATION_INSTALLED.

The owner attached the requested direct-terminal jumper; its length was not
supplied. All 33 SHORT captures on source 063c7378319f293640d6d19d5fadcb3fa3ad686f
were accepted with six stable blocks and SAFE teardown. No new protocol error
occurred; the earlier count of one is retained. No candidate or installation
was attempted. These captures remain engineering evidence, not qualification.

The unchanged solver requires a common accepted channel for OPEN/SHORT/LOAD.
Three OPEN/SHORT pairs lack one: 1 kOhm / 10 kHz / 500 mVrms and 1 MOhm /
10 kHz / both amplitudes. A LOAD cannot repair the existing missing pairwise
intersection. The [raw checkpoint](OSL-Short.json) preserves these flags honestly.
No artifact is relabelled, and no mixed-channel workaround is admitted.

The calibration workflow applies the ordinary minimum nonzero return amplitude
to a known SHORT. A regression using a real quantized zero-return block fails
before correction. A zero standard must observe zero/noise without demanding a
nonzero DUT signal. The bounded fix changes that minimum only for SHORT; real
excitation, denominator, finiteness, clipping, repeatability and all electrical
permits remain required. HG gain overlap still requires the original nonzero
signal thresholds on both channels, so zero noise does not establish gain.

Source identity changes require a new campaign. Preserve all 66 earlier captures
and reacquire OPEN/SHORT with the corrected image; do not combine their device
identities or fabricate quality flags. LOAD and independent holdouts remain
pending, as do Hantek-dependent checks. The current source-defined equations,
ADC timing, excitation classes, hardware identity and storage model are unchanged.

See [physical evidence bundle](../../../Firmware/releases/2026-10-25/osl-short-checkpoint/README.md).
