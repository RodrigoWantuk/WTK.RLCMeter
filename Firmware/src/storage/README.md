# `storage`

Persistência sobre a Flash SPI externa W25Q.

## Responsabilidades

```text
storage_layout.c/.h
record_store.c/.h
asset_store.c/.h
settings_store.c/.h
calibration_store.c/.h
```

## Baseline

Sem filesystem na primeira versão. A Flash é dividida em regiões lógicas para assets, settings e calibração.

Todo record persistente deve possuir identificação, versão, tamanho e CRC. Settings/calibração devem usar slots redundantes ou journal simples para sobreviver a perda de energia durante escrita.

## Regras

- nunca assumir que conteúdo de Flash é válido sem verificação;
- schema de calibração precisa carregar `hardware_revision`;
- versões antigas devem ser rejeitadas ou migradas explicitamente;
- erase/program não deve ocorrer durante aquisição crítica;
- endereço/tamanho da W25Q não deve ser hard-coded fora do layout/driver.

## Asset store

Fornece acesso por ID a bitmaps/fontes empacotados, permitindo streaming em blocos para o TFT sem framebuffer completo.
