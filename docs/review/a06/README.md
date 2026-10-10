# A06.1 PC OSL calibration foundation

STATUS: IMPLEMENTED_TESTED_HOST / SYNTHETIC_END_TO_END. A06 migration remains open.
REQUIRES_BENCH_VALIDATION: Rev.1 PCB unassembled. Real-device provisioning is not implemented.

Starting SHA: `29bb74adcd5bf95e0a57b8756201392ab53cd388`, confirmed merge of PR #9.
The clean checkout was fetched, fast-forwarded to origin/main, and recursive
submodules initialized before implementation. Branch: `codex/a06-pc-osl-foundation`.
Owner decisions O01–O07 are unchanged. PRODUCT's full wizard, calibration boot gate,
schema v2/model v4, safety, acquisition timing, resource recovery and curve capability
are unchanged. Only host tools and a host CTest bridge were added.
The measured implementation commit is `8f3d0c6e738877daf735c86475d4312d8128f9bc`;
subsequent changes contain the experiment tool and review evidence only.

## Implemented path

`Firmware/tools/pc_osl.py` imports voltage phasors, forms source/return differences
against the mean VMID, and normalizes both acquisition paths. It reuses the existing
frame inspector and firmware equations: observed HG is the mean usable overlap
ratio; prefer a valid 1X fit, otherwise normalize HG by its observed complex transfer.
The correction is `K*(t-ts)/(t-to)`, with `K=Zload*(tl-to)/(tl-ts)`.
No HG fallback is allowed without observed overlap. Nominal HG alone does not certify
that path. Known nonzero passive complex LOADs and resistance LOADs are accepted.

Exactly 33 Rev.1 keys and 99 unique OPEN/SHORT/LOAD captures are required. The parser
rejects forbidden 10-ohm/500-mVrms keys, unsafe/unstable metadata, invalid path flags,
missing/duplicate/mismatched captures, nonfinite numbers, unsupported references,
singular solutions, float32 overflow/collapse, invalid ADC scales and zero sequences.
The deterministic 2,760-byte candidate uses existing schema, record IDs, flags,
CRC and commit marker. It never sets QUALIFIED. The Markdown report preserves LOAD
IDs, centers and tolerance intervals; printed 1% tolerance is not an exact reference
or an instrument error bound. ADC provenance is mandatory and remains unqualified.
Capture metadata is an import contract, not proof that a physical interlock ran.

`pc_osl_synthetic.py` derives samples from a series-impedance/shunt-admittance fixture,
complex channel gains, independent source/HG gain, sinusoidal excitation, bounded
noise and DC offsets. Six channels are sampled at 256 points and passed through the
existing host DFT. Voltage samples are clipped to 0–3.3 V; this is not an ADC/timing
or hardware model. The fixture has no precomputed calibration coefficients.

`pc_osl_provision.py` provides bounded PLC1 frames and a fake device. Development IDs
0x40/0x41/0x42 are simulator-only BEGIN/CHUNK/END operations, not a firmware protocol
commitment. Existing PLC1 payload CRC, header encoding and 128-byte payload limit are
reused. Chunks carry offset/length/reserved fields and at most 120 data bytes; the
full candidate is also CRC checked. Header sequence/flags and chunk order are checked
because PLC1's CRC covers payload only. Reset discards staging. Installation validates
before erasing the inactive 4-KiB slot, programs header and payload before the commit
marker, validates full readback, then activates. Blank devices deny normal measurement.
Factory-standard captures have separate simulated safe/idle permissions and never
grant measurement permission. A candidate must be the active sequence's successor;
0xffffffff rollover is rejected pending the embedded migration decision.

## Commands (repository root)

```powershell
python Firmware/tools/pc_osl_calibrate.py synthetic --ideal --out Firmware/build/captures.json
python Firmware/tools/pc_osl_calibrate.py build Firmware/build/captures.json --sequence 1 --out Firmware/build/candidate.bin --report Firmware/build/calibration.md
python Firmware/tools/pc_osl_calibrate.py inspect Firmware/build/candidate.bin
python Firmware/tools/pc_osl_calibrate.py simulate --out-dir Firmware/build/a06-demo
python -m unittest discover -s Firmware/tests/tools -p 'test_*.py'
```

