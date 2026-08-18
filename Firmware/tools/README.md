# `tools`

Ferramentas executadas no PC para apoiar firmware e calibração.

## Ferramentas planejadas

- conversor de PNG/fontes para assets RGB565/máscaras;
- asset packer com manifest, offsets e CRC;
- gerador/validador de tabelas de calibração;
- parser de logs UART;
- scripts de análise de sessões e comparação com instrumentos de referência;
- geração de vetores para testes host-side.

Ferramentas devem ser determinísticas e, quando possível, registrar versão/formato usado para gerar artefatos consumidos pelo firmware.
