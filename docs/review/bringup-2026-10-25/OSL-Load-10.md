# First LOAD group — October 25, 2026

Status: LOAD_10_COMPLETE; FIVE_REFERENCE_GROUPS_PENDING; NOT_QUALIFIED.

The owner remeasured the nominal 10-ohm +/-1% resistor: Minipa ET-1100A display
10.5 ohm on the 200-ohm range, then explicitly confirmed direct attachment alone
across TEST_HI/TEST_LO. Printed nominal/tolerance remains the uncertain fit anchor;
the DMM display is consistency evidence. No exact 0.5-ohm lead subtraction or
certified actual resistance is invented.

All three supported 10-ohm conditions (10 Hz / 1 kHz / 10 kHz, 100 mVrms) were
accepted, with six stable repeats, both return channels valid, zero clipping,
observed HG overlap, SAFE teardown and no new protocol errors during capture.
The strict solver accepted all three same-source-7017e5e0 OPEN/SHORT/LOAD triplets
and their float32 fit checks. This is fit consistency, not independent accuracy
validation. There are now 69/99 observations and 3/33 complete conditions.
No complete candidate was generated or installed; active calibration remains blank.

At the owner's request, check battery before every new resistor value. LOW starts
at nominal 3.50 V; request charging if LOW/CRITICAL, and investigate UNKNOWN rather
than starting that group. Charger interlock is unchanged. The pre-10 reading was
3.71824 V, OK. The pre-100 reading was 3.71663 V, OK. Both are fresh SAFE/idle
nominal-ADC readings, not charge-percentage or runtime estimates.

After the LOAD sweep, an unpaced read-only STATUS request timed out before SWD.
A paced read-only reconnect succeeded; protocol counter was 1 before and after
that battery snapshot, fault mask zero and SAFE/idle confirmed. Both attempts
are retained. No measurement START was replayed and no reset was issued.

The owner has been asked to remeasure and attach the 100-ohm reference next.
Other LOADs, independent holdouts, oscilloscope-dependent checks and installation
remain pending. Final leads require calibration in their final geometry.

See [structured checkpoint](OSL-Load-10.json),
[physical evidence](../../../Firmware/releases/2026-10-25/osl-load-10-checkpoint/README.md)
and [the prior endpoint checkpoint](OSL-Same-Image-Endpoints.md).