The synthetic command needs an existing output parent; `simulate` creates its output
directory. It writes captures, a binary candidate, PLC1 transfer stream, Markdown
report and simulation JSON. There is deliberately no COM-port upload command.
Input fields and physical-operation gaps are described in [the tool contract](../../../Firmware/tools/PC_OSL.md).

## Validation

Host Debug/Release: 38/38 tests OFF and 41/41 ON in each configuration. All 171 Python
tests pass, including 16 new OSL/provisioning tests. The host-only C bridge links the
actual solver, serializer, PLC1 codec and W25Q store state machine under warnings-as-errors.
Its CTest driver checks 132 PC/C condition fits (33 HG), 100 exact PLC1 round trips,
four byte-exact calibration frame round trips, 45 C store interruption states,
blank installation, corrupt-slot fallback and invalid-candidate rejection.
Maximum normalized complex coefficient difference: **1.94002796e-7**, below 2e-5.
Normalization is `|PC-C|/max(1,|PC|,|C|)` for each complex coefficient; comparing tiny
imaginary components in isolation would misrepresent cancellation near a real LOAD.
The ideal sequence-7 frame golden SHA-256 is
`d7bb946c5c94c902c7e2cbbf9e3bcb821284e1be90b78142feca622e6b63845b`.

The adapter tests every incomplete transfer prefix and all **2,760 byte interruption
positions**, including partial commit markers. Previously active calibration survives;
blank interrupted installation remains blank. Tests also cover CRC-valid duplicate
keys, QUALIFIED self-assertion, skipped/zero sequences, corrupt payloads, invalid
candidate, charger/residual/idle denial and recovery from corrupt newer slots.

The complete sampled demo provisions 99 captures / 33 conditions using 25 PLC1
frames, reboots to sequence 1, and performs 33 sampled complex DUT holdouts. Maximum
relative impedance error is **0.0011700813 (0.117%)**, with 0.2-mV bounded sample noise
and 25-mV channel-dependent DC offsets. Every result is explicitly synthetic and
unqualified. Reference-tolerance testing separately demonstrates approximately 0.5%
bias when the actual LOAD differs from its printed center by 0.5%; fitting does not
remove reference uncertainty.

