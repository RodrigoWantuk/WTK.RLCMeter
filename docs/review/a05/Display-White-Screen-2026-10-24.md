# Display white-screen investigation

Status: IN_PROGRESS; REQUIRES_BENCH_VALIDATION. No final-assembly go-ahead.

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
