# 3. Mapa de registros utilizado

Todas las direcciones de esta tabla están escritas a mano en [`firmware/src/plataforma.h`](../firmware/src/plataforma.h) y verificadas en compilación contra los headers oficiales del SDK por [`verifica_mapa.c`](../firmware/src/verifica_mapa.c) (si una estuviera mal, el proyecto no compila).

## 3.1 Direcciones base

| Bloque | RP2040 | RP2350 | Para qué lo usamos |
|---|---|---|---|
| `RESETS` | `0x4000C000` | `0x40020000` | Sacar el PWM de reset |
| `IO_BANK0` | `0x40014000` | `0x40028000` | `FUNCSEL` de GP2, GP3, GP4 |
| `PADS_BANK0` | `0x4001C000` | `0x40038000` | Habilitar salida del pad (y quitar `ISO` en RP2350) |
| **`PWM`** | **`0x40050000`** | **`0x400A8000`** | Generar la señal ENA |
| `SIO` | `0xD0000000` | `0xD0000000` | IN1 / IN2 (dirección) |

**Alias atómicos** (iguales en ambos chips, para bloques en APB/AHB):
`base + 0x1000` = XOR, `base + 0x2000` = SET (OR), `base + 0x3000` = CLR (AND NOT).
Escribir una máscara en el alias modifica solo esos bits, sin leer-modificar-escribir.

## 3.2 Registros PWM del slice 2 (GPIO4 = canal A)

Cada slice ocupa `0x14` bytes. Offset del slice 2 = `0x14 × 2 = 0x28`.

| Registro | Offset | Dirección RP2040 | Dirección RP2350 | Acceso | Valor que usamos (100 Hz) |
|---|---|---|---|---|---|
| `CH2_CSR` | `0x28` | `0x40050028` | `0x400A8028` | RW | `0x00000001` (EN=1) |
| `CH2_DIV` | `0x2C` | `0x4005002C` | `0x400A802C` | RW | RP2040 `0x7D0` (125.0) · RP2350 `0x960` (150.0) |
| `CH2_CTR` | `0x30` | `0x40050030` | `0x400A8030` | RW | se escribe 0 al configurar; luego lo cuenta el HW |
| `CH2_CC` | `0x34` | `0x40050034` | `0x400A8034` | RW | `CC.A` = 0 … 10000 según la velocidad |
| `CH2_TOP` | `0x38` | `0x40050038` | `0x400A8038` | RW | `9999` (`0x270F`) |
| `EN` | `0xA0` / **`0xF0`** | `0x400500A0` | `0x400A80F0` | RW | bit 2 = 1 (espejo de `CSR.EN`) |
| `INTR` | `0xA4` / **`0xF4`** | `0x400500A4` | `0x400A80F4` | WC | no usado |
| `INTE` | `0xA8` / **`0xF8` (IRQ0_INTE)** | `0x400500A8` | `0x400A80F8` | RW | no usado |

### `CHn_CSR` — control y estado

| Bits | Campo | Acceso | Significado | Valor |
|---|---|---|---|---|
| 0 | `EN` | RW | 1 = el slice cuenta | **1** |
| 1 | `PH_CORRECT` | RW | 1 = cuenta arriba/abajo (triángulo, f/2) | 0 (`pwm fase on` → 1) |
| 2 | `A_INV` | RW | invierte la salida A | 0 (`pwm inv on` → 1) |
| 3 | `B_INV` | RW | invierte la salida B | 0 |
| 5:4 | `DIVMODE` | RW | 0 = libre · 1 = cuenta mientras B alto · 2 = flanco subida de B · 3 = flanco bajada de B | **0** |
| 6 | `PH_RET` | SC | retrasa el contador 1 cuenta (se limpia solo) | 0 |
| 7 | `PH_ADV` | SC | adelanta el contador 1 cuenta (se limpia solo) | 0 |

### `CHn_DIV` — divisor fraccional 8.4

| Bits | Campo | Significado |
|---|---|---|
| 11:4 | `INT` | parte entera (1–255; reset = 1) |
| 3:0 | `FRAC` | parte fraccionaria en dieciseisavos |

`DIV = INT + FRAC/16`. En C: `*pwm_div = (div_int << 4) | div_frac;`

Ejemplo RP2040 a 1 kHz: `DIV = 12.5` → `INT = 12 (0xC)`, `FRAC = 8` → `0x000000C8`.

