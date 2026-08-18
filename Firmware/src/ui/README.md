# `ui`

Interface gráfica do instrumento no ILI9341.

## Arquivos planejados

```text
ui_core.c/.h
ui_theme.c/.h
ui_format.c/.h
ui_navigation.c/.h
ui_widgets.c/.h
screen_startup.c/.h
screen_measure.c/.h
screen_details.c/.h
screen_graph.c/.h
screen_settings.c/.h
screen_calibration.c/.h
screen_diagnostics.c/.h
```

## Regras

- sem framebuffer full-screen;
- renderização incremental;
- sem delays bloqueantes para animação;
- bitmaps grandes em streaming da W25Q;
- UI não aciona relés ou ranges diretamente;
- formatação SI e unidades fica separada do cálculo metrológico;
- atualizações SPI pesadas são suspensas em quiet mode.

## Telas baseline

Startup, medição principal, detalhes de impedância/fase, gráficos, settings, calibração e diagnóstico.

## Interação

Três botões: UP, OK e DOWN, com press, long-press e repeat quando apropriado.
