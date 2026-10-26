# Display white-screen investigation

Status: WHITE_SCREEN_RESOLVED_BY_OWNER_FLAT_SOLDER_REPAIR;
PERIPHERAL_RENDERING_ORIENTATION_AND_BUTTONS_CONFIRMED_BY_OWNER.
This is a bounded functional observation, not full instrument qualification.

The owner confirms that the dedicated peripheral firmware responds to UP/OK/DOWN
with tones, but the lit TFT remains white after reset. UART wiring is physically
removed to fit the display; no absent serial data is treated as a boot failure.

The module header labels, starting at pin1, are VCC,GND,CS,RESET,DC,SDI,SCK,LED,SDO.
They match the current J_TFT interface. The owner measured3.3V at module VCC,
CS,RESET and DC relative to its GND; no regulator could be identified visually.
The owner reports the backlight arrangement correct. Neither module identity nor
internal panel supply/current is independently qualified by these observations.

## MCU-side evidence

Initial peripheral image e1e49c2 was programmed with byte-exact internal Flash
readback and separately reset/run. Brief SWD inspection without reset found:

| Evidence | Actual value | Limit |
| --- | --- | --- |
| Display state | READY, init index5 | No physical acknowledge in write-only SPI |
| UI | stage8, failed0, dirty0 | All MCU-side draw operations completed |
| GPIOB CRH |0x94922222 | PB13/PB15 AF output, PB14 input, TFT controls output |
| GPIOB ODR/IDR |0x1C18 | Reset/CS/DC high, K1/range off |
| GPIOA ODR |0xB000 | Excitation/K2 off |
| SPI2 CR1/CR2/SR |0x354/0/2 | Mode0, divider8, enabled; no pending transfer error |

The e4c8633 diagnostic adds a bounded read-only response capture before rendering,
with raw bytes and software statuses retained in26B for SWD. It changes no panel
power/VCOM/gamma setting and runs no DUT operation. Four relevant host tests and
the strict Arm peripheral build passed. The sealed clean build uses14308B Flash
and11772B accounted RAM including2048B reserved stack.

Image SHA256:
`03b0595991a22f32ed188bd18290089590095101a15aa4ed42e44d49a1450281`.
Programming was INTERNAL_FLASH_VERIFIED; full prior Flash was backed up and matched
the previous deployment before erase. Whole new Flash SHA256:
`48a91db11ca0c91dab13ba9acc67e0b46429358fbb8d6fe50e75cb3692d5a4ce`.
W25Q/options were not programmed. Controlled reset/run and subsequent halt/read/
resume were logged separately; UART is not connected.

All five raw four-byte responses for D3,0A,0B,0C,0F were00 00 00 00; software
status0 for each, completed5. Display/UI states still reach READY/stage8/failed0.
This is not valid controller identification or positive receipt evidence.
The shared-bus Flash probe separately records detected=true, JEDEC EF4016 and
capacity4MiB. This demonstrates a nonzero SPI return from the external Flash;
it does not prove the TFT connector, TFT SDO or TFT power/reset/interface mode.

## Connectivity and next check

A narrow read-only extraction from the October24 EasyEDA source and matching
Gerber flying-probe export confirms J_TFT pin nets match the header labels,
including shared TFT_MISO to UFLASH.2 and USTM32.3. This extraction is neither
full routed-copper DRC nor continuity evidence from the assembled board.

The owner is asked to remove5V and unplug ST-Link USB before resistance checks:
B15→SDI and B13→SCK should include the33Ohm series resistors; B14→SDO,
B12→CS, B10→RESET and B11→DC should be close to lead/contact resistance.
The owner subsequently reports B15→SDI=33Ohm, B13→SCK=33Ohm, and each of the other
four paths=0Ohm, with no short from these signals to GND. These observations
support end-to-end connectivity through the fitted connector; they do not prove
waveform integrity or controller function. Module code/marking and size remain
requested, along with confirmation that power/ST-Link were restored. Do not
increase module supply or apply panel-specific power-control values by guessing.

