# 100-ohm LOAD checkpoint — October 25, 2026

Status: LOAD_100_COMPLETE; FOUR_REFERENCE_GROUPS_PENDING; NOT_QUALIFIED.

The owner reported "100R exatos, está conectado" after the requested Minipa
ET-1100A 200-ohm-range reading and direct attachment alone. Displayed 100 ohm is
consistency evidence; nominal 100 ohm +/-1% remains the uncertain fit anchor.
Earlier band-color identification remains provisional; no traceable actual value
or precision beyond the reference evidence is asserted.

Six LOAD keys (10 Hz / 1 kHz / 10 kHz, 100 / 500 mVrms) passed six stable repeats,
SAFE teardown and same-image OPEN/SHORT/LOAD fit checks. Saturated HG channels
were retained as rejected paths; usable 1X evidence supports those fits. The
100-ohm / 10-kHz / 100-mVrms key used HG with actual observed overlap from its
same-key standards. All nine
complete triplets, including the earlier 10-ohm group, passed strict float32
solver checks. Progress is 75/99 observations and 9/33 complete conditions.
This establishes fit consistency, not independent accuracy validation.

Fresh battery before this group: 3.71341 V; before the next 1-kohm group:
3.71502 V. Both are nominal-ADC readings, state OK, from SAFE/idle snapshots
without reset. The owner's per-resistor battery-check policy remains active.
Protocol-error baseline was 1 and did not
increase during this group. Fault mask remains zero and calibration storage blank.
No complete candidate was generated or installed. Four LOAD groups, independent
holdouts and scope-dependent qualification remain pending.

See [structured checkpoint](OSL-Load-100.json),
[physical evidence](../../../Firmware/releases/2026-10-25/osl-load-100-checkpoint/README.md)
and [the previous LOAD checkpoint](OSL-Load-10.md).
