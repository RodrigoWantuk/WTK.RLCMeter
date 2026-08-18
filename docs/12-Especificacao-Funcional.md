# Especificação funcional

Este documento consolida o comportamento esperado do WTK.RLCMeter sem confundir **meta de projeto**, **feature implementada** e **capacidade já qualificada**.

## Estados de maturidade

- **PLANNED** — arquitetura decidida, ainda não implementada.
- **IMPLEMENTED** — código/hardware existe, ainda não necessariamente qualificado.
- **QUALIFIED** — comportamento medido e aceito contra critérios definidos.
- **EXPERIMENTAL** — recurso disponível para ensaio, sem compromisso de produto.

No estágio atual, grande parte das funções abaixo está entre PLANNED e primeiro bring-up.

## Escopo da Rev.1

### Medição

- impedância complexa de componentes passivos desenergizados;
- operação em dois fios;
- cálculo de `R + jX`;
- derivação de resistência, capacitância e indutância;
- magnitude `|Z|` e fase;
- ESR, Q e D quando matematicamente e metrologicamente válidos;
- frequências baseline de 100 Hz, 1 kHz e 10 kHz;
- amplitudes baseline de 100 mVrms e 500 mVrms;
- seleção automática entre seis RREF;
- seleção automática entre `RET_1X` e `RET_HG`;
- rerange/retry quando a primeira tentativa não tiver qualidade suficiente.

### Metas de alcance

Metas de projeto para a primeira caracterização:

| Grandeza | Meta de alcance |
|---|---:|
| R | ~1 Ω a 10 MΩ |
| C | ~1 nF a 10 mF |
| L | ~10 µH a 10 H |
| Frequências | 100 Hz / 1 kHz / 10 kHz |
| Excitação | 100 mVrms / 500 mVrms |

Esses números não são ainda garantias de precisão. A qualificação definirá subfaixas reais e condições permitidas.

## Regras de excitação

- 500 mVrms é proibido no RREF de 10 Ω;
- a amplitude pode ser reduzida por política de current/headroom;
- a frequência pode ser alterada automaticamente para melhorar observabilidade;
- combinações não qualificadas não devem aparecer como leituras normais de alta confiança.

## Confidence

Cada resultado contém estado de qualidade. Exemplos de causas de degradação:

- ADC clipping;
- SNR insuficiente;
- instabilidade entre blocos;
- fase incoerente;
- excitação fora da janela esperada;
- saturação do AFE;
- proximidade excessiva de OPEN/SHORT;
- combinação range/frequência/amplitude não qualificada;
- tensão residual;
- carregador conectado;
- falha de calibração para a chave ativa.

Classes planejadas:

- `NOMINAL` — região principal qualificada;
- `EXTENDED` — leitura utilizável fora da região principal;
- `LOW_CONFIDENCE` — resultado disponível somente como diagnóstico/indicação;
- `REJECTED` — resultado não publicado como medição.

## Autorange

Banco:

```text
10 Ω
100 Ω
1 kΩ
10 kΩ
100 kΩ
1 MΩ
```

A escolha não é feita apenas pela ordem de grandeza do DUT. O algoritmo considera headroom, corrente, SNR, canal 1X/HG, frequência, amplitude e qualificação.

Toda troca de RREF deve desabilitar o banco antes de mudar endereço:

```text
RANGE_EN=0 -> A0/A1/A2 -> dead-time -> RANGE_EN=1 -> settling
```

## Segurança

A Rev.1 deve permanecer SAFE por default.

MEASURE só é permitido quando:

- boot/self-test relevante foi concluído;
- não há tensão residual acima do limiar;
- `CHG_VBUS` não está ativo;
- range selecionado é válido;
- nenhuma condição crítica de alimentação/fault está presente.

Falha ou reset deve desenergizar K1 e retornar a SAFE.

A rede residual possui envelope aproximado de observação de ±100 V, mas isso **não autoriza medição energizada** e não representa rating CAT.

## Alimentação e bateria

A carrier recebe de um módulo externo de bateria/carga/boost:

```text
VBAT_PROT
+5V_SYS
GND
CHG_VBUS
```

Funções planejadas:

- estimativa de nível de bateria;
- aviso de bateria baixa;
- indicação de carregador;
- bloqueio de MEASURE durante carga;
- dimming de backlight por política de energia;
- shutdown/estado seguro em alimentação inadequada, conforme o hardware permitir.

## UI

Telas planejadas:

1. startup/splash;
2. medição principal;
3. detalhes de impedância;
4. visualizações/gráficos;
5. seleção/configuração;
6. calibração;
7. diagnóstico;
8. informações do equipamento/firmware.

A tela principal deve priorizar legibilidade da grandeza dominante, unidade, frequência, qualidade e status de segurança.

## Botões

Três botões:

- UP;
- OK;
- DOWN.

Eventos:

- press;
- release;
- long press;
- repeat quando aplicável.

Encoder não faz parte da Rev.1.

## Feedback sonoro

Piezo passivo remoto, controlado por PB1 via transistor.

Padrões planejados:

- confirmação curta;
- ação inválida;
- conclusão de medição;
- alerta de tensão residual;
- bateria baixa;
- fault crítico.

O buzzer permanece desligado durante aquisição metrológica.

## Backlight

PB0 controla PWM do TFT.

Políticas planejadas:

- brilho configurável;
- auto-dimming por inatividade;
- duty estável durante aquisição;
- eventual congelamento em condição conhecida caso acoplamento seja observado em bancada.

## TFT e assets

Display ILI9341 em SPI compartilhado com W25Q.

Requisitos:

- sem framebuffer full-screen;
- renderização incremental;
- assets grandes na Flash externa;
- leitura em blocos pequenos;
- nenhum erase/program de Flash no meio de aquisição crítica;
- CS de TFT e Flash mutuamente controlados.

## Diagnóstico

A build de laboratório deve expor:

- firmware/hardware revision;
- uptime/reset reason;
- estado da state machine;
- valores ADC crus;
- VEXC/VMID/RET estimados;
- RREF ativo;
- estado de K1/K2;
- bateria, NTC e `CHG_VBUS`;
- JEDEC ID/status da W25Q;
- clipping, SNR e confidence;
- erros persistentes e últimos eventos.

## Calibração

Fluxos planejados:

- OPEN;
- SHORT;
- LOAD quando necessário à estratégia final;
- correção complexa por frequência/range/amplitude;
- registros versionados por revisão de hardware;
- CRC e estratégia de escrita resiliente a perda de energia.

A calibração corrige erros estáveis e repetíveis. Não corrige contato instável, cabo movimentando, umidade variável ou leakage imprevisível.

## Features explicitamente fora da Rev.1

- USB device nativo;
- medição CAT-rated;
- medição direta de rede elétrica;
- alta tensão AC 400 Vrms;
- alta tensão DC 600–800 V;
- Kelvin/4-wire;
- ADC externo;
- RTOS obrigatório;
- filesystem completo na Flash.

Algumas dessas ideias são discutidas em [`14-Extensoes-Futuras.md`](14-Extensoes-Futuras.md).
