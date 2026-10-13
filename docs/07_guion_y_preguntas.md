# 7. Guion de presentación (≥ 40 min) y banco de preguntas

> Repartir las partes entre los integrantes, pero **todos** deben poder contestar el banco de preguntas (criterio 10: dominio individual). Ensayar con cronómetro.

## 7.1 Guion por bloques

| # | Parte | Tiempo | Diapositivas | Qué decir / mostrar |
|---|---|---|---|---|
| 1 | Introducción y problema tecnológico | 5 min | Título, integrantes, ¿qué es PWM?, problema | Pin digital solo da 0 o 3.3 V → ¿cómo controlar la velocidad de un motor? Reóstato desperdicia energía. PWM: interruptor todo/nada + promedio. Fórmulas `f`, `D`, `V_prom` |
| 2a | Antecedentes históricos y evolución | 5 min | Línea de tiempo | Reóstato → Ward Leonard (1891) → tiratrones → SCR (1957) → 555 / SG3524 (70s) → MOSFET/IGBT → PWM por software → output compare → módulos PWM → RP2040/RP2350 |
| 2b | Aplicaciones y arquitectura | 5 min | Aplicaciones, diagrama de slice, GPIO→slice | Motores, servos, LEDs, fuentes, clase D, VFD. Slice: DIV → CTR → CC → INV → FUNCSEL → pin. `slice = (gpio>>1)&7` |
| 3 | Registros y punteros | 7 min | Mapa de registros, CSR bit a bit, DIV 8.4, secuencia base→offset→puntero | Recorrer `0x40050000 + 0x28 + 0x0C = 0x40050034`. `volatile`, máscara `0xFFFF0000`, `(INT<<4)|FRAC`. Mostrar `regs_aplicar()` y `gpio_funcsel()` |
| 4 | Pico SDK | 5 min | Código SDK, tabla función↔registro, ventajas/limitaciones | `pwm_init` escribe CSR=0, CTR, CC=0, TOP, DIV, CSR. Demo de equivalencia con `pwm modo sdk` + `pwm regs` |
| 5 | Comparación RP2040 vs RP2350 | 5 min | Tabla de diferencias, código `#if` | Base PWM, slices 8/12, EN 0xA0/0xF0, RESETS bit 14/16, ISO en pads, SIO offsets, clk 125/150 → DIV 125/150, IRQ 1 vs 2 líneas |
| 6 | Comando REPL | 5 min | Tabla `commands[]`, argc/argv, validación, diagrama de cadena | `motor 50 derecha` → argc=3, argv[1]="50"… `strtol` vs `atoi`. Errores en vivo: `motor 150 derecha`, `motor 50 arriba` |
| 7 | Opciones de configuración | 3 min | Opciones: freq, TOP, fase, inv, DIVMODE | `pwm freq 1000` (motor no arranca a duty bajo → por qué 100 Hz), `pwm fase on`, `pwm inv on` (motor al revés del esperado) |
| 8 | Demostración física | 6–8 min | Diagrama de conexión, foto del montaje | Ver guion de demo abajo, en **las dos tarjetas** |
| 9 | Conclusiones y referencias | 2 min | Conclusiones, referencias | |
| | **Total** | **≈ 48–50 min** | | |

## 7.2 Guion de la demostración

Antes de empezar: fuente del motor conectada, GND común, jumper de ENA quitado, terminal serie abierta.

**Pico 2 W (RP2350)**

```text
help
pwm info                  ← direcciones del RP2350
motor 50 derecha          ← motor gira; consola: argv, rampa, CC.A = 7000
pwm regs                  ← CSR=1, DIV=0x960 (150.0), TOP=9999, CC.A=7000, f=100 Hz
motor 100 derecha         ← acelera
motor 30 izquierda        ← [SEGURIDAD] rampa a 0, invierte, acelera
motor estado
motor 150 derecha         ← error de validación
pwm freq 1000             ← motor más silencioso; DIV = 15.0
motor 10 derecha          ← a 1 kHz y duty bajo puede no arrancar → por eso 100 Hz
pwm freq 100
pwm modo sdk              ← mismo comando, ahora con el SDK
motor 50 derecha
pwm regs                  ← mismos valores que con punteros
demo                      ← secuencia CHALLENGE 06
motor stop
```

**Pico (RP2040)** – cambiar la tarjeta, cargar `Motor_PWM_RP2040.uf2`, mismo cableado:

```text
pwm info                  ← PWM_BASE 0x40050000, bit reset 14, sin ISO
motor 50 derecha
pwm regs                  ← DIV=0x7D0 (125.0), mismas TOP y CC → misma frecuencia
demo
```

## 7.3 Banco de preguntas individuales (con respuesta)

### Del documento de la actividad

