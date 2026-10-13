/* =========================================================================
 *  pwm_backend.h - Interfaz comun para las dos implementaciones del PWM
 *
 *  pwm_backend_regs : acceso directo a registros con punteros
 *  pwm_backend_sdk  : Raspberry Pi Pico SDK (hardware_pwm / hardware_gpio)
 *
 *  La logica del motor (motor.c) solo conoce esta interfaz, asi que el
 *  mismo comando del REPL puede ejecutarse con cualquiera de las dos.
 * ========================================================================= */
#ifndef PWM_BACKEND_H
#define PWM_BACKEND_H

#include <stdint.h>
#include <stdbool.h>

/* Pines del puente H L298N (iguales que en la Sesion 06) */
#define PIN_IN1   2u    /* GP2 - direccion            */
#define PIN_IN2   3u    /* GP3 - direccion            */
#define PIN_ENA   4u    /* GP4 - PWM slice 2, canal A */

#define PWM_SLICE_ENA   ((PIN_ENA >> 1u) & 7u)   /* GPIO4 -> slice 2   */

/* Configuracion del slice, ya traducida a valores de registro */
typedef struct {
    uint32_t freq_hz;    /* frecuencia pedida                         */
    uint16_t top;        /* TOP: el contador va de 0 a TOP            */
    uint8_t  div_int;    /* DIV.INT  [11:4]                           */
    uint8_t  div_frac;   /* DIV.FRAC [3:0] (dieciseisavos)            */
    bool     fase;       /* CSR.PH_CORRECT                            */
    bool     inv;        /* CSR.A_INV                                 */
} pwm_cfg_t;

typedef struct {
    const char *nombre;
    void (*init)(void);                        /* GPIO + PWM, motor quieto */
    void (*aplicar)(const pwm_cfg_t *cfg);     /* DIV, TOP, CSR            */
    void (*nivel)(uint16_t cc_a);              /* CC.A (0..TOP+1)          */
    void (*direccion)(bool in1, bool in2);     /* IN1 / IN2 por SIO        */
} pwm_backend_t;

extern const pwm_backend_t pwm_backend_regs;
extern const pwm_backend_t pwm_backend_sdk;

/* Calcula DIV para que f = clk / (DIV * (TOP+1) * (fase ? 2 : 1)).
 * Regresa false si el divisor no cabe en 1.0 .. 255.9375. */
bool pwm_cfg_calcular(pwm_cfg_t *cfg, uint32_t clk_hz, uint32_t freq_hz,
                      uint16_t top, bool fase, bool inv);

/* Imprime los registros del slice leyendolos directo con punteros */
void pwm_regs_dump(void);

#endif /* PWM_BACKEND_H */
