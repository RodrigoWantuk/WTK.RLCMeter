# Corrected zero-return SHORT bringup

`firmware.zip` contains nine source-bound binaries; `firmware-builds.json`
records their identities and hashes. `physical-evidence.zip` preserves the
verified deployment and 33 corrected SHORT artifacts.

This is an engineering checkpoint. Same-image OPEN/LOAD, installation and
independent qualification remain pending. The board runs BRINGUP_CAL; no
calibration is installed. The W25Q resource pack is unchanged.

See [bench report](../../../../docs/review/bringup-2026-10-25/OSL-Zero-Short.md).
