# Roadmap

## M0 — Hardware Rev.1

- fabricar PCB;
- montar por blocos;
- validar shorts/rails;
- fechar BOM real de montagem;
- registrar DNP.

## M1 — Firmware bootstrap

- toolchain;
- startup/clock;
- GPIO SAFE;
- JTAG remap;
- UART;
- watchdog;
- diagnostics base.

## M2 — UI e storage

- SPI2;
- ILI9341;
- W25Q JEDEC/read/write;
- asset pack;
- fonts;
- buttons;
- backlight;
- buzzer.

## M3 — Safety e range control

- ADC residual;
- battery/NTC;
- charger detect;
- K1/K2 services;
- 74HC238 range selection;
- state machine SAFE/READY/MEASURE.

## M4 — Excitação e aquisição

- TIM1 PWM carrier;
- amplitude generation;
- ADC1/ADC2;
- DMA;
- trigger timer;
- raw capture/stream via UART;
- quiet mode.

## M5 — DSP e impedância

- I/Q;
- channel reconstruction;
- complex Z;
- R/X/phase;
- L/C series equivalents;
- clipping/SNR/confidence.

## M6 — Autorange

- range search;
- amplitude policy;
- 1×/HG selection;
- settling;
- retry/fallback.

## M7 — Calibração

- OPEN/SHORT/LOAD wizard;
- persistent records;
- complex correction;
- calibration migration/versioning.

## M8 — Produto Rev.1

- UI completa;
- gráficos;
- battery UX;
- diagnostics console;
- power/idle policy;
- qualification matrix.

## M9 — Decisão Rev.2

A Rev.2 só deve ser desenhada com dados da Rev.1.

Possíveis itens:

- melhorar high-Z leakage/Coff;
- 4-wire/Kelvin;
- MCU/pinout com USB nativo disponível;
- power control mais integrado;
- proteção TVS qualificada;
- redução de tamanho;
- frontend separado para medição de tensão.

### Fora do escopo Rev.1

Foi discutida futura medição de sinais pequenos e também tensão AC/DC elevada, incluindo aproximadamente 400 Vrms e 600–800 VDC. Isso exige novo frontend, conectores separados, novos clearances e análise de segurança. Não deve ser implementado por extensão improvisada da entrada RLC atual.
