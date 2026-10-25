# Coherent calibration pilot — first unit

Source a52dba35c74e0643a8b15d916eb29c427d917f77 adds bounded coherent groups only
to the CAL profile and host tests. PRODUCT and ordinary BRINGUP compile out the
integration state. All46 host tests and nine strict UID-bound NTC builds pass.
CAL is37,424 bytes Flash /13,740 bytes accounted RAM; the service context is1072
bytes. The original3072-byte DMA workspace and hardware safety transactions remain.

`bringup-cal.zip` contains the guarded schema2 deployment package, NTC metadata,
map/HEX and build/test logs. It is bound to UID54ff70068370525760380567. The BIN
was programmed and byte-for-byte verified, then COM4 identified a52dba35 with
capabilities127. Boot battery was3.926 V by serial, SAFE/idle and fault/protocol
counters zero; active OSL remains blank. No W25Q write was requested.

New CCO2 artifacts identify physical blocks/group and actual physical attempts;
old CCO1 clients reject the negotiated format. Integrated keys use a bounded
90-second global capture deadline while ordinary captures retain20 seconds.
Every constituent physical transaction retains its original permit, hardware
deadline, relay guards, residual/charger/battery checks and SAFE teardown.

The three OPEN pilot keys pass the original gates with actual1X/HG validity
and observed HG overlap. They contain96,576,96 accepted physical blocks,
respectively, with zero rejected groups or clipping. Final pilot battery is
3.931 V over COM4, SAFE/idle, zero safety faults and protocol errors.
`physical-evidence.zip` preserves programming/readback, boot, all three raw CCO2
artifacts, UART evidence and the bounded pilot script. Their exact bytes seed
the new source-a52dba35 OPEN cohort.

All33 OPEN keys are now accepted, totaling198 groups and948 accepted physical
blocks/attempts. `open-complete.zip` preserves the exact three pilot seeds and
30 new observations with raw CCO2 bytes, UART evidence and the complete33-key
capture document. `open-checkpoint.json` records hashes, counts and final serial
status: battery3.920 V, SAFE/idle, fault/protocol0, charger absent, active OSL0.
Clipped HG streams remain excluded when 1X supplies the valid OPEN evidence.
The owner confirmed the short copper jumper and all 33 same-image SHORT keys
also completed: 198 accepted groups, 948 accepted physical blocks/attempts,
no rejected groups or clipping. Both SHORT return paths are valid; no SHORT
artifact claims observed HG gain from its zero-return endpoint. Every OPEN/SHORT
pair has an actual usable path in common. There are now 66 source-a52dba35
endpoint observations; the original 33 OPEN artifacts remain unchanged.

`short-complete.zip` contains all SHORT artifacts, the complete 66-observation
document, UART evidence, owner confirmation and the bounded script.
`short-checkpoint.json` adds the per-key endpoint-path audit. Battery was 3.915 V
at completion. A later fresh COM4 read, preserved in `power-before-load-10.json`,
reports 3.889 V, SAFE/idle, fault/protocol counters zero and charger absent.
The owner was asked to freshly measure and attach only the approximately 10-ohm
reference after removing the jumper. Same-image LOADs, strict fitting and
installation remain pending; no W25Q calibration record has been written.

The owner next reported 10.4 ohm and direct attachment alone. `load-10.zip` and
`load-10-checkpoint.json` preserve three accepted current-source LOADs and their
strict serialized-float32 fits: 69 current observations, three complete current
conditions. Printed 10 ohm +/-1% remains the uncertain anchor; the Minipa reading
is a consistency check without automatic subtraction of the earlier 0.5-ohm
shorted-lead reading. Fresh pre-group battery was 3.884 V, post-group 3.887 V.

After the owner objected to repeated fixture exchanges, `condition-reuse-audit.json`
documents compatible reuse of 18 complete original triplets for 100 ohm/1 kohm/
10 kohm, with original source/identity/flags intact. The same UID/hardware/model/
ADC, 91 unchanged critical source files, unchanged workflow math and target
one-block acquisition metadata were checked. The current 10-ohm conditions are
retained; all twelve high-Z conditions will use fresh current-source LOADs.
`power-before-load-100k-reuse.json` records 3.883 V and SAFE/idle before the revised
next-reference request. See [the explicit composition decision](../../../../plans/14D-Condition-Local-Calibration-Reuse.md).
No default campaign verifier, safety threshold or full-set installation gate is
relaxed. Candidate composition, C validation and installation remain pending.

Do not claim completed calibration,
installed OSL or qualified accuracy from these builds. The original99-observation
source-7017e5e0 cohort and all f86b7f4e diagnostics remain unchanged and separate.
See [protocol](../../../tools/PC_CAL_CAPTURE.md) and
[execution plan](../../../../plans/14C-High-Z-Integration-Investigation.md).
