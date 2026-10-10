# A06.2b serial OSL installation

`BRINGUP_CAL` contains the actual acquisition and installation handlers. Build from
Firmware with `cmake --preset stm32-bringup-cal` followed by
`cmake --build --preset stm32-bringup-cal`. Installation is enabled by default;
`-DWTK_CAL_INSTALL_SERVICE=OFF` builds a capture-only service and removes its
installation capability. PRODUCT and BRINGUP do not contain this serial installer.

Connect USART1 PA9/TX, PA10/RX and GND through a **3.3 V** USB-UART adapter at
115200 8N1. Use SWD to load the development firmware. The PCB is unassembled:
physical capture, Flash behavior and electrical safety remain
`REQUIRES_BENCH_VALIDATION`.

## Windows workflow

Install PC dependency `pyserial`. First collect OPEN/SHORT/known LOAD observations
for every condition using [the capture client](PC_CAL_CAPTURE.md). Campaign files
preserve ADC provenance, reference uncertainty and capture SHA-256 identities.
A printed resistor tolerance is an interval, not an exact characterized value.

```powershell
python tools/pc_cal_capture.py ports
python tools/pc_osl_install.py status --port COM5
# Use next_sequence from STATUS; a blank device requires 1.
python tools/pc_osl_calibrate.py build captures.json --sequence 1 --out candidate.bin --report calibration.md
python tools/pc_osl_install.py install --port COM5 --candidate candidate.bin --report installation.json --readback installed.bin
python tools/pc_osl_install.py readback --port COM5 --readback installed-again.bin
```

Successful installation requires device-side verification, INSTALLED status and
byte-identical readback of the persisted frame. The report contains identity,
timestamps, sequence, frame CRC and candidate/readback SHA-256 hashes. A successful
transfer alone is insufficient. The unchanged schema cannot store all campaign
provenance: retain the campaign, reference report and installation report together.
PC candidates cannot assert physical QUALIFIED flags.

Reset the device and query STATUS/readback again. To use the normal standalone
instrument, load PRODUCT through SWD while preserving external W25Q contents.
There is no native USB firmware-switch mechanism. PRODUCT keeps its full-set boot
prerequisite and embedded wizard. Installed coefficients remain unqualified until
independent bench evidence supports metrology qualification.

An opt-in PRODUCT variant now supports factory-only calibration without the embedded
wizard: `cmake --preset stm32-factory-release`, then
`cmake --build --preset stm32-factory-release`. Full usable OSL and the Resource Pack
remain required. Default PRODUCT retains the wizard. See
[A06.2c-prep deployment and qualification gates](../../docs/review/a06/A06.2c-prep-report.md)
before evaluating this configuration on an assembled board.

## Bounded PLC1 contract

API 1 preserves the 16-byte PLC1 header, payload CRC and 128-byte payload limit.
Capture commands 0x50–0x54 and Resource Pack commands are unchanged. IDENTIFY
capability bit 4 advertises installation only when the handler is built and enabled.
Common request/response envelopes are described in [PC_CAL_CAPTURE.md](PC_CAL_CAPTURE.md).
Each command uses a new monotonic nonzero request ID; the BEGIN request ID is the
transaction ID echoed in all transaction bodies. All integers are little endian.

| Command | ID | Body after common request envelope |
| --- | --- | --- |
| INSTALL_BEGIN | 0x55 | u32 length=2760, u32 whole-frame CRC32, u32 candidate sequence |
| INSTALL_CHUNK | 0x56 | u32 transaction, u16 offset, 1–96 candidate bytes |
| INSTALL_VALIDATE | 0x57 | u32 transaction |
| INSTALL_COMMIT | 0x58 | u32 transaction |
| INSTALL_STATUS | 0x59 | empty |
| INSTALL_READBACK | 0x5a | u32 active sequence, u16 offset, u8 count=1–96, u8 reserved=0 |
| INSTALL_ABORT | 0x5b | u32 transaction |

STATUS returns 32 bytes: transaction/expected sequence u32 at 0/4; received/size
u16 at 8/10; actual active sequence/frame CRC u32 at 12/16; installation state,
error, store state and storage availability u8 at 20–23; next sequence u32 at 24;
active slot/valid u8 at 28/29; two reserved zeros. States 0–6 are IDLE, RECEIVING,
VALIDATED, WRITING, INSTALLED, FAILED, ABORTED. New errors 12–15 are
INVALID_CANDIDATE, SEQUENCE_ERROR, STORAGE_ERROR, TOO_LATE. READBACK returns
u32 sequence, u16 offset, u16 length=2760, u32 frame CRC, then requested bytes.
The status/frame CRC is the schema CRC at offset 56; BEGIN uses CRC32 of the
entire transferred frame. These are different checksums.

CHUNK offsets must be contiguous. Duplicate/stale requests, duplicate chunks,
oversized frames and mismatched transactions are rejected. Receive/validated
transactions expire after 20 seconds without a chunk; programming has a 30-second
deadline plus the existing bounded NOR operation timeout. The writer advances
cooperatively without requiring PC polling. UART TX uses the bounded nonblocking
capture queue. No programming occurs in an ISR.

Installation requires SAFE, excitation off, range disabled, no quiet acquisition,
valid service safety permissions and exclusive CALIBRATION_STORE workspace
ownership. Captures and installations exclude each other. The single existing
3072-byte workspace holds the candidate; the decoded store scratch is reused.

## Validation, activation and recovery

Before erase, both PC and device validate complete schema v2/model v4/Rev.1
frames, 33 unique allowed keys, IDs, CRC, positive finite ADC scales, finite
nondegenerate OSL/HG coefficients, flags and reserved metadata. Device validation
is authoritative. The service uses sequence 1 on blank media and exactly the
usable active sequence + 1 afterwards. UINT32_MAX blocks further installation;
there is no rollover. Rebuild a candidate with the queried successor rather than
replaying an old frame. The older generic low-level writer API retains historical
behavior for its existing tests; both application-service commit entry points use
the explicit bound policy.

Recovery selects the newest fully usable set, ignoring a newer incompatible,
incomplete or corrupt slot. Writes target the opposite usable slot, preserving
the only usable record. The existing A/B state machine programs header/payload,
checks uncommitted bytes in 128-byte read blocks, writes the commit marker last,
then decodes and checks CRC/sequence/coverage before activation. A new OSL
sequence/CRC makes old supplementary-curve bindings unusable through existing
binding checks; overlays are never automatically requalified.

ABORT before programming releases staging. During erase/program it drains the
pending bounded NOR operation before releasing ownership. Once the marker command
has been issued, ABORT returns TOO_LATE and verification continues: a completed
commit cannot honestly be reported as rolled back. After a lost response, query
STATUS and readback. Never blindly retry BEGIN/COMMIT. The PC client fails closed
on ambiguous transport errors; it does not silently repeat writes.

## Reproducible synthetic demonstration

```powershell
cmake --preset host-debug
cmake --build --preset host-debug
python tools/pc_osl_install.py simulate --bridge build/host-debug/tests/Debug/wtk_pc_cal_capture_bridge.exe --output build/serial-provisioning-demo
```

Use an empty output directory. This collects 99 observations through the actual C
parser/session/DSP, solves 33 conditions, transfers and validates the candidate,
executes the real C store with a fault-injectable NOR adapter, verifies readback,
resets the simulated device and applies the persisted model through
`measurement_cal_process_block`. Hardware acquisition and NOR electrical behavior
are mocked. Every result is synthetic, `REQUIRES_BENCH_VALIDATION`.