ARM Debug/Release/BRINGUP builds, size gates and profile/composition checks pass.
Wokwi local-file checks and lint pass; full scenarios were not executed.
Supplementary curves ON/OFF are preserved. No board accuracy, safety, SNR, leakage,
relay behavior or ADC timing has been qualified. Hosted CI is reported separately
from local checks; Wokwi scenario execution is not evidence for this PC fixture.
PR #10's initial [Flash forensics run](https://github.com/RodrigoWantuk/WTK.RLCMeter/actions/runs/38002704043)
and [Virtual Hardware run](https://github.com/RodrigoWantuk/WTK.RLCMeter/actions/runs/38002703971)
were blocked before execution: both job annotations report an account billing lock,
and both job step arrays are empty. These are not passing CI or code-test failures.

## Measured memory and migration forecast

[Machine-readable measurements](measurements.json) and [isolated linker results](link-probes.json)
record the source SHA, memory, binary hashes and configuration. A01's
`collect_flash_evidence.py` generated section/map/symbol evidence for all four images.
RAM includes static allocations and the 2,048-byte stack reservation; heap is zero.

| Image | Before Flash / RAM (B) | After Flash / RAM (B) |
| --- | ---: | ---: |
| PRODUCT Debug OFF | 58,396 / 14,536 | 58,396 / 14,536 |
| PRODUCT Release OFF | 61,580 / 14,976 | 61,580 / 14,976 |
| PRODUCT Release ON | 64,096 / 15,040 | 64,096 / 15,040 |
| BRINGUP MinSizeRel OFF | 65,052 / 14,588 | 65,052 / 14,588 |

No embedded provisioning code was added: current MCU cost is **0 B Flash / 0 B RAM**.
Release OFF leaves 3,956 B below the guaranteed 65,536-B silicon limit and 2,932 B
below the PRODUCT gate of 64,512 B. This alone does not fund remaining functionality.

The reproducible isolated experiment archives tracked HEAD into new ignored
directories, uses canonical Release flags and LTO, and never edits deployable source:

```powershell
python Firmware/tools/pc_osl_flash_probe.py --out Firmware/build/a06-new-link-probes
```

| Linker experiment (NEVER FLASH) | Flash / RAM (B) | Interpretation |
| --- | ---: | --- |
| Identical baseline | 61,580 / 14,976 | Confirms configuration equivalence |
| Solver solve-function stub only | 59,428 / 14,976 | 2,152 B saving; runtime application retained |
| Wizard public workflow calls stubbed | 51,092 / 14,984 | Gross 10,488 B reachability saving |
| Same wizard stubs, capture API anchors retained | 55,880 / 14,976 | 4,788 B restored, including 24 B anchor table |
| Same wizard stubs, capture + installation APIs retained | 57,836 / 14,976 | Further 1,956 B restored, including 28 B installation anchors |

These are LTO reachability deltas, including inlining and unreachable UI/service code,
not exact source-file byte totals. Wizard init/context remain; stubs prevent workflow
execution. The images are unusable for initial calibration. Retained capture APIs
pull workflow/acquisition dependencies back in; retaining complete APIs is conservative
for a future raw-capture receiver. Candidate begin/discard/access/validation/insertion,
commit and service-step anchors additionally retain the actual A/B writer and activation
path. Omitting these would overstate migration savings by 1,956 B. Solver-only savings
overlap gross wizard savings and must not be added again. The measured removable
remainder with acquisition AND installation retained is **3,744 B**.

Provisioning receiver cost is **estimated**, not measured: reserve 1,000 / 1,800 /
3,000 B optimistic/central/conservative, retaining safety, store and runtime APIs.
Projected net recovery is **2,744 / 1,944 / 744 B** and resulting Release Flash is
58,836 / 59,636 / 60,836 B before other remaining MUST work.

Using A02's unchanged remaining MUST increments (2,984 / 6,408 / 12,856 B), which
already include the replacement protocol, gives **60,820 / 64,244 / 70,692 B** after
the measured 3,744-B remainder is removed. Do not charge provisioning twice.
The central forecast leaves 1,292 B silicon headroom and passes the PRODUCT gate by
only 268 B; the conservative forecast exceeds silicon by 5,156 B. Re-enabling curves
adds 2,516 B: central 66,760 B exceeds silicon by 1,224 B. Capacity for all mandatory
work is therefore not proven. A06.2 must remeasure its actual receiver and completed migration;
the current forecast is not a release acceptance result.

## Next task

A06.2a capture is now implemented in the development STM32 `BRINGUP_CAL` profile
and PC serial adapter, with synthetic C-backed serial evidence. See
[A06.2a report](A06.2a-report.md). A06.2b installation/readback/recovery is now
implemented below. The next smallest task is bench qualification of this service;
PRODUCT wizard/boot-gate migration remains a separate review.

The following recommendation records the A06.1 handoff and is fulfilled by A06.2a:

Implement a development-only, safe initial-standard capture command and PC adapter,
including device identity, exact condition echo, ADC provenance and cancellation.
Exercise it against the existing acquisition abstractions on the host. Keep the
wizard and boot gate until the complete candidate installation/recovery path passes
[the A06.2 migration checklist](A06.2-checklist.md).

## A06.2b installation implementation

The actual BRINGUP_CAL binary now receives, validates and installs complete PC OSL
candidates through PLC1, the existing workspace and the existing W25Q A/B store.
The 99-capture C-backed serial workflow includes readback, reset recovery and the
normal C calibration application path. See [report](A06.2b-report.md) and
[operating instructions](../../../Firmware/tools/PC_OSL_INSTALL.md).
Physical behavior remains REQUIRES_BENCH_VALIDATION; PRODUCT wizard/gate migration
is a separate remaining step.
