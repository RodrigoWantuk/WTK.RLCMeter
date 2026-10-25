# UID-bound experimental NTC unit delivery

`firmware.zip` contains nine source-bound BIN/ELF/HEX/MAP images and generated
factory-table metadata. These are only for UID 54ff70068370525760380567.
Use the schema2 deployment manifest; manual flashing bypasses the UID binding.
`physical-evidence.zip` preserves all temperature references, uncertainty,
monitoring, verified programming/readback and post-boot conversion evidence.

NTC adjustment is experimental and unqualified; OSL calibration is not installed.
Rebuilds must explicitly select the unit profile. See
[bench report](../../../../docs/review/bringup-2026-10-25/NTC-Factory-Profile.md).

Primary guarded deployment packages are inside firmware.zip under deployable/
(BRINGUP, BRINGUP_CAL and FACTORY_PRODUCT). Factory production still requires
complete OSL provisioning; the NTC adjustment alone does not satisfy that gate.