Protocol reference: [ILITEK ILI9341 V1.05, section7.1.10 and diagnostic register
descriptions](https://files.waveshare.com/upload/e/e3/ILI9341_DS.pdf).
The five-command existing init remains a candidate limitation; zero readback alone
does not establish whether the fault is wiring, return-path support, internal
power, controller identity, interface mode or initialization.

Evidence is local under `Firmware/build/physical-20261024-display-readback/`, with
the prior state under `physical-20261024-peripheral-test/`. Preserve sealed images,
full backups, deployment/boot transcripts, raw state reads and tft-nets.json.

## Slow common-DCS attempt

Owner module label: "2.8 TFT SPI240x320 V1.2". A similar purchase listing names
ST7789/ILI9341 ambiguously; this does not identify the fitted controller. The
isolated peripheral image62e7659 adds only common DCS display controls, reset50ms
with250ms after release, longer software-reset/sleep/display waits, and TFT clock
PCLK1/64. Flash keeps PCLK1/8. No controller-specific power/VCOM/gamma/porch
parameters are applied. Ordinary profiles retain the original command sequence;
the diagnostic table is compiled only with the existing peripheral variant.

Five relevant host tests pass, including wrap-safe reset timing, the common command
list and unchanged ordinary list, raw read04/09 responses, clock switching with
CS inactive, Flash clock preservation and quiet exclusion. Strict Arm builds and
hard memory/composition checks pass: peripheral14468B/11788B accountedRAM,
ordinaryBRINGUP65448B andPRODUCT62232B Flash. Existing soft-size/LTO advisories
remain; ordinaryBRINGUP has very little remaining Flash headroom.

Clean sealed image62e7659 SHA256:
`dfc930cfc51ac54b938f2ec4393c8979976923cf46d47db54cba03487eacb999`.
Full prior internal Flash was backed up/matched before programming. Exact
readback passed; new full Flash SHA256:
`ad3faa1ad08eae1497448f03e3a197996d4498faf18594199e632b3909e50c2f`.
Separate controlled boot confirmed target access/power and reset/run. Subsequent
SWD showed displayREADY/index8, UIstage8/failed0 and SPI2CR1=0x36C/SR2. All seven
five-byte responses (04,D3,09,0A,0B,0C,0F) are zero, all software statuses0,
completed7. Flash still reports EF4016/4MiB. Owner confirms screen still white.

A second controlled boot with two image-bound hardware breakpoints observed
actual GPIO inputs just before a command/data SPI transfer. Command phase:
GPIOB IDR0x8419/ODR0x0418 (PB12CS=0,PB11DC=0,PB10RESET=1). Data phase:
IDR/ODR0x0C18 (CS=0,DC=1,RESET=1). MOSI/backlight bits may differ with instantaneous
state. K1/range/excitation remain off. Both breakpoints were removed and the
firmware resumed. Thus CS/DC can reach their commanded low/high levels at the
MCU; this is not an oscilloscope trace at the LCD or a panel identity result.

Evidence: `Firmware/build/physical-20261024-display-slow-dcs/`, including
`display-state-001` and `control-levels-001`. Next useful evidence is clear front/
back module identification and inspection of regulator/interface configuration;
do not equate lack of readback with a proven defective LCD. Generic read-only
protocol basis also includes the [Sitronix ST7789V datasheet](https://dl.espressif.com/dl/schematics/ST7789V_SPEC_V1.0.pdf).

## Resolution and portrait180 confirmation

The owner repaired a bad solder joint on the display flat and reported visible
rendering, then requested180degree reversal for the actual mounting. Image3d276b1
changes only the peripheral diagnostic orientation to MADCTL88/logical rotation2;
the ordinary initialization retains48. Two relevant host regressions and the
strict Arm peripheral build passed (14476B Flash,11788B accountedRAM).

Sealed clean image SHA256:
`1e5e819c89535bd492949da4090496be76eb6f8342a142985b10313a3206bc03`.
Prior image backed up/matched, exact programming readback passed; new full Flash
SHA256 `38764dce0350dd650d2535f025b7121891fe4ade6876fb6d527619165101cc7c`.
Separate controlled boot and short SWD halt/read/resume confirmed UIstage8/failed0,
displayREADY/rotation2 and safe output states. No metrology operation was run.

Raw five-byte responses after repair, in the recorded command order:

| Register | Raw bytes |
| --- | --- |
|04 ID |00 00 00 00 00 |
|D3 ID |00 00 00 00 00 |
|09 status |E2 29 82 00 00 |
|0A power mode |9C 00 00 00 00 |
|0B MADCTL |88 00 00 00 00 |
|0C pixel format |05 FF FF FF FF |
|0F self diagnostic |C0 00 00 00 00 |

All software statuses0/completed7. Matching MADCTL88 plus nonzero status responses
demonstrates LCD return communication after repair, rather than merely Flash MISO
activity. Zero identification commands do not identify the exact controller.
The owner then confirmed the rotated appearance and button operation ("perfeito.
botões ok tb.") in response to the orientation/UP/OK/DOWN state/tone check.
No module replacement or panel-specific analog settings were needed.

Evidence: `Firmware/build/physical-20261024-display-portrait180/`. Keep the owner
mounting preference for subsequent product UI integration; this diagnostic change
does not silently alter the final product screen configuration. Calibration,
full range/phase/transient qualification and battery/charger validation remain
separate work. Do not mark Phase03's external-resource/Flash-write/quiet bench
criteria complete based on this limited peripheral pass.
