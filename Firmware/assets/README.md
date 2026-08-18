# `assets`

Fontes dos assets gráficos usados pela UI.

## Conteúdo esperado

- splash/startup;
- ícones;
- fontes;
- imagens auxiliares;
- descrições/manifest de origem.

Arquivos desta pasta são fontes para processamento; o firmware consome o asset pack gerado pelas ferramentas de `tools/`.

## Regras

- não assumir framebuffer completo;
- preferir formatos fáceis de converter para RGB565/máscaras;
- manter licença/origem de qualquer asset de terceiros;
- assets devem receber IDs estáveis para não acoplar a UI a offsets físicos da Flash.
