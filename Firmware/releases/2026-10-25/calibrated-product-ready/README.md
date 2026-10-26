# Calibrated first-unit PRODUCT handoff

The first unit now runs the PC-provisioned PRODUCT release from source
8849cfa2c29a3a4a174f01692de22673ad385028. Internal Flash was programmed and read
back exactly. Controlled boot reaches READY (state 4), loads the complete active
sequence 1 with 33 calibration records, and admits the existing resources.
K1 is commanded SAFE, range/excitation OFF, quiet released, no safety faults,
and measurement session count 0. The raw range blocker remains 8 because the
range is actually disabled; no hardware permission is forged to make UI READY.
The passive serial banner is limited SAFE_BOOT; the additional readiness evidence
comes from compiler-ABI-derived metadata reads, not an invented PRODUCT protocol.
The owner confirms the splash and READY after replacing TTL, and reports a twice-painted READY background, slow painting and low brightness with correct colors. These UI issues are tracked for follow-up.

A power interruption corrupted a temporary ignored build report. Git integrity,
committed code and persisted calibration metadata survived. Nine strict UID-bound
images were rebuilt in a new directory. 46 standard and 47 PC-provisioned host
CTests pass. FACTORY_PRODUCT uses 54224 bytes Flash and 15568 bytes
accounted RAM. The experimental UID-bound NTC profile is preserved. No new
resistor/DUT measurement or calibration/resource installation was commanded.
The last battery voltage was 3.873 V through COM4 before the first PRODUCT image;
PRODUCT has no ordinary serial battery-query service. No battery voltage was
read through SWD. Post-power metadata checks distinguish supply state/age from
voltage. A failed preflight is preserved; the successful programming preflight
had battery state OK and no supply/charger/fault blocker.

## Scheduling correction

Application readiness now separates scheduling from the hardware permission to
energize K1. Actual idle disabled ranges can schedule preparation. Known
residual refresh/requalification and an owned measurement's ADC/range transitions
do not self-cancel that transaction. Charger, actual unsafe residual, supply and
fault flags retain their blocking paths; invalid idle ranges and unexplained
sensor failure remain blocked. The hardware FSM still issues/validates a fresh
single-use permit and dynamically aborts on range, charger, DMA or fault changes.
At PERMIT_ISSUE only, PRODUCT may wait for actual auxiliary refresh for at most
1,000 ms from the attempt's start. Expiry falls through to the unchanged strict
permit check. TFT work yields while a polled auxiliary group is active or residual
requalification is pending; fresh unsafe/invalid states can still render warnings.
BSP, hardware FSM, measurement math, ADC timing, calibration schema/model and
stored coefficients are unchanged. No recapture is required for these app edits.

## Actual feature status

| Feature | Current implementation and limit |
| --- | --- |
| Normal controls | Short OK requests one Click transaction; UP/DOWN browse results; long OK opens menus. No automatic measurement at boot. |
| C/L | Complex calibrated impedance first, then series-equivalent C or L when sign/classification/quality support it. Primary value plus R/X/magnitude/phase details and conditional capacitor AC ESR. The PC-provisioned PRODUCT details page also shows conditional Q/D, with n/a when unsupported. |
| Capacitors | Basic nonpolar 10-nF response was observed before OSL. This handoff performs no new physical C validation. Polarized/electrolytic operation still needs a defined, qualified bias/excitation method. |
| Inductors | Basic 470-uH positive-reactance response was observed before OSL. The earlier 2.2-uH estimate was unreliable. Small-L resolution/accuracy and losses remain unqualified. |
| Autorange | Six references (10 ohm through 1 Mohm), 1X/HG quality selection, nominal 100/500-mVrms policy; 10 ohm + 500 mVrms prohibited. Starts at 1 kohm/1 kHz/100 mVrms unless reusing the previous condition hint. At most six attempts/four range moves/one frequency refinement. Each acquisition returns to SAFE. |
| Frequency search | Fresh Click begins at 1 kHz and can inspect 10 kHz for reactive classification. 10 Hz exists for fixed-condition acquisition/calibration but is not automatically selected by the current fresh Click policy. This is not an exhaustive three-frequency sweep. |
| Live | Enum/design exist; continuous Live is not enabled in the PRODUCT UI. Autorange runs inside each individual Click. |
| Qualification | Installed engineering OSL is not independent physical accuracy qualification. No QUALIFIED flags, certified uncertainty or release accuracy are inferred. |

firmware-images.zip contains all nine source-bound images/configurations/maps,
the guarded FACTORY_PRODUCT manifest and check logs. physical-evidence.zip
preserves post-power checks, the refused preflight, successful programming/readback,
boot metadata and exact procedures. The earlier failed pilot remains separately
archived in ../calibrated-product-pilot/. Final test-lead, independent R/C/L,
phase/timing/power and thermal qualification remain REQUIRES_BENCH_VALIDATION.
