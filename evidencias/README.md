# Evidencias

## Del laboratorio (Sesión 06, MicroPython en Pico 2 W)

Estas evidencias son del mismo montaje físico (Pico 2 W + L298N + motor DC) en la Sesión 06, antes de portar el control a C:

| Archivo | Contenido |
|---|---|
| [`hardware.jpg`](hardware.jpg) | Montaje físico Pico 2 W + L298N + motor |
| [`hardware_video.mp4`](hardware_video.mp4) | Motor funcionando con rampas y cambio de dirección |
| [`simulation.png`](simulation.png) | Simulación en Wokwi |
| [`serial.png`](serial.png) | Salida serial de la simulación |

## Del proyecto en C (por capturar)

Guardar aquí con estos nombres para que el README los enlace:

| Evidencia requerida | Archivo sugerido | Cómo obtenerla | ✔ |
|---|---|---|---|
| Programa compilado | `compilacion.png` | Captura de `./build.sh` terminando con los dos `.uf2` | ☐ |
| REPL funcionando | `repl_help.png` | `help` en la terminal serie | ☐ |
| Configuración del periférico | `pwm_regs_rp2350.png`, `pwm_regs_rp2040.png` | `motor 50 derecha` + `pwm regs` en cada tarjeta | ☐ |
| Equivalencia regs vs SDK | `regs_vs_sdk.png` | `pwm regs` con `pwm modo regs` y con `pwm modo sdk` | ☐ |
| Validación de argumentos | `validacion.png` | `motor 150 derecha`, `motor 50 arriba`, `pwm freq 10` | ☐ |
| Dispositivo externo conectado | `montaje_rp2040.jpg`, `montaje_rp2350.jpg` | Foto con el cableado visible | ☐ |
| Demostración física | `demo_rp2350.mp4`, `demo_rp2040.mp4` | Video: comando en la terminal + motor girando | ☐ |
| Funcionamiento en RP2040 | `pwm_info_rp2040.png` | `pwm info` | ☐ |
| Funcionamiento en RP2350 | `pwm_info_rp2350.png` | `pwm info` | ☐ |
| (Opcional) Osciloscopio | `osciloscopio_100hz.png` | GP4 a 100 Hz, duty 70 % | ☐ |
