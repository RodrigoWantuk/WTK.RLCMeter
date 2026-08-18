# PCB

This directory contains the engineering and manufacturing artifacts for the WTK.RLCMeter PCB.

Circuit behavior is documented under [`../docs`](../docs). This directory should contain the **actual EasyEDA Pro exports** and the manufacturing artifacts used for each hardware revision.

## Recommended layout

```text
PCB/
├── README.md
├── source/                 # complete EasyEDA Pro project export
├── fabrication/
│   ├── Rev1/
│   │   ├── Gerber_*.zip
│   │   ├── BOM_*.csv
│   │   └── PCB_*.pdf
│   └── Rev2/
├── renders/                # top/bottom views, 3D renders, 1:1 images
└── revisions/              # physical/electrical revision notes
```

The exact filenames are not mandatory, but source, fabrication output, renders, and revision history should remain clearly separated.

## Rev.1 baseline

- Two-layer FR-4 PCB.
- Manual assembly is a design priority.
- Passives are predominantly 0805; 1206 is used where voltage, power, low impedance, or mechanical robustness justify it.
- No switching-bank device smaller than SOT-23 in the current baseline.
- Blue Pill is mounted as a THT module.
- Ground copper pour, especially on the bottom layer.
- 33 Ω series resistors on SPI SCK and MOSI.
- `D_TVS` and `R_TVS_LINK` remain DNP in the first assembly.
- K2 is a contingency option; the baseline uses `R0_BANK = 0 Ω`.

## Reference fabrication rules

These are project baselines and do not replace EasyEDA DRC or the fabrication vendor's limits.

| Rule | Baseline |
|---|---:|
| General signal trace | 0.25–0.30 mm |
| VEXC / RET / VMID | ~0.8 mm where practical |
| LOWZ_BUS | ~1.0 mm where practical |
| +3V3 / +5V_A | ~0.6 mm |
| +5V_SYS | ~0.8 mm |
| General clearance | 0.25 mm |
| SAFE / residual-voltage area | larger clearance, target ≥1.0 mm in sensitive areas |
| Signal via | 0.80 / 0.30 mm |
| Power via | 1.00 / 0.40 mm |

## Checklist before publishing a revision

1. Synchronize the PCB from the schematic.
2. Run DRC and resolve shorts, open nets, and critical violations.
3. Verify Blue Pill and connector pinout/orientation against the physical parts.
4. Verify board outline and drills in a Gerber viewer.
5. Verify top and bottom solder mask.
6. Re-export Gerbers after every PCB change.
7. Export a BOM matching the exact same revision.
8. Record all DNP components for the intended assembly.
9. Update documentation if pinout, safety behavior, or analog behavior changes.

## Blue Pill USB limitation

In Rev.1, PA11 and PA12 are reused by board functions (`K2_CMD` and `FLASH_CS` in the current pinout). These pins are the STM32F103 native USB D-/D+ pins, so the Blue Pill Micro-USB connector must **not** be treated as an available native USB interface in this revision.

See [`../docs/05-Pinout-and-Interfaces.md`](../docs/05-Pinout-and-Interfaces.md).
