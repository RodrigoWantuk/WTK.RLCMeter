# Experimental unit NTC factory calibration

The optional `WTK_NTC_FACTORY_PROFILE` CMake FILEPATH selects a versioned
`WTK_NTC_FACTORY_PROFILE_V1` JSON file. Without it, the original nominal
100-kohm/B3950 conversion table is unchanged. All PRODUCT, BRINGUP and BRINGUP_CAL
variants can select the same unit profile.

The current engineering profile is
`Firmware/config/units/54ff70068370525760380567-ntc-engineering.json`.
It fits the owner's 23.5-degC/ADC-1952 and 30.1-degC/ADC-1856 references. The owner
explicitly accepted possible contact interference. The earlier ADC-1877 warm
sample is transient diagnostic evidence, not a fit anchor. The effective Beta
and R25/Rtop ratio describe these supplied observations; they are not verified
intrinsic thermistor specifications. Reference uncertainty is unknown, not zero.
The thermometer model is intentionally not recorded.

The PC fits `ln(raw/(4095-raw))` against reciprocal absolute temperature. This
avoids assuming that Rtop or R25 was independently measured. The generator emits
21 monotonic float32 table points, including the two reference temperatures as
exact nodes. Existing bounded linear interpolation remains in the MCU. There
is no runtime log, extra acquisition buffer or external-Flash partition.

From `Firmware/` in PowerShell, quote the complete cache argument:

```powershell
cmake --preset stm32-bringup-cal -B build/unit-cal '-DWTK_NTC_FACTORY_PROFILE=config/units/54ff70068370525760380567-ntc-engineering.json'
cmake --build build/unit-cal
cmake --preset stm32-release -B build/unit-product '-DWTK_NTC_FACTORY_PROFILE=config/units/54ff70068370525760380567-ntc-engineering.json'
cmake --build build/unit-product
```

The normal deployment build helper accepts the equivalent option from the repo
root:

```powershell
python Firmware/tools/pc_firmware_deploy.py build --profile BRINGUP_CAL --out Firmware/build/unit-deployment --ntc-factory-profile Firmware/config/units/54ff70068370525760380567-ntc-engineering.json
```

This option must be selected again for future firmware updates to this unit.
The calibration persists as table data in the internal MCU image; it is not a
runtime setting or W25Q record. Recalibration requires rebuild/reflash. Generic
images keep the nominal table and do not preserve this unit-specific adjustment.

Unit manifests use `WTK_FIRMWARE_2`, which older deployment tools reject. The
manifest seals the profile, profile SHA and expected float32 table. Inspection
regenerates that table and requires its bytes exactly once in the linked binary.
Programming requires the explicit matching MCU UID before any backend operation.
Readback/BIN identity establishes byte continuity, not physical temperature
accuracy. Manual flashing tools cannot enforce the manifest binding.

This engineering format deliberately rejects `qualified: true`. No extrapolated
temperature or the wider -20..80-degC table domain is physically qualified by two
owner-supplied points. Contact uncertainty, an independent third temperature and
full-range checks remain REQUIRES_BENCH_VALIDATION. No firmware claim of qualified
temperature accuracy is introduced. Battery conversion, residual checks, K1,
range control, acquisition and OSL equations are unchanged. The old source-7017
OSL observations remain preserved diagnostic evidence; three high-Z/10-kHz fits
are still unresolved and no OSL set is installed.
