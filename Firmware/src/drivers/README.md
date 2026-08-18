# `drivers`

Drivers de dispositivos externos e abstrações pequenas de periféricos.

## Baseline

```text
ili9341.c/.h
w25q.c/.h
buttons.c/.h
spi_bus.c/.h       # se necessário para arbitrar TFT/Flash
crc32.c/.h         # se não vier de outra camada comum
```

## ILI9341

O driver expõe operações de baixo nível: init, reset, leitura de ID/status, rotation, window, fill e envio de pixels RGB565.

Não conhece telas, unidades, menus ou resultados RLC.

## W25Q

O driver expõe JEDEC ID, read/fast-read, page program, sector erase, status e wait-ready.

Deve reconhecer densidades compatíveis W25Q16/32/64/128 sem codificar o tamanho da W25Q64 em todo o firmware.

## SPI compartilhado

TFT e Flash compartilham SCK/MOSI/MISO e possuem CS independentes.

Invariantes:

- nunca selecionar os dois devices ao mesmo tempo;
- evitar transações longas durante quiet mode;
- TFT pode ser atualizado incrementalmente;
- erase/program de Flash não ocorre no meio de aquisição crítica.

## Buttons

Debounce transforma GPIO em eventos `PRESS`, `RELEASE`, `LONG_PRESS` e `REPEAT`, sem incorporar navegação de UI.
