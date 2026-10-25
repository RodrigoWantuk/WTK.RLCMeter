# High-Z OPEN repeatability pilot — diagnostic only

`open-pilot.zip` preserves six separate physical OPEN CCO1 artifacts at
1 Mohm / 10 kHz / 100 mVrms, decoded observations, the complete UART log and
the bounded acquisition script. Source is f86b7f4e, UID
54ff70068370525760380567, hardware 0x10002/model5. Each original artifact contains
six accepted hardware blocks, zero rejected blocks, HG-only validity and no
observed HG overlap. These 36 hardware blocks are not a new integrated observation.

Across the six exported six-block means, relative complex standard deviation is
6.91360% for normalized 1X transfer and 6.83089% for effective HG transfer.
The mean-of-ratios gain is diagnostic only; no coefficient or validity flag was
installed. This is not accuracy, physical qualification or proof of a noise model.

The final serial battery reading is 3.942 V, nominal scaling. All six captures
returned SAFE/idle, fault/protocol counters stayed zero, and active OSL remains
absent. Original source-7017e5e0 observations are unchanged and separate.
Next dependency: owner's fresh Minipa reading and direct 1-Mohm LOAD attachment.
See [the execution plan](../../../../plans/14C-High-Z-Integration-Investigation.md).
