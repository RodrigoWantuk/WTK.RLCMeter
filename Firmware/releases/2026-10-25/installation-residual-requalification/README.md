# First-unit engineering calibration installed

Firmware source: 46df1fbb557a803da7df9683ddfbc2ff124cb1dc.
Unit: 54ff70068370525760380567; hardware 0x00010002; schema 2/model 5.

All 33 complete conditions pass Python, the actual C solver, canonical frame
validation and serialized LOAD-center checks. The explicit composition retains
18 audited complete original triplets and 15 current triplets with original
identities, raw bytes and flags. The final 1-Mohm part read 996 kohm on the
owner's 2-Mohm DMM range and 1.00 Mohm on its 20-Mohm range; the nominal 1-Mohm
1% reference remains an uncertain engineering anchor, not a certified value.

Sequence 1 is physically installed in W25Q calibration slot 0. Its 2,760-byte
readback matches candidate.bin exactly, SHA256:
af76d19b15a510dff0c880a4ab6e86893e0065ac7e845b9930111680752da78c.
A controlled reboot rescanned it as active; a second physical W25Q readback
matched exactly. No COMMIT was replayed. One STATUS frame was lost while the
cooperative writer performed synchronous NOR work; the original helper recorded
an ambiguous timeout. Read-only recovery found INSTALLED/error 0 and verified
the actual persisted frame. Protocol errors were 1 before reset, 0 afterward.
Safety faults were zero. The final battery read is 3.879 V over COM4, nominal
ADC scaling, charger absent, SAFE/idle, range and excitation off.

The fix waits through expired residual evidence and all eight actual safe
requalification sweeps before advancing the writer. Original protection limits,
permits, ADC/DSP, schema, candidate, deadlines and output controls remain.
46/46 host tests and nine strict unit-profile builds pass. CAL is 37,552 bytes
Flash and 13,740 bytes accounted RAM. The UID-bound experimental NTC profile is
retained. The board remains on BRINGUP_CAL for serial diagnostics; production
firmware has been built but has not been installed by this checkpoint.

This completes engineering OSL capture/fitting/installation, not independent
metrology qualification. No QUALIFIED flags were asserted. Final accuracy,
independent R/C/L holdouts, phase/timing/scope/power checks, experimental NTC
qualification and calibration with final test leads remain
REQUIRES_BENCH_VALIDATION. No new reference exchange is needed to finish this
installation. Assets and settings were not targeted by the transaction.

physical-installation-evidence.zip preserves all deployment, UART, timeout,
read-only recovery and reboot evidence plus the exact procedures. The earlier
incomplete fix and hardware-breakpoint diagnosis are preserved separately in
../installation-sensor-refresh/. Image hashes, memory/build results and final
status are recorded in checkpoint.json, firmware-builds.json and persistence.json.
