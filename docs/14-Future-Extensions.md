# Future Extensions

This document records ideas discussed for later revisions without turning them into Rev.1 requirements.

## Rule

A feature listed here must not be described as an available capability of the current hardware. Any change that modifies the analog path, safety envelope, isolation, or parasitics should create a new hardware identity and require new calibration/qualification.

## Kelvin / 4-wire measurement

Rev.1 is two-wire.

A Kelvin revision could improve low-resistance measurement by removing much of the contribution from:

- lead resistance;
- terminal/contact resistance;
- connector and trace resistance.

Expected impact:

- four external terminals;
- separate FORCE/SENSE routing;
- revised relay/switch matrix;
- more complex input protection;
- new OPEN/SHORT/LOAD strategy;
- UI support for 2-wire/4-wire modes;
- revised autorange/confidence logic.

This cannot be added to Rev.1 by firmware alone.

## Direct high-voltage measurement

A future separate input has been discussed for roughly:

- AC up to ~400 Vrms;
- DC in the ~600–800 V range;
- while preserving a separate path capable of small-signal measurements.

This is **outside Rev.1**.

A serious implementation requires a dedicated front-end and full safety review, potentially including:

- distributed high-voltage divider networks;
- resistor voltage-rating analysis;
- surge/transient protection;
- appropriate creepage and clearance;
- dedicated connectors/enclosure design;
- physical segregation from the impedance AFE;
- controlled discharge;
- power/failure analysis;
- possibly galvanic isolation;
- a new validation and safety-classification process.

The current residual-voltage SAFE network is **not** a high-voltage voltmeter front-end.

## Wide-range voltage input

A future voltage-input concept should retain useful resolution for small signals while also supporting high-voltage ranges. A single fixed divider is unlikely to satisfy both goals well.

Possible future approaches:

- multiple switchable divider ratios;
- buffered ranges;
- voltage-specific autorange;
- input protection separate from the RLC terminals;
- per-range calibration.

A high-voltage/small-signal voltage input should use connectors distinct from the RLC DUT terminals to avoid safety ambiguity.

## Additional test frequencies

After 100 Hz, 1 kHz, and 10 kHz are qualified, more frequencies may be added if the hardware demonstrates adequate margin.

Before doing so, measure:

- excitation-filter response;
- MCP6002 magnitude/phase response;
- ADC skew/timing;
- switch capacitance;
- PCB parasitics;
- DUT behavior across the proposed range.

Adding frequencies without calibration/qualification is not acceptable.

## Additional excitation levels

100 mVrms and 500 mVrms are the current baseline.

Future levels may be added only if:

- analog current remains safe;
- VEXC/RET headroom is sufficient;
- distortion remains acceptable;
- the level is included in calibration/qualification.

## External ADC

An external ADC was intentionally rejected for Rev.1 because of cost, availability, and prototype complexity.

Reopen that decision only if measured evidence shows the internal ADCs remain the dominant limitation after:

- deterministic sampling;
- appropriate averaging/oversampling where useful;
- calibration;
- noise/layout improvements;
- correct range/gain selection.

## Future MCU

Blue Pill remains the Rev.1 baseline. A future MCU migration may be justified by needs such as:

- more RAM for richer graphics or larger buffers;
- better ADCs;
- more timers/DMA channels;
- native USB without pin conflicts;
- larger internal Flash;
- more DSP margin.

The first prototype should produce evidence before a migration is considered.

## Native USB

PA11/PA12 are occupied in the current hardware, so Rev.1 has no native USB device interface.

A future pinout may restore D-/D+ and enable:

- CDC console;
- calibration/log export;
- firmware update workflows;
- PC application integration.

## Larger storage / filesystem

Rev.1 uses a simple W25Q layout without a filesystem.

If future requirements include many assets, logs, or profiles, candidates include:

- higher-density Flash;
- a lightweight filesystem;
- improved asset compression;
- PC-driven asset-pack updates.

The initial implementation should remain simple and deterministic.

## Active guard

The PCB may contain provisions for guard experiments, but the baseline keeps such paths unpopulated/disconnected until high-Z leakage measurements justify them.

If leakage is the dominant high-Z error, guard options can be evaluated through controlled A/B measurements before becoming standard population.

## K2 low-Z isolation

K2 is a physical contingency for isolating the low-Z bank if its parasitic capacitance harms high-impedance measurements.

Baseline:

```text
R0_BANK = 0 Ω populated
K2      = DNP
```

Only measured leakage/parasitic evidence should justify changing this population strategy.

## Optional TVS

The terminal TVS option remains DNP initially because capacitance and leakage can degrade metrology.

A more robust future variant may populate it only after measuring the metrology cost of the selected device.

## Promotion criteria

A future extension becomes an active project requirement only after documenting:

1. explicit requirement;
2. electrical architecture;
3. safety impact;
4. PCB/BOM impact;
5. firmware impact;
6. calibration strategy;
7. acceptance criteria;
8. new hardware identification where applicable.
