# Personalized boot splash delivery — 2026-10-24/25

The owner PNG is in the seven-entry schema2/API4 resource pack and all nine
embedded firmware variants implement the shared three-second boot presentation.
The new pack was installed on the first board and compared byte-for-byte through
the real W25Q driver. The complete mutable32KiB tail is unchanged. Standard
BRINGUP was updated, verified in internal Flash and independently booted SAFE.

Firmware source: `adfce3c59aac085a95b7fa487fed5f836dca21ec`. Uploader handshake pacing is tracked in commit4bbaa2b;
the final firmware is bound to the clean source recorded here. The temporary
resource receiver used source52df01cdd341 before the final clear correction.
Pack:104499bytes, SHA256`d5e6f558173a67f3a30212f4951be11db7893121e12fc210d3b06df30c2cd78f`. Erase span `[0,0x1A000)`; mutable
tail `[0x3F8000,0x400000)`. All873 frames received validated ACKs and END/COMPLETE.
The initial1ms attempt failed on BEGIN before mutation; it was inspected and
separately reset. The successful public uploader used10ms per BEGIN byte,
2ms per subsequent byte, per-byte flushing and250ms post-BEGIN settling.

Running-core SWD sampling observed full-image HOLD then CLEAR after3012ms,
with K1/range/excitation commands off and no fault throughout. This verifies
controller/SPI completion timing; it does not prove visible TFT pixels. PRODUCT
accepted the pack after reset and remained CALIBRATION_REQUIRED with invalid OSL.
The final BRINGUP banner matches the firmware source and its passive status shows
K1 SAFE, range DISABLED, charger ABSENT and zero faults. No DUT capture, relay
pulse or calibration/settings mutation was requested in this task.

All nine embedded builds passed strict warnings, silicon/project memory limits
and profile-composition checks. Host CTest44/44 and Python490 tests ran:391 passed and99 were skipped; after pacing changed,8 transport and17 resource
tests passed. BRINGUP fills the guaranteed64KiB Flash; the experimental DC pilot is now
an explicit opt-in. Bench/K1 variants retain every command but reduce unsolicited
logging; CAL includes only boot graphics then returns to binary serial service.

REQUIRES_BENCH_VALIDATION: observe the actual TFT splash/orientation/visible
timing. Valid OSL and remaining electrical/metrology qualification are separate.
Only standard BRINGUP was physically exercised at the final source; the other
eight final variants were build-tested. The temporary factory receiver at52df01c
was physically booted for resource provisioning.

[Detailed receipt](Boot-Splash.json) and
[bundles](../../../Firmware/releases/2026-10-25/boot-splash) contain source/binary
identities, build metrics, complete transfer and physical readback evidence.

The owner explicitly chose to inspect the visible splash later. No visual pass is inferred.
