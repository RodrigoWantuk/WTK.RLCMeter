# `app`

Camada de aplicação do firmware.

## Responsabilidades

- state machine global;
- orchestration entre safety, measurement, storage e UI;
- fila/dispatch de eventos;
- política de retry/rerange;
- faults globais;
- versionamento de firmware/hardware exposto à UI.

## Não deve fazer

- acessar GPIO diretamente;
- falar com ILI9341/W25Q diretamente;
- calcular fasores/impedância;
- energizar relés sem passar por `hardware`.

## Arquivos planejados

```text
app_state_machine.c/.h
app_events.c/.h
app_context.c/.h
app_scheduler.c/.h
app_faults.c/.h
app_version.c/.h
```

## Invariantes

- todo caminho que entra em MEASURE possui transição explícita de volta a SAFE;
- faults críticos interrompem excitação e solicitam SAFE imediatamente;
- nenhuma transição depende de delay bloqueante;
- o estado atual deve ser visível ao diagnóstico.
