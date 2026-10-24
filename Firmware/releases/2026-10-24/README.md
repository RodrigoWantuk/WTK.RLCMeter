# RGS-to-GND firmware integration snapshot

Engineering integration builds, not a qualified metrology release.
[firmware-rgs-ground.zip](firmware-rgs-ground.zip) contains nine variants with
BIN/HEX/ELF/MAP outputs. [firmware-builds.json](firmware-builds.json) records source,
hashes/sizes, profile/options and Flash/RAM. Clean source commit0dd1f3f (full SHA
in manifest), calibration hardware0x00010002. Reproduce using the listed CMake
presets/options at that commit.

| Folder | Use |
| --- | --- |
| product-release | Normal PRODUCT, full wizard, Release |
| product-debug | Normal PRODUCT, Debug with symbols/assertions |
| factory-release | PRODUCT with PC-provisioned valid OSL, Release |
| factory-debug | PRODUCT with PC-provisioned valid OSL, Debug |
| bringup | Standard engineering console |
| bringup-cal | Headless PC calibration service |
| bench-supply | Explicit external-supply measurement diagnostic |
| k1-test | Explicit empty-fixture one-shot relay diagnostic |
| peripheral | Display/buttons/tones, no DUT transactions |

All images pass guaranteed64KiB/20KiB silicon and project memory/composition gates.
Existing soft-size/LTO advisories remain. PRODUCT keeps real battery, charger,
residual, fault and valid-calibration gates. No image is automatically flashed.
Use the existing reviewed SWD/readback plus separate controlled-boot workflow;
back up the current image and preserve W25Q. Old0x00010001 OSL is incompatible,
not erased or re-labelled. Capture new calibration on the reworked board.
Battery/charger and physical/metrology qualification remain separate; see
[the integration report](../../../docs/review/bringup-2026-10-24/README.md).
