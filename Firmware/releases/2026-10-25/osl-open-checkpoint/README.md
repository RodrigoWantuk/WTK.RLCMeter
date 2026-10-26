# OSL OPEN checkpoint

The nine-image `firmware.zip` is built from source 063c7378319f293640d6d19d5fadcb3fa3ad686f.
`firmware-builds.json` binds every image/hash/profile. `physical-evidence.zip`
preserves accepted OPEN frames, historical rejected attempts, wire logs, the
verified deployment, battery read, tests and local capture helpers.

This is an engineering OPEN-only checkpoint. SHORT/LOAD, candidate generation,
installation, independent validation and electrical qualification remain pending.
The board is running BRINGUP_CAL; do not treat PRODUCT builds as a calibrated
device release. The existing W25Q resource pack is unchanged.

See [bench report](../../../../docs/review/bringup-2026-10-25/OSL-Open.md).
