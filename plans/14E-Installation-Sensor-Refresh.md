# 14E — Installation sensor-refresh scheduling

STATUS: COMPLETE — ENGINEERING_OSL_INSTALLED_AND_PERSISTENT; INDEPENDENT_QUALIFICATION_PENDING

All 33 condition fits pass Python and the actual C solver. The explicitly
audited composition reuses 18 complete old triplets and 15 complete current
triplets, without relabeling any source or quality. Candidate SHA256:
af76d19b15a510dff0c880a4ab6e86893e0065ac7e845b9930111680752da78c.
All fresh 10-ohm/100-kohm/1-Mohm groups completed. No further resistor exchange
is required to investigate installation.

The first physical installation accepted transfer, validation and COMMIT, then
returned SAFETY_BLOCKED, active sequence0, store idle, zero faults/protocol
errors. Preserve the failed transaction; never blindly replay its COMMIT.
COMMIT validation/serialization is synchronous, whereas residual sensor age is
bounded to50 ms. install_step currently aborts on any transient invalid sensor
snapshot before the cooperative auxiliary service can refresh it.

Add an explicit internal snapshot flag for expired residual evidence. While
receiving/validated/writing, only if outputs are transfer-safe, faults are zero,
the deadline is unexpired, and the only blockers are SENSOR_INVALID plus the
expected disabled range, wait for real auxiliary refresh. Do not advance the
Flash writer during this wait. Fresh permission remains mandatory. Keep charger,
unsafe residual, critical/unknown battery, output activity, faults and timeout
abort behavior unchanged. No stale sample substitution, threshold extension or
measurement authorization is introduced.

Expected files: capture service snapshot/wait policy; CAL shell freshness flag;
C-backed fixture and regression tests for commit-time refresh, persistent expiry,
and charger preemption. Run host checks and strict unit builds. Keep acquisition,
DSP, calibration model/records and all captured artifacts unchanged. Programming
the installer-only fix must not cause another capture campaign.

After verified internal-Flash update, boot/rescan actual W25Q state. If the first
attempt already committed a matching frame, verify physical readback and recover
without another write. Otherwise require fresh safe state and known successor
before a new transaction. Use the existing guarded writer and exact physical
readback; preserve assets/settings and record all failure/recovery evidence.
No accuracy or independent scope qualification is implied.

## Physical diagnosis of the incomplete first fix

The ba948235 image still aborted. Two hardware breakpoints at the exact abort
branch observed faults=0, blockers=0x0C (SENSOR_INVALID plus disabled RANGE),
transfer_safe=true, factory_allowed=false and aux_refresh_pending=false. A
second breakpoint captured only residual evidence (no battery voltage over
SWD): valid VMID, residual UNKNOWN, consecutive safe count=1 and fresh age=0.
Expiration resets the auxiliary residual policy. The first seven fresh safe
sweeps remain UNKNOWN; waiting only for sample age expiry was insufficient.

The CAL shell now also recognizes fresh UNKNOWN with valid VMID and a positive
safe count below the required eight as requalification in progress. This is
only a scheduling hint: the existing installer waits without advancing NOR,
retains its original deadline, and still requires actual SAFE before writing.
No safe count, threshold, ADC/DSP code, stored calibration model or capture is
changed. Actual invalid VMID, saturation, unsafe residual, other blockers and
faults retain their abort paths. The host test drives the real auxiliary ADC
state machine through expiry and all eight fresh sweeps, and checks rejection
of invalid VMID, saturation and unsafe residual. Existing installer tests cover
no writes during waiting, timeout, and charger preemption.

## Completion evidence

Source 46df1fbb passes 46/46 host tests and all nine strict unit-profile builds.
CAL uses 37,552 bytes Flash and 13,740 bytes accounted RAM. Internal-Flash
programming and readback matched. The unchanged candidate was installed as
active W25Q sequence 1. One STATUS request was lost during synchronous NOR work;
the helper failed closed with a timeout, without replaying COMMIT. Subsequent
read-only recovery found INSTALLED/error 0 and byte-exact physical readback.
Protocol errors were 1 before reset. A controlled reboot rescanned sequence 1
and a second full physical readback matched the same candidate SHA256. Final
serial status is SAFE/idle, battery 3.879 V, zero safety/protocol errors and
calibrated=true. Assets/settings were not targeted. The board remains CAL.

See [physical installation evidence](../Firmware/releases/2026-10-25/installation-residual-requalification/README.md).
This closes installation scheduling and persistence, not independent accuracy,
scope, final-lead or thermal qualification. No additional capture was needed.
