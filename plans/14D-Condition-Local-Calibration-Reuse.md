# 14D — Audited condition-local calibration reuse

STATUS: COMPLETE — ENGINEERING_COMPOSITION_INSTALLED_AND_PERSISTENT; INDEPENDENT_QUALIFICATION_PENDING

## Owner steering and bounded decision

The owner objected to repeatedly exchanging the same six resistors and asked
whether calibration was still ongoing. Before requesting more exchanges, audit
which complete earlier condition records remain compatible. The current 10-ohm
LOAD group is complete: fresh owner reading 10.4 ohm, three accepted source-a52
triplets, strict serialized-float32 checks passed. Preserve those actual results.

Reuse the 18 complete original OPEN/SHORT/LOAD triplets for RREF 100 ohm, 1 kohm
and 10 kohm from source 7017e5e0. Each triplet remains within its original source;
do not relabel frames, change quality flags or combine standards across sources.
Use current source a52dba35 for the three new 10-ohm conditions and all twelve
100-kohm/1-Mohm conditions. This leaves two physical reference setups, not five.
The high-Z references are freshly remeasured by the owner before connection.

This is an explicit first-unit engineering composition, not permission for the
ordinary campaign verifier to accept mixed identities. Default capture/campaign
contracts remain unchanged. Existing model5 records are condition-local and
firmware-independent when hardware, acquisition interpretation and runtime math
remain compatible; the installed frame still requires all 33 unique conditions.

## Compatibility evidence

The read-only audit validates the same real UID, hardware revision 0x10002,
model5 and identical nominal ADC scale/offset vectors. It verifies identical Git
blobs for all 91 BSP/hardware/measurement files except the explicitly reviewed
power/NTC files. Critical workflow extraction, stability and reconstruction
function bodies and battery conversion/classification bodies are unchanged.
Build flags match except source identity, CAL integration and the documented
experimental UID-bound NTC profile. Actual current OPEN/SHORT artifacts for each
reused condition confirm the one-block acquisition path, not grouped integration.

Every reused triplet passes the original artifact CRC/SHA/identity/safety/quality
checks and strict solver, including the HG-fitted condition where appropriate.
No gain borrowing, path promotion, absent standard or invented fit is allowed.
Original per-condition temperature readings retain their original nominal-NTC
source provenance. They are not directly comparable with the new experimental
NTC readings. No thermal accuracy/compensation is claimed; base model5 runtime
is unchanged and supplementary curves are disabled. Reference tolerance remains
uncertain; fresh DMM digits are consistency evidence, not certified truth.

## Completion path

1. Preserve the compatibility audit, full original cohorts and new 10-ohm group.
2. Acquire all six current-source LOAD conditions using only the owner's freshly
   checked 100-kohm resistor; then all six using the freshly checked 1-Mohm part.
3. Produce an explicit per-condition composition manifest retaining source Git,
   original artifact hashes and complete triplet identity for each record.
4. Solve all 33 complete triplets without changing the existing model/thresholds.
   Validate serialized coefficients and the full frame with the existing C
   solver and canonical installer validator. Preserve both inputs and outputs.
5. Only after those checks, install using the existing guarded transactional
   protocol and verify byte-exact readback. No firmware change is needed.

No complete candidate or installed calibration exists at this checkpoint.
Independent accuracy/scope qualification and final-lead calibration remain
separate. This amendment supersedes the unnecessarily broad request to recapture
all six LOAD references after the transport/NTC/three-condition integration edits.

## Final capture and composition checkpoint

The owner reported 99.95–100.00 kohm and later 996 kohm on the 2-Mohm scale /
1.00 Mohm on the 20-Mohm scale. All six current 100-kohm and all six current
1-Mohm LOAD conditions pass, including the three originally blocked keys with
actual observed HG transfer. Every triplet retains its actual source identity.
The resulting composition contains 18 complete original and 15 complete current
condition fits; all 33 pass both Python and the actual C solver, canonical frame
validation and the existing serialized LOAD-center consistency criterion.
Missing conditions, duplicate observations, identity relabeling and quality-flag
promotion are rejected without producing a final candidate.

Candidate SHA256 is af76d19b15a510dff0c880a4ab6e86893e0065ac7e845b9930111680752da78c.
The Python and C frames need not be byte-identical due to arithmetic rounding;
both pass their strict checks, and the C-generated frame is selected. The first
physical installation returned SAFETY_BLOCKED after COMMIT; active sequence
remains zero and no successful installation is claimed. Plan14E isolates the
sensor-refresh scheduling fix. Captures and candidate remain unchanged; no
further resistor exchange is required for installation recovery.

## Installed result

Plan 14E completed the installer-only scheduling correction. The unchanged
33-condition candidate is installed as active sequence 1; full physical W25Q
readback matches before and after a controlled reboot. Original identities and
flags remain intact. Engineering capture, composition and installation are
complete. Independent accuracy and final-lead qualification remain pending.
See [installation evidence](../Firmware/releases/2026-10-25/installation-residual-requalification/README.md).
