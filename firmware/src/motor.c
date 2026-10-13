/* =========================================================================
 *  motor.c - Logica del controlador (portada de main.py de la Sesion 06)
 *
 *  IN1/IN2 deciden hacia donde gira; ENA (PWM) decide que tan rapido.
 *  Toda la velocidad cambia por rampas y el sentido solo se invierte con
 *  el motor en 0 %. No sabe si el PWM se maneja con punteros o con SDK:
 *  todo pasa por el backend activo.
 * ========================================================================= */
#include <stdio.h>

#include "pico/stdlib.h"
#include "hardware/clocks.h"

#include "motor.h"

static const pwm_backend_t *be;
static pwm_cfg_t cfg;
static uint32_t velocidad_actual = 0;          /* % logico 0-100 */
static motor_dir_t direccion_actual = DETENIDO;

/* ---------------- Calculo del divisor ---------------- */
/* f = clk_sys / (DIV * (TOP+1) * (fase ? 2 : 1)), DIV en punto fijo 8.4:
 * div16 = DIV * 16 debe quedar entre 16 (1.0) y 4095 (255.9375). */
bool pwm_cfg_calcular(pwm_cfg_t *c, uint32_t clk_hz, uint32_t freq_hz,
                      uint16_t top, bool fase, bool inv) {
    if (freq_hz == 0u) return false;

    uint64_t den   = (uint64_t)freq_hz * ((uint32_t)top + 1u) * (fase ? 2u : 1u);
    uint64_t div16 = ((uint64_t)clk_hz * 16u + den / 2u) / den;   /* redondeo */

    if (div16 < 16u || div16 > 0xFFFu) return false;

    c->freq_hz  = freq_hz;
    c->top      = top;
    c->div_int  = (uint8_t)(div16 >> 4);
    c->div_frac = (uint8_t)(div16 & 0xFu);
    c->fase     = fase;
    c->inv      = inv;
    return true;
}

/* ---------------- Zona muerta ---------------- */
/* 0 % = apagado. 1-100 % logico se reparte entre MIN_DUTY y 100 % real.
 * CC = TOP + 1 deja la salida siempre en alto (100 %). */
uint16_t motor_cc_para(uint32_t pct) {
    if (pct == 0u) return 0u;
    uint32_t real_x100 = MIN_DUTY * 100u + (100u - MIN_DUTY) * pct;   /* % * 100 */
    return (uint16_t)((real_x100 * ((uint32_t)cfg.top + 1u)) / 10000u);
}

static void set_speed(uint32_t pct) {
    if (pct > 100u) pct = 100u;

    /* Si venimos de 0 y vamos a movernos: empujon para vencer la inercia */
    if (velocidad_actual == 0u && pct > 0u) {
        be->nivel((uint16_t)(cfg.top + 1u));
        sleep_ms(KICK_MS);
    }
    be->nivel(motor_cc_para(pct));
    velocidad_actual = pct;
}

static void ramp_to(uint32_t destino) {
    uint32_t v = velocidad_actual;
    if (v == destino) return;

    printf("%s: %lu", destino > v ? "Acelerando" : "Desacelerando", (unsigned long)v);
    while (v != destino) {
        if (destino > v) v = (destino - v > PASO_RAMPA) ? v + PASO_RAMPA : destino;
        else             v = (v - destino > PASO_RAMPA) ? v - PASO_RAMPA : destino;
        set_speed(v);
        printf(" %lu", (unsigned long)v);
        sleep_ms(DELAY_RAMPA_MS);
    }
    printf(" %%  (CC.A = %u de TOP+1 = %lu)\n", motor_cc_para(v),
           (unsigned long)cfg.top + 1u);
}

static void aplicar_direccion(motor_dir_t d) {
    switch (d) {
    case DERECHA:   be->direccion(true,  false); break;
    case IZQUIERDA: be->direccion(false, true);  break;
    default:        be->direccion(false, false); break;   /* nunca IN1=IN2=1 */
    }
    direccion_actual = d;
}

