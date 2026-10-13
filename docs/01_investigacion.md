# 1. Investigación y contexto: el periférico PWM

## 1.1 ¿Qué es el PWM?

**PWM (Pulse Width Modulation, modulación por ancho de pulso)** es un periférico que genera una señal digital cuadrada de **frecuencia fija** en la que se puede controlar el **tiempo en alto** de cada periodo.

- **Periodo (T)**: duración de un ciclo completo. `f = 1 / T`.
- **Duty cycle (D)**: fracción del periodo en alto. `D = t_alto / T`.
- **Valor promedio** de la señal: `V_prom = D · V_alto`.

Un pin digital solo puede estar en 0 V o 3.3 V. El PWM no saca "la mitad" del voltaje, pero si la carga es lenta comparada con la frecuencia (un motor, la inercia térmica de un LED vista por el ojo, un filtro LC) la carga **solo percibe el promedio**. Así una salida digital controla una magnitud "analógica": velocidad, brillo, voltaje, posición.

### Problema tecnológico que resuelve

Controlar potencia **sin desperdiciarla**. La alternativa clásica es una resistencia en serie (reóstato) o un transistor en zona lineal: el voltaje que "sobra" se convierte en calor. En PWM el transistor está **totalmente encendido** (caída de voltaje ≈ 0 → potencia ≈ 0) o **totalmente apagado** (corriente = 0 → potencia = 0). La disipación solo ocurre en las transiciones, por eso los convertidores PWM superan el 90 % de eficiencia.

Además, como periférico de hardware, **libera al CPU**: una vez configurado, el contador genera la señal solo. El programa únicamente cambia un número (el nivel de comparación) cuando quiere otra velocidad.

### Tipo de información que maneja

| Magnitud | Dónde vive en el RP2040/RP2350 |
|---|---|
| Frecuencia | `clk_sys`, divisor `DIV` (8.4 bits) y `TOP` (16 bits) |
| Duty cycle | Comparador `CC` (16 bits por canal) frente al contador `CTR` |
| Polaridad | Bits `A_INV` / `B_INV` |
| Modo de cuenta | `PH_CORRECT` (diente de sierra o triángulo) |
| Entrada (modo medición) | `DIVMODE`: el canal B puede contar flancos o medir duty |

Es información **digital y temporal**: números de 16 bits que se convierten en tiempos.

### Relación con CPU, buses y GPIO

```
CPU (Cortex-M0+ / Cortex-M33 / Hazard3)
   │  load/store de 32 bits
   ▼
Bus AHB-Lite → puente APB ──► bloque PWM (registros CHn_CSR, DIV, CTR, CC, TOP, EN, INTR…)
                                     │ salidas A/B de cada slice
                                     ▼
                              IO_BANK0 (FUNCSEL = 4 "PWM")  ──► PADS_BANK0 ──► pin GPIO
                                     │
                              IRQ PWM_WRAP → NVIC     DREQ → DMA
```

1. El CPU escribe los registros del PWM a través del bus (son direcciones de memoria: *memory-mapped I/O*).
2. El PWM genera las señales internamente con su reloj (`clk_sys`).
3. **IO_BANK0** decide qué periférico maneja cada pin (`FUNCSEL`). Con `FUNCSEL = 4` el pin queda conectado al PWM.
4. **PADS_BANK0** configura la parte eléctrica del pin (habilitar salida, fuerza de corriente, y en RP2350 el aislamiento `ISO`).
5. Opcional: al dar la vuelta el contador (*wrap*) puede generar una interrupción o pedir una transferencia DMA.

## 1.2 Aplicaciones actuales

1. **Control de velocidad de motores DC** (este proyecto): puente H + PWM en el pin de habilitación. Robots, drones, ventiladores, bombas.
2. **Servomotores RC**: pulso de 1–2 ms cada 20 ms (50 Hz) codifica el ángulo.
3. **Atenuación (dimming) de LEDs y retroiluminación** de pantallas y focos LED.
4. **Fuentes conmutadas y convertidores DC-DC** (buck/boost): el duty fija el voltaje de salida. Cargadores de celulares y laptops.
5. **Amplificadores de audio clase D**: bocinas Bluetooth, barras de sonido, audio automotriz.
6. **Inversores y variadores de frecuencia (VFD)**: motores trifásicos industriales, autos eléctricos, inversores solares (PWM senoidal).
7. **Ventiladores de PC de 4 pines**: control por PWM a 25 kHz.
8. **DAC de bajo costo**: PWM + filtro RC para generar voltajes analógicos o tonos.

