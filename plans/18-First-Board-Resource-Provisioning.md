# 18 — First-board production Resource Pack provisioning

Status: COMPLETE_FOR_RESOURCE_INSTALL_AND_READBACK; PRODUCT_RELEASE_PENDING.
Explicit owner request, 2026-10-24, extending plans03/08/09A/17.

Installed22891bytes, schema2/API4, all193 frames ACKed on the reviewed second
attempt; post-reset PRODUCT resource admissionOK with calibration required.
Complete external pack readback is identical,32KiB mutable tail unchanged.
Standard source0dd1f3f BRINGUP restored, internal readback and final UART SAFE
boot/status verified. See [physical evidence](../docs/review/bringup-2026-10-24/Resource-Install.md).
No firmware source change. Serial truncation required inspected recovery and
10ms paced/flushed bytes plus a10s post-BEGIN dwell; generic unpaced upload and
power-loss qualification remain pending.

Prepare the current schema2/API4 resources from the tracked manifest and install
them in the fitted4MiB W25Q. Preserve calibration/settings and return to the
standard BRINGUP image for remaining functional tests. No calibration creation,
DUT transaction, relay pulse or product release is included.

Prerequisites: source-bound published images; observed safe standard BRINGUP,
empty DUT terminals, charger absent, battery/VMID valid; capacity-relative layout.
The existing factory PRODUCT receiver supports blank-media provisioning without
valid OSL; missing OSL must continue to block measurement. Its temporary use for
resource recovery is not acceptance of a final factory PRODUCT deployment.

Expected files: this plan; bounded ignored operation scripts/logs, pack and backup;
durable resource-install evidence/report and reproducible release resource bundle.
No firmware policy/format change is planned. Reuse the real receiver, CRC/frame
validation and W25Q driver; no raw SPI register implementation or blanket erase.

Sequence: validate deterministic pack and partition bounds; observe idle SAFE;
back up internal Flash, the external erase span and mutable tail. If necessary,
use debugger calls to the existing read-only W25Q driver only at an idle point,
with core/workspace ownership controlled and scratch restored. Debugger/Flash
algorithms can disturb the execution context; follow them with an explicit
controlled reset/run and separate UART boot acceptance instead of assuming a
debugger detach resumes the original application context.
Temporarily deploy/readback-check the published receiver-capable PRODUCT image;
boot into resource recovery with no valid calibration. Send bounded acknowledged
PLC1 frames with DTR/RTS deasserted and pacing compatible with polled UART.
Wait for NOR completion before any SWD access. Restore/readback-check standard
BRINGUP, read the actual external resource bytes and mutable tail, and compare
against the pack/backup. Confirm post-reset persistence, identity and idle SAFE.

Acceptance: real external readback equals the complete built pack; immutable
artifact/source hashes and all frame results retained; mutable tail unchanged;
resource header/entries/text/fonts validated and standard BRINGUP running SAFE.
Record observations separately from unexecuted power-loss, wear, electrical and
physical UI qualification. On an ambiguous write, stop and inspect state before
retrying; do not replay automatically or claim successful upload from ACK alone.