/* Nunca se invierte en movimiento: rampa a 0 %, pausa, y luego IN1/IN2 */
static void cambiar_direccion(motor_dir_t nueva) {
    if (nueva == direccion_actual) return;

    if (velocidad_actual > 0u) {
        printf("[SEGURIDAD] Bajando a 0 %% antes de cambiar de direccion\n");
        ramp_to(0u);
    }
    aplicar_direccion(DETENIDO);
    sleep_ms(PAUSA_INVERSION_MS);

    aplicar_direccion(nueva);
    printf("[DIRECCION] %s  IN1=%d IN2=%d\n", motor_dir_nombre(nueva),
           nueva == DERECHA, nueva == IZQUIERDA);
}

/* ---------------- API ---------------- */
void motor_init(const pwm_backend_t *backend) {
    be = backend;
    pwm_cfg_calcular(&cfg, clock_get_hz(clk_sys), PWM_FREQ_DEFAULT,
                     PWM_TOP_DEFAULT, false, false);
    be->init();
    be->aplicar(&cfg);
    be->nivel(0u);
    aplicar_direccion(DETENIDO);
    velocidad_actual = 0u;
}

void motor_set_backend(const pwm_backend_t *backend) {
    if (velocidad_actual > 0u) motor_stop();
    be = backend;
    be->init();
    be->aplicar(&cfg);
    be->nivel(0u);
    aplicar_direccion(DETENIDO);
}

const pwm_backend_t *motor_backend(void) { return be; }

void motor_mover(uint32_t pct, motor_dir_t dir) {
    if (pct == 0u || dir == DETENIDO) {
        motor_stop();
        return;
    }
    cambiar_direccion(dir);
    ramp_to(pct > 100u ? 100u : pct);
}

void motor_stop(void) {
    ramp_to(0u);
    be->nivel(0u);
    aplicar_direccion(DETENIDO);
    velocidad_actual = 0u;
    printf("[STOP] Motor detenido  IN1=0 IN2=0 CC.A=0\n");
}

void motor_mantener(uint32_t ms) {
    printf("[MANTENER] %lu %% durante %lu ms\n",
           (unsigned long)velocidad_actual, (unsigned long)ms);
    sleep_ms(ms);
}

/* Cambia una opcion: recalcula DIV, reescribe el slice y repone el nivel */
static bool reconfigurar(uint32_t freq, uint32_t top, bool fase, bool inv) {
    pwm_cfg_t nueva;
    if (top < 99u || top > 65534u) return false;
    if (!pwm_cfg_calcular(&nueva, clock_get_hz(clk_sys), freq, (uint16_t)top, fase, inv))
        return false;
    cfg = nueva;
    be->aplicar(&cfg);
    be->nivel(motor_cc_para(velocidad_actual));
    return true;
}

bool motor_set_freq(uint32_t f)  { return reconfigurar(f, cfg.top, cfg.fase, cfg.inv); }
bool motor_set_top(uint32_t top) { return reconfigurar(cfg.freq_hz, top, cfg.fase, cfg.inv); }
bool motor_set_fase(bool fase)   { return reconfigurar(cfg.freq_hz, cfg.top, fase, cfg.inv); }
bool motor_set_inv(bool inv)     { return reconfigurar(cfg.freq_hz, cfg.top, cfg.fase, inv); }

uint32_t motor_velocidad(void)      { return velocidad_actual; }
motor_dir_t motor_direccion(void)   { return direccion_actual; }
const pwm_cfg_t *motor_cfg(void)    { return &cfg; }

const char *motor_dir_nombre(motor_dir_t d) {
    switch (d) {
    case DERECHA:   return "DERECHA (FORWARD)";
    case IZQUIERDA: return "IZQUIERDA (REVERSE)";
    default:        return "DETENIDO";
    }
}
