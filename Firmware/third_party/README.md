# `third_party`

Dependências externas usadas pelo firmware.

## Regras

- manter cada dependência isolada e identificável;
- registrar versão/commit de origem;
- preservar arquivos de licença e notices exigidos;
- evitar copiar código de terceiros diretamente para módulos próprios;
- preferir dependências pequenas, permissivas e adequadas a firmware embarcado.

CMSIS/HAL gerenciados pelo fluxo de build podem ser referenciados externamente em vez de vendorizados, desde que o build continue reproduzível.
