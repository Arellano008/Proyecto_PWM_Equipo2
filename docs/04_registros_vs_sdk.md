# 4. Implementación: punteros vs Pico SDK

El firmware tiene **dos implementaciones de la misma función** detrás de una interfaz común ([`pwm_backend.h`](../firmware/src/pwm_backend.h)):

| Backend | Archivo | Se activa con |
|---|---|---|
| Registros con punteros | [`pwm_regs.c`](../firmware/src/pwm_regs.c) | `pwm modo regs` (por defecto al arrancar) |
| Pico SDK | [`pwm_sdk.c`](../firmware/src/pwm_sdk.c) | `pwm modo sdk` |

La lógica del motor ([`motor.c`](../firmware/src/motor.c)) no sabe cuál está activo. Por eso podemos ejecutar **el mismo comando** con las dos y comprobar con `pwm regs` que el hardware queda **idéntico**.

## 4.1 Acceso directo a registros

### Conceptos

| Concepto | En el código | Explicación |
|---|---|---|
| **Dirección** | `0x40050034` | Cada registro del periférico es una posición de memoria (memory-mapped I/O) |
| **Puntero** | `volatile uint32_t *pwm_cc` | Variable que guarda esa dirección |
| **Desreferenciar** | `*pwm_cc = 7000;` | Leer/escribir *en* la dirección, no la variable |
| **`volatile`** | `volatile uint32_t *` | El valor lo cambia el hardware (p. ej. `CTR`), y escribir tiene efectos. Sin `volatile` el compilador podría eliminar lecturas repetidas, juntar escrituras o reordenarlas: el `while` de `RESET_DONE` sería un ciclo infinito y escribir `CSR = 0` y luego `CSR = EN` podría quedar en una sola escritura |
| **Tamaño** | `uint32_t` | Los registros son de 32 bits y deben accederse de 32 bits |
| **Máscara** | `0xFFFF0000u`, `1u << 14` | Selecciona bits: `&` para leer/conservar, `|` para encender, `& ~` para apagar |
| **Campo de bits** | `(div_int << 4) | div_frac` | Varios valores empaquetados en un registro; se desplazan a su posición (`LSB`) |

### Código clave

```c
/* plataforma.h */
#define PWM_BASE_ADDR   0x40050000u            /* RP2040 (0x400A8000u en RP2350) */
#define PWM_CH_STRIDE   0x014u
#define PWM_CC_OFS      0x00Cu
#define PWM_SLICE_REG(slice, ofs) (PWM_BASE_ADDR + PWM_CH_STRIDE * (slice) + (ofs))

/* pwm_regs.c */
static volatile uint32_t *const pwm_cc =
    (volatile uint32_t *)PWM_SLICE_REG(PWM_SLICE_ENA, PWM_CC_OFS);

static void regs_aplicar(const pwm_cfg_t *c) {
    uint32_t csr = CSR_EN;
    if (c->fase) csr |= CSR_PH_CORRECT;
    if (c->inv)  csr |= CSR_A_INV;

    *pwm_csr = 0u;                                          /* apagar           */
    *pwm_div = ((uint32_t)c->div_int << DIV_INT_LSB) | c->div_frac;
    *pwm_top = c->top;
    *pwm_ctr = 0u;
    *pwm_csr = csr;                                         /* EN = 1 arranca   */
}

static void regs_nivel(uint16_t cc_a) {
    *pwm_cc = (*pwm_cc & 0xFFFF0000u) | cc_a;               /* conserva CC.B    */
}
```

Configurar el pin a mano (lo que hace `gpio_set_function`):

```c
static void gpio_funcsel(uint32_t pin, uint32_t funcsel) {
    volatile uint32_t *pad  = (volatile uint32_t *)(PADS_BANK0_BASE_ADDR + PADS_GPIO_OFS(pin));
    volatile uint32_t *ctrl = (volatile uint32_t *)(IO_BANK0_BASE_ADDR + IO_GPIO_CTRL_OFS(pin));

    *pad  = (*pad & ~PAD_OD_BIT) | PAD_IE_BIT;
    *ctrl = funcsel;                                        /* 4 = PWM, 5 = SIO */
#if PADS_TIENE_ISO                                          /* solo RP2350      */
    REG32(PADS_BANK0_BASE_ADDR + ALIAS_CLR + PADS_GPIO_OFS(pin)) = PAD_ISO_BIT;
#endif
}
```

## 4.2 Pico SDK