### `CHn_CTR`, `CHn_TOP`

| Registro | Bits | Significado |
|---|---|---|
| `CTR` | 15:0 | valor actual del contador (se puede leer en vivo con `pwm regs`) |
| `TOP` | 15:0 | valor de wrap; periodo = `TOP + 1` cuentas |

### `CHn_CC` — comparadores

| Bits | Campo | Significado |
|---|---|---|
| 15:0 | `A` | salida A en alto mientras `CTR < A` |
| 31:16 | `B` | salida B en alto mientras `CTR < B` |

En C se escribe solo el canal A conservando B:
`*pwm_cc = (*pwm_cc & 0xFFFF0000u) | cc_a;`

**Tabla de valores de la aplicación** (zona muerta 40 %, TOP = 9999):

| Comando | % lógico | Duty real | `CC.A` |
|---|---|---|---|
| `motor stop` | 0 | 0 % | 0 |
| `motor 25 derecha` | 25 | 55 % | 5500 |
| `motor 50 derecha` | 50 | 70 % | 7000 |
| `motor 75 derecha` | 75 | 85 % | 8500 |
| `motor 100 derecha` | 100 | 100 % | 10000 (= TOP + 1) |
| (empujón de arranque) | — | 100 % por 120 ms | 10000 |

## 3.3 Registros de soporte

### RESETS

| Registro | Offset | Acceso | Campo usado |
|---|---|---|---|
| `RESET` | `0x00` | RW | bit `PWM`: **14 en RP2040**, **16 en RP2350**. 1 = en reset |
| `RESET_DONE` | `0x08` | RO | mismo bit: 1 = ya salió de reset |

```c
REG32(RESETS_BASE_ADDR + ALIAS_CLR + RESETS_RESET_OFS) = 1u << RESETS_PWM_BIT;  // quitar reset
while (!(REG32(RESETS_BASE_ADDR + RESETS_RESET_DONE_OFS) & (1u << RESETS_PWM_BIT))) ;
```

### IO_BANK0 — `GPIOx_CTRL` (offset `8·x + 4`)

| Pin | Offset | Dir. RP2040 | Dir. RP2350 | `FUNCSEL` [4:0] |
|---|---|---|---|---|
| GP2 (IN1) | `0x14` | `0x40014014` | `0x40028014` | 5 = SIO |
| GP3 (IN2) | `0x1C` | `0x4001401C` | `0x4002801C` | 5 = SIO |
| GP4 (ENA) | `0x24` | `0x40014024` | `0x40028024` | **4 = PWM** |

### PADS_BANK0 — `GPIOx` (offset `4 + 4·x`)

| Pin | Offset | Bits relevantes |
|---|---|---|
| GP2 | `0x0C` | |
| GP3 | `0x10` | |
| GP4 | `0x14` | `OD` (7) = 0 salida habilitada · `IE` (6) = 1 · **`ISO` (8) = 0 solo RP2350** |

En el RP2350 los pads salen de reset con `ISO = 1` (aislados). Si no se limpia, el PWM funciona internamente pero **el pin no cambia**. El RP2040 no tiene ese bit.

### SIO — dirección del motor (IN1 / IN2)

| Registro | RP2040 | RP2350 | Uso |
|---|---|---|---|
| `GPIO_OUT_SET` | `0xD0000014` | `0xD0000018` | IN = 1 |
| `GPIO_OUT_CLR` | `0xD0000018` | `0xD0000020` | IN = 0 |
| `GPIO_OE_SET` | `0xD0000024` | `0xD0000038` | GP2/GP3 como salidas |

La base es la misma, pero **los offsets cambian** porque el RP2350 agrega registros para GPIO 32–47 (`GPIO_HI_*`) intercalados.

## 3.4 Secuencia completa: dirección → periférico

```
PWM_BASE          0x40050000  (RP2040)
+ offset slice 2  0x28
+ offset CC       0x0C
= dirección       0x40050034
→ puntero         volatile uint32_t *pwm_cc = (volatile uint32_t *)0x40050034;
→ valor           *pwm_cc = (*pwm_cc & 0xFFFF0000u) | 7000;
→ periférico      CTR < 7000 durante 7000 de 10000 cuentas → ENA en alto 70 % del tiempo
→ L298N           el puente H conduce 70 % del periodo
→ motor           gira a ~70 % de su velocidad
```