| Pregunta | Respuesta |
|---|---|
| ¿Cuál es la dirección base? | PWM: `0x40050000` (RP2040), `0x400A8000` (RP2350). SIO `0xD0000000` en ambos |
| ¿Qué registro modificas? | `CH2_CC` (`base + 0x34`) para la velocidad; `CH2_DIV`/`CH2_TOP`/`CH2_CSR` para la configuración; `SIO GPIO_OUT_SET/CLR` para IN1/IN2 |
| ¿Qué significa este bit? (`CSR` bit 0) | `EN`: habilita el contador del slice. Bit 1 `PH_CORRECT`, bit 2 `A_INV`, bits 5:4 `DIVMODE` |
| ¿Por qué utilizarías `volatile`? | Porque el registro lo puede cambiar el hardware y cada acceso tiene efecto; sin `volatile` el compilador puede eliminar, juntar o reordenar lecturas/escrituras (p. ej. el `while` de `RESET_DONE` se volvería infinito) |
| ¿Qué hace esta función del SDK? (`pwm_init`) | Apaga el slice (`CSR=0`), reinicia `CTR` y `CC`, escribe `TOP` y `DIV`, y escribe `CSR` con `EN=1` si `start=true` |
| ¿Qué cambia entre RP2040 y RP2350? | Bases (PWM, RESETS, IO, PADS), slices 8→12, offsets de `EN/INTR/INTE`, bit de reset 14→16, bit `ISO` en pads, offsets SIO, reloj 125→150 MHz (DIV 125→150), 1→2 líneas de IRQ |
| ¿Qué contiene `argv[1]`? | En `motor 50 derecha`: la **cadena** `"50"` (un `char *`), no el número. Se convierte con `strtol` y se valida 0–100 |
| ¿Qué ocurre físicamente al ejecutar el comando? | IN1=1, IN2=0 → el L298N conecta OUT1 a +V y OUT2 a GND; GP4 genera 100 Hz con 70 % en alto → el puente conduce el 70 % del tiempo → el motor gira a la derecha a ~70 % de velocidad, subiendo por rampa |

### Adicionales probables

| Pregunta | Respuesta |
|---|---|
| ¿Cómo calculas la frecuencia? | `f = clk_sys / (DIV × (TOP+1))`. RP2040: 125 MHz / (125 × 10000) = 100 Hz |
| ¿Por qué DIV tiene parte fraccionaria? | Para frecuencias que no salen exactas con un entero. Ej. 1 kHz en RP2040: DIV = 12.5 → `INT=12, FRAC=8` |
| ¿Qué pasa si CC > TOP? | La salida queda siempre en alto (100 %). Usamos `CC = TOP+1 = 10000` para 100 % |
| ¿Por qué GPIO4 es el slice 2 canal A? | `slice = (4 >> 1) & 7 = 2`; 4 es par → canal A |
| ¿Por qué 100 Hz y no 1 kHz? | Medido en el lab: a 1 kHz los pulsos son tan cortos que la corriente de la bobina no sube y el motor no arranca con duty bajo; a 100 Hz sí (a cambio, zumba) |
| ¿Qué es la zona muerta? | Por debajo de ~40 % el motor no gira (caída de ~2 V del L298N + fricción). Repartimos 1–100 % lógico en 40–100 % real |
| ¿Para qué el empujón de 120 ms? | La fricción estática es mayor que la dinámica: se necesita más par para arrancar que para seguir girando |
| ¿Por qué no invertir el sentido en movimiento? | El motor girando actúa como generador; invertir de golpe produce un pico de corriente que calienta el L298N, mete ruido y puede reiniciar la Pico |
| ¿Qué hace IN1=IN2=1? | Freno dinámico (corto del motor a través del puente). No lo usamos |
| ¿Qué es el alias `+0x3000`? | Alias CLR atómico: escribir una máscara ahí pone en 0 esos bits sin leer-modificar-escribir |
| ¿Qué es `ISO` en el RP2350? | Aislamiento del pad: al salir de reset el pad no está conectado al periférico. Hay que limpiarlo (lo hace `gpio_set_function`) |
| ¿Qué hace `PH_CORRECT`? | El contador sube y baja (triángulo): la frecuencia se reduce a la mitad y los pulsos quedan centrados |
| ¿Qué es `DIVMODE`? | Origen del conteo: 0 libre; 1–3 usan el pin B como entrada (gate o flancos) para medir duty o frecuencia |
| ¿Para qué el registro `EN`? | Habilitar varios slices **al mismo tiempo** con una sola escritura (sincronía) |
| ¿Usaron interrupciones? | No (son opcionales). El PWM corre solo por hardware; el doble búfer de CC/TOP aplica los cambios al final del periodo |
| ¿Por qué no sirve el mismo .uf2 en las dos tarjetas? | Distinta arquitectura de CPU y distinto mapa de memoria: el binario del RP2040 escribiría en direcciones que en el RP2350 son otros periféricos |
| ¿Por qué `strtol` y no `atoi`? | `atoi` no detecta errores (`"5o"` → 5, `"abc"` → 0). Con `strtol` y `endptr` se rechaza cualquier carácter no numérico |
| ¿Cómo comprobaron que sus direcciones son correctas? | `verifica_mapa.c`: `_Static_assert` contra los headers oficiales; si una dirección estuviera mal, no compila |
