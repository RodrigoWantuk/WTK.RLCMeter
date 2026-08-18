# PCB

Esta pasta recebe os arquivos de engenharia e fabricação da PCB do WTK.RLCMeter.

A documentação técnica do funcionamento do circuito fica em [`../docs`](../docs). Este diretório deve conter os arquivos **reais exportados do EasyEDA Pro** e os artefatos usados para fabricar cada revisão.

## Estrutura sugerida

```text
PCB/
├── README.md
├── source/                 # export completo do projeto EasyEDA Pro
├── fabrication/
│   ├── Rev1/
│   │   ├── Gerber_*.zip
│   │   ├── BOM_*.csv
│   │   └── PCB_*.pdf
│   └── Rev2/
├── renders/                # vistas Top/Bottom, 3D e imagens 1:1
└── revisions/              # notas de alterações físicas por revisão
```

A estrutura é uma recomendação; não há obrigação de preservar exatamente os nomes acima.

## Baseline Rev.1

- 2 layers, FR-4.
- Montagem manual.
- Passivos predominantemente 0805; 1206 onde tensão, potência ou robustez justificam.
- Sem componentes menores que SOT-23 no banco de chaveamento atual.
- Blue Pill montada como módulo THT.
- Plano/copper pour de GND, especialmente no Bottom.
- Resistores série de 33 Ω em SPI SCK e MOSI.
- `D_TVS` e `R_TVS_LINK` devem permanecer DNP na primeira montagem.
- K2 é contingência; o baseline usa `R0_BANK = 0 Ω`.

## Regras de fabricação usadas como referência

Valores abaixo são baseline de projeto, não substituem o DRC do arquivo EasyEDA:

| Regra | Baseline |
|---|---:|
| Track de sinal | 0,25–0,30 mm |
| VEXC / RET / VMID | ~0,8 mm quando possível |
| LOWZ_BUS | ~1,0 mm quando possível |
| +3V3 / +5V_A | ~0,6 mm |
| +5V_SYS | ~0,8 mm |
| Clearance geral | 0,25 mm |
| SAFE / tensão residual | clearance maior, alvo ≥1,0 mm na região sensível |
| Via de sinal | 0,80 / 0,30 mm |
| Via de potência | 1,00 / 0,40 mm |

## Checklist antes de publicar uma revisão

1. Sincronizar PCB a partir do esquemático.
2. DRC sem shorts, nets abertas ou violações críticas.
3. Conferir pinout/orientação física da Blue Pill e conectores.
4. Conferir Board Outline e drills no Gerber viewer.
5. Conferir Top/Bottom solder mask.
6. Exportar Gerber novamente após qualquer alteração de PCB.
7. Salvar BOM correspondente à mesma revisão.
8. Registrar componentes DNP da montagem.

## Observação sobre o USB da Blue Pill

Na Rev.1, PA11 e PA12 foram reutilizados como `K2_CMD` e `FLASH_CS`. Como esses pinos são D-/D+ do USB nativo do STM32F103, o conector Micro-USB da Blue Pill **não deve ser considerado uma interface USB disponível nesta revisão**. Consulte [`../docs/05-Pinout-e-Interfaces.md`](../docs/05-Pinout-e-Interfaces.md).