```c
static void sdk_init(void) {
    gpio_init(PIN_IN1);  gpio_init(PIN_IN2);
    gpio_put(PIN_IN1, 0); gpio_put(PIN_IN2, 0);
    gpio_set_dir(PIN_IN1, GPIO_OUT); gpio_set_dir(PIN_IN2, GPIO_OUT);
    pwm_set_chan_level(pwm_gpio_to_slice_num(PIN_ENA), PWM_CHAN_A, 0);
    gpio_set_function(PIN_ENA, GPIO_FUNC_PWM);
}

static void sdk_aplicar(const pwm_cfg_t *c) {
    uint slice = pwm_gpio_to_slice_num(PIN_ENA);
    pwm_config pc = pwm_get_default_config();
    pwm_config_set_clkdiv_int_frac4(&pc, c->div_int, c->div_frac);
    pwm_config_set_wrap(&pc, c->top);
    pwm_config_set_phase_correct(&pc, c->fase);
    pwm_config_set_output_polarity(&pc, c->inv, false);
    pwm_config_set_clkdiv_mode(&pc, PWM_DIV_FREE_RUNNING);
    pwm_init(slice, &pc, true);
}

static void sdk_nivel(uint16_t cc_a) { pwm_set_gpio_level(PIN_ENA, cc_a); }
```

## 4.3 Qué abstrae el SDK (relación función ↔ registro)

| Función SDK | Registro(s) reales | Qué te ahorra |
|---|---|---|
| `gpio_set_function(4, GPIO_FUNC_PWM)` | `PADS_BANK0.GPIO4` (IE, OD, **ISO**), `IO_BANK0.GPIO4_CTRL.FUNCSEL` | Saber offsets de pads/IO y que el RP2350 necesita limpiar `ISO` |
| `pwm_gpio_to_slice_num(4)` | — (cálculo `(gpio >> 1) & 7`) | Recordar el mapeo pin → slice |
| `pwm_config_set_clkdiv_int_frac4()` | `CHn_DIV.INT`, `CHn_DIV.FRAC` | Empaquetar el formato 8.4 |
| `pwm_config_set_wrap()` | `CHn_TOP` | — |
| `pwm_config_set_phase_correct()` | `CHn_CSR.PH_CORRECT` | Máscara del bit |
| `pwm_config_set_output_polarity()` | `CHn_CSR.A_INV`, `B_INV` | Máscaras |
| `pwm_config_set_clkdiv_mode()` | `CHn_CSR.DIVMODE` | Campo de 2 bits |
| `pwm_init(slice, &cfg, true)` | `CSR=0`, `CTR=0`, `CC=0`, `TOP`, `DIV`, `CSR` (+EN) | Orden correcto de escritura |
| `pwm_set_gpio_level()` / `pwm_set_chan_level()` | `CHn_CC` (mitad A o B) | Escribir solo la mitad A o B con `hw_write_masked` (alias XOR atómico) sin tocar el otro canal |
| `gpio_put()` | `SIO.GPIO_OUT_SET` / `GPIO_OUT_CLR` | Offsets SIO distintos entre chips |
| `pwm_set_enabled()` / `pwm_set_mask_enabled()` | `CSR.EN` / `EN` | Arranque simultáneo de slices |
| `clock_get_hz(clk_sys)` | `CLOCKS` / frecuencia configurada | Saber el reloj real |

### Ventajas del SDK

- **Portabilidad**: el mismo código compila en RP2040 y RP2350; el SDK conoce bases, offsets, bits de reset y el `ISO` de los pads.
- **Legibilidad y menos errores**: nombres en vez de números mágicos.
- **Validaciones** en modo debug (`valid_params_if`).
- **Mantenimiento**: si cambia el hardware, se actualiza el SDK, no la aplicación.

### Limitaciones del SDK

- **Oculta lo que pasa**: es fácil no saber que `pwm_init` pone `CC = 0` (por eso nuestro código repone el nivel después de cambiar la frecuencia).
- **Puede hacer escrituras de más** (p. ej. `pwm_init` reescribe todo el slice aunque solo cambie un campo).
- **Dependencia** de la versión del SDK y su sistema de compilación.
- No expone todo: para depurar hay que leer los registros de todos modos (nuestro `pwm regs` lee siempre con punteros).

### Comparación de líneas para la misma tarea

| Tarea | Punteros | SDK |
|---|---|---|
| Conectar GP4 al PWM | ~5 líneas + conocer 3 bases/offsets | 1 línea |
| Configurar frecuencia y modo | 5 escrituras, máscaras explícitas | 6 llamadas descriptivas |
| Cambiar duty | 1 línea con máscara | 1 línea |
| Portar a otro chip | cambiar `plataforma.h` | nada |

## 4.4 Prueba de equivalencia (para la demo)

```text
RP2350> pwm modo sdk
RP2350> motor 50 derecha
RP2350> pwm regs          ← anotar CSR, DIV, TOP, CC
RP2350> motor stop
RP2350> pwm modo regs
RP2350> motor 50 derecha
RP2350> pwm regs          ← mismos valores: el SDK escribe los mismos registros
```
