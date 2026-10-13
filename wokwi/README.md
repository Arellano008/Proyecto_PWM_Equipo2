# Antecedente: Sesión 06 (MicroPython + Wokwi)

- Proyecto Wokwi: https://wokwi.com/projects/476181809960581121
- `diagram.json`: circuito de la plantilla del curso (GP2=IN1, GP3=IN2, GP4=ENA).
- `main_sesion06.py`: controlador original en MicroPython. De aquí salieron los ajustes que usa el firmware en C: 100 Hz, zona muerta de 40 %, empujón de 120 ms, rampas de 5 % cada 100 ms y cambio de dirección pasando por 0 %.

En MicroPython, `PWM(Pin(4)).freq(100)` y `duty_u16()` esconden todo el trabajo con registros; el proyecto en C lo hace explícito (`firmware/src/pwm_regs.c`).
