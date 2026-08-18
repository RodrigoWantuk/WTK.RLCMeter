# `config`

Configurações versionadas e defaults de build/firmware.

## Conteúdo planejado

- feature flags de laboratório;
- defaults de UI;
- parâmetros de logging;
- identificação de revisão de hardware;
- parâmetros não metrológicos que precisam existir mesmo com Flash inválida.

Configurações de calibração não pertencem aqui como constantes mágicas: dados medidos devem ficar em records persistentes/versionados.

Nenhuma configuração de usuário pode desabilitar intertravamentos de segurança em builds normais.
