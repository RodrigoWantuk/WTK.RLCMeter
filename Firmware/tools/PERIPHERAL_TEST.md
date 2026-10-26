# Standalone BRINGUP peripheral test

Current October24 contract: WTK_BRINGUP_PERIPHERAL_TEST defaults OFF, requires
BRINGUP, and is exclusive with bench-supply/one-shot K1 variants. No active Lab,
DUT or calibration mutation path is linked. Safe boot, real sensors/charger/fault
checks and watchdog remain active. No W25Q writes occur.

Build from repository root:

```powershell
python Firmware/tools/pc_firmware_deploy.py build --profile BRINGUP --peripheral-test --out Firmware/build/peripheral-handoff
```

Or from Firmware/:

```powershell
cmake --preset stm32-bringup -B build/peripheral -DWTK_BRINGUP_PERIPHERAL_TEST=ON
cmake --build build/peripheral
```

The panel clears black, draws red/green/blue bars and shows setup/FW/Git identity.
UP/OK/DOWN rows show PRESSED in green or RELEASED in white. Each PRESS plays120ms:
UP660Hz, OK1000Hz, DOWN1500Hz. A newer press preempts; release updates display only.
Quiet/fault policy stops or blocks tones. UI has no relay/range control.

Rendering is incremental:64-pixel chunks, one glyph per step, no framebuffer or
external assets. Quiet pauses it; display errors stop retries while safety runs.
All TFT profiles now share portrait180 MADCTL88/rotation2,50ms reset/250ms release
wait and the common DCS init. TFT clock=PCLK1/64; Flash=/8. BRINGUP_CAL is headless.
Backlight remains25%, which is not an electrical current limit.

Only this variant reads04,D3,09,0A,0B,0C,0F once before drawing, retaining five raw
bytes/status each in43B for SWD when UART cannot fit beside TFT. Dummy/padding is
preserved; no register command interrupts RAMWR. Completion is not presence proof.
Owner repaired bad solder on the display flat and confirmed rendering/orientation/
buttons/tones. LCD now returns MADCTL88, power-mode9C and diagnosticC0; ID04/D3 stay
zero. Exact controller ID remains unresolved. Owner reports backlight corrected;
independent load/current qualification remains separate from functional success.

Keep K1 de-energized and range/excitation off. UART absence is not boot failure.
Programming/readback and controlled boot are separate actions. Source consolidation
does not automatically flash a new physical image. Calibration hardware identity
is0x00010002; PRODUCT retains real battery/calibration/safety gates.

See [display evidence](../../docs/review/a05/Display-White-Screen-2026-10-24.md) and
[current integration/build status](../../docs/review/bringup-2026-10-24/README.md).
