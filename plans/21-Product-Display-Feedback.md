# 21 — PRODUCT display feedback

STATUS: IN_PROGRESS

The owner confirms personalized splash and READY with active OSL, but reports
READY art painted twice, slow rendering, and low brightness with correct colors.
No resistor recapture or C/L measurement is requested for this UI correction.

The SPI bus currently defaults TFT to PCLK1/64 (562.5 kHz at 36 MHz), retained
from white-screen diagnosis before the display-flat solder repair. Use the
existing conservative PCLK1/8 path (4.5 MHz) for normal TFT transfers, keeping
the explicit /64 diagnostic fallback, W25Q speed and quiet ownership unchanged.
This restores an existing bus mode, not a new metrology timing architecture.
Physical signal integrity and visible rendering need owner confirmation.

The renderer overwrites its pending view during a long clear/image but keeps
the old rendering generation, so startup state changes can finish READY art and
then restart it. Restart promptly on real state/page/language changes, bind the
latest generation before text begins, and coalesce READY metadata changes that
cannot alter its two visible labels or background. Preserve newest-result/menu/
fault semantics, bounded operations and no-framebuffer memory behavior.

Default brightness is 25%. Raise the fallback for missing/invalid settings to
75%, preserving valid saved preferences and the existing brightness editor.
Do not touch LCD analog/gamma registers or replace the personalized image.

Expected files: spi_bus default; settings default; ui_product request/coalescing;
targeted clock, settings/menu and render regressions; this plan and delivery docs.
Run standard and PC-provisioned PRODUCT host tests, all nine strict UID-bound
images and linker gates. Program a source-bound PRODUCT image preserving W25Q
assets, settings and sequence 1. Confirm READY/SAFE/no auto measurement using
metadata, then ask the owner to verify brightness, one READY painting and normal
colors. Keep ADC/DSP, hardware permits, analog limits and calibration unchanged.
