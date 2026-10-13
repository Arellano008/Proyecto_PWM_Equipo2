# 5. Comparación técnica RP2040 vs RP2350 (periférico PWM)

No basta con que el programa funcione en las dos tarjetas: esta tabla muestra **qué cambia en registros, direcciones, campos y código**. Todos los valores están verificados en compilación ([`verifica_mapa.c`](../firmware/src/verifica_mapa.c)) y se pueden ver en vivo con `pwm info` y `pwm regs`.

## 5.1 Plataforma

| | Raspberry Pi Pico | Raspberry Pi Pico 2 W |
|---|---|---|
| Chip | RP2040 (2021) | RP2350A (2024) |
| CPU | 2× Cortex-M0+ | 2× Cortex-M33 (o 2× Hazard3 RISC-V) |
| `clk_sys` por defecto (SDK) | **125 MHz** | **150 MHz** |
| SRAM | 264 KB | 520 KB |
| GPIO en banco 0 | 30 | 30 (RP2350A) · 48 (RP2350B) |
| `PICO_BOARD` | `pico` | `pico2_w` |
| Familia UF2 | `rp2040` | `rp2350-arm-s` |

## 5.2 Diferencias en el PWM

| Aspecto | RP2040 | RP2350 | Consecuencia en nuestro código |
|---|---|---|---|
| **Dirección base PWM** | `0x40050000` | `0x400A8000` | `PWM_BASE_ADDR` distinto en `plataforma.h` |
| **Número de slices** | 8 | 12 | `PWM_NUM_SLICES`; GPIO4 sigue en slice 2 |
| **Mapeo GPIO → slice** | `(gpio >> 1) & 7` | igual para GPIO 0–31; GPIO 32–47 → slices 8–11 | sin cambio para GP4 |
| Registros por slice (`CSR, DIV, CTR, CC, TOP`) | offsets `0x00–0x10`, stride `0x14` | **idénticos** | mismo código de configuración |
| Campos de `CSR` y `DIV` | EN, PH_CORRECT, A/B_INV, DIVMODE, PH_RET/ADV · DIV 8.4 | **idénticos** | mismas máscaras |
| **`EN`** | `0xA0`, bits [7:0] | **`0xF0`**, bits [11:0] | offset distinto |
| **`INTR`** | `0xA4` | **`0xF4`** | offset distinto |
| **Interrupciones** | 1 línea: `INTE/INTF/INTS` (`0xA8–0xB0`), IRQ 4 | 2 líneas: `IRQ0_INTE/INTF/INTS` (`0xF8–0x100`) e `IRQ1_*` (`0x104–0x10C`), IRQ 8 y 9 | con SDK: `pwm_set_irq_enabled` vs `pwm_set_irq0_enabled`/`pwm_irqn_*` |
| **Valor de `DIV` para 100 Hz** (TOP 9999) | 125.0 → `0x7D0` | 150.0 → `0x960` | se calcula con `clock_get_hz(clk_sys)` |
| Frecuencia mínima con TOP 9999 | 48.8 Hz | 58.6 Hz | validación en `pwm freq` |

## 5.3 Diferencias en los bloques de soporte

| Aspecto | RP2040 | RP2350 | Consecuencia |
|---|---|---|---|
| **RESETS base** | `0x4000C000` | `0x40020000` | |
| **Bit PWM en `RESETS.RESET`** | **14** | **16** | `RESETS_PWM_BIT` |
| **IO_BANK0 base** | `0x40014000` | `0x40028000` | offsets `GPIOx_CTRL` iguales (`8x+4`) |
| `FUNCSEL` PWM / SIO | 4 / 5 | 4 / 5 | igual |
| **PADS_BANK0 base** | `0x4001C000` | `0x40038000` | offsets iguales (`4+4x`) |
| **Bit `ISO` en pads** | no existe | **bit 8, arranca en 1** | hay que limpiarlo o el pin no sale (`#if PADS_TIENE_ISO`) |
| SIO base | `0xD0000000` | `0xD0000000` | igual |
| **`GPIO_OUT_SET` / `CLR` / `OE_SET`** | `+0x14` / `+0x18` / `+0x24` | `+0x18` / `+0x20` / `+0x38` | offsets distintos (RP2350 intercala registros `GPIO_HI_*`) |
| Seguridad | — | ARM TrustZone / `ACCESSCTRL`: los periféricos pueden restringirse a modo Secure | nuestro binario corre en Secure (`rp2350-arm-s`), acceso permitido |

## 5.4 Qué cambia en el código

### Con punteros

Todo lo que cambia está aislado en **un solo archivo** con `#if PICO_RP2040 / #elif PICO_RP2350` ([`plataforma.h`](../firmware/src/plataforma.h)):

```c
#if PICO_RP2040
#define PWM_BASE_ADDR          0x40050000u
#define RESETS_PWM_BIT         14u
#define PWM_EN_OFS             0x0A0u
#define SIO_GPIO_OUT_SET_OFS   0x014u
#define PADS_TIENE_ISO         0
#elif PICO_RP2350
#define PWM_BASE_ADDR          0x400A8000u
#define RESETS_PWM_BIT         16u
#define PWM_EN_OFS             0x0F0u
#define SIO_GPIO_OUT_SET_OFS   0x018u
#define PADS_TIENE_ISO         1
#endif
```

Si se usara el binario de RP2040 en un RP2350 (o al revés) escribiría en direcciones que pertenecen a **otros periféricos**: por eso cada tarjeta necesita su propio `.uf2`.

### Con SDK

`pwm_sdk.c` es **idéntico** para los dos chips. El SDK selecciona en compilación los headers `src/rp2040/...` o `src/rp2350/...` según `PICO_BOARD`. Lo único que cambiaría es el manejo de interrupciones (no usado aquí).

### En la compilación

```bash
cmake -S firmware -B build_pico    -DPICO_BOARD=pico      # RP2040
cmake -S firmware -B build_pico2_w -DPICO_BOARD=pico2_w   # RP2350
```

## 5.5 Similitudes (lo que se conserva)

- Misma arquitectura del slice: divisor 8.4 → contador 16 bits → 2 comparadores → polaridad.
- Mismos registros por slice, mismos offsets internos y mismos campos de bits.
- Mismo mapeo de GPIO 0–29 a slices 0–7 → **el cableado es idéntico** en ambas tarjetas (mismo pinout físico).
- Mismo mecanismo de alias atómicos (`+0x1000/0x2000/0x3000`).
- Misma API del SDK para configurar y cambiar duty.

## 5.6 Cómo demostrarlo en vivo

1. Cargar `Motor_PWM_RP2040.uf2` en la Pico → `pwm info` → `motor 50 derecha` → `pwm regs`.
2. Cargar `Motor_PWM_RP2350.uf2` en la Pico 2 W → mismos comandos.
3. Comparar: **direcciones** diferentes (`0x40050028` vs `0x400A8028`), **`DIV`** diferente (`0x7D0` vs `0x960`), **`CC.A` y `TOP` iguales** (7000 / 9999), **misma frecuencia** de salida (100 Hz).