## 1.3 Antecedentes históricos y evolución tecnológica

### La necesidad que lo originó

Desde finales del siglo XIX se necesitaba **variar la velocidad de motores eléctricos** (tranvías, elevadores, máquinas-herramienta) y **regular voltaje**. Todas las soluciones iniciales "tiraban" la energía sobrante.

### Cómo se hacía antes de integrarse en microcontroladores

| Época | Técnica | Limitación |
|---|---|---|
| Finales s. XIX | **Reóstatos** en serie con el motor | La energía sobrante se disipa como calor |
| 1891 | **Sistema Ward Leonard**: un motor mueve un generador cuyo voltaje alimenta al motor de trabajo | Tres máquinas rotativas: caro, pesado, mantenimiento |
| 1920–1950 | **Tubos de vacío, tiratrones y rectificadores de arco de mercurio** con control de fase | Frágiles, calientes, voluminosos |
| 1957 → 60s | **SCR / tiristor** (General Electric): control de ángulo de fase y *choppers* para tracción DC | Difícil de apagar en DC; frecuencias bajas |
| 1960s | **Amplificadores clase D** experimentales y comerciales | Transistores bipolares lentos, mucho ruido |
| 1970s | **Timer 555** (1972) y los primeros **controladores PWM integrados** (SG3524, TL494) para fuentes conmutadas | PWM analógico: rampa + comparador, sin programación |
| Finales 70s–80s | **MOSFET de potencia** e **IGBT**: interruptores rápidos y eficientes | — habilitan PWM de decenas de kHz con alta potencia |

### Evolución dentro de los microcontroladores

1. **Primeros MCU (finales 70s, inicio 80s)**: el PWM se generaba **por software** con un timer y una interrupción que encendía/apagaba un pin. Consumía CPU y tenía *jitter*.
2. **Timers con "output compare"** (familias de 8/16 bits de los 80s): un comparador de hardware cambia el pin al coincidir con el contador. Ya no hace falta la interrupción en cada flanco.
3. **Módulos PWM dedicados** (años 90, p. ej. los módulos CCP de los PIC y los timers de los AVR): varios canales, modos rápido y *phase-correct*, pocos bits de resolución (8–10).
4. **Timers avanzados de 32 bits** (Cortex-M, años 2000): tiempos muertos (*dead-time*) para puentes H, salidas complementarias, sincronía entre canales, DMA. Pensados para control de motores y conversión de energía.
5. **RP2040 (2021)** y **RP2350 (2024)**: PWM "de propósito general" muy flexible: **todos los GPIO** pueden ser salida PWM, divisor **fraccional 8.4**, contador de 16 bits, modo de **medición** en el canal B, avance/retraso de fase, inicio simultáneo de slices (`EN`), IRQ y DMA. El RP2350 amplía a **12 slices** y **dos líneas de interrupción**.

### Principales mejoras a lo largo del tiempo

- **Eficiencia**: de reóstatos (< 50 %) a convertidores PWM (> 90 %).
- **Velocidad**: de cientos de Hz (tiristores) a cientos de kHz o MHz (MOSFET/GaN).
- **Integración**: de máquinas rotativas a un bloque dentro del chip que ocupa una fracción de mm².
- **Precisión**: resolución de 8 bits → 16 bits; divisor fraccional para frecuencias exactas.
- **Flexibilidad**: cualquier GPIO, polaridad, modo de cuenta, medición de señales, sincronía, DMA.
- **Consumo de CPU**: de PWM por software (100 % del tiempo) a PWM por hardware (el CPU solo escribe un registro).

### Situación actual en sistemas embebidos

El PWM es el periférico estándar para **actuar sobre el mundo físico**: prácticamente todo microcontrolador moderno lo incluye. En control de motores, iluminación y conversión de energía es la técnica dominante; los MCU actuales (como el RP2350) lo combinan con DMA, PIO y doble núcleo para controlar varios actuadores al mismo tiempo sin carga para el CPU.

## 1.4 Referencias

Ver [README → Referencias](../README.md#16-referencias).
