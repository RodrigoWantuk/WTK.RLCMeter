# `hardware`

Serviços de hardware específicos do instrumento.

## Responsabilidades

```text
hw_safety.c/.h
hw_relays.c/.h
hw_range.c/.h
hw_excitation.c/.h
hw_power.c/.h
hw_battery.c/.h
hw_temperature.c/.h
hw_backlight.c/.h
hw_buzzer.c/.h
```

Esta camada transforma GPIO/periféricos do BSP em operações semanticamente seguras.

## Exemplos de API

```text
safety_force_safe()
safety_measure_allowed()
relay_set_safe()
relay_set_measure()
range_disable()
range_select()
excitation_configure()
excitation_stop()
charger_connected()
battery_read()
temperature_read()
backlight_set()
buzzer_play()
```

## Invariantes

- K1 desenergizado é SAFE;
- `RANGE_EN=0` durante troca de endereço;
- 500 mVrms não é permitido com RREF de 10 Ω;
- `CHG_VBUS` ativo impede MEASURE;
- fault crítico para excitação e retorna K1 a SAFE;
- UI nunca aciona relé/range diretamente.

K2 é contingência do banco low-Z. O baseline físico usa `R0_BANK=0 Ω` e K2 DNP; o serviço deve permitir futura variante sem espalhar condicionais pelo restante do firmware.
