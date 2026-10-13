/* =========================================================================
 *  motor.h - Control del motor DC con L298N (logica de la Sesion 06 en C)
 * ========================================================================= */
#ifndef MOTOR_H
#define MOTOR_H

#include <stdint.h>
#include <stdbool.h>

#include "pwm_backend.h"

/* Ajustes del motor fisico (medidos en la Sesion 06) */
#define PWM_FREQ_DEFAULT    100u    /* Hz. A 1 kHz el motor no arrancaba      */
#define PWM_TOP_DEFAULT     9999u   /* 10000 pasos -> 0.01 % de resolucion   */
#define MIN_DUTY            40u     /* % real minimo con el que el motor gira */
#define KICK_MS             120u    /* empujon al 100 % al arrancar desde 0  */
#define PASO_RAMPA          5u      /* % por paso de rampa                   */
#define DELAY_RAMPA_MS      100u    /* 0 -> 100 % en 2 s                     */
#define PAUSA_INVERSION_MS  500u    /* motor detenido antes de invertir      */

typedef enum {
    DETENIDO = 0,
    DERECHA  = 1,   /* FORWARD: IN1 = 1, IN2 = 0 */
    IZQUIERDA = 2,  /* REVERSE: IN1 = 0, IN2 = 1 */
} motor_dir_t;

void motor_init(const pwm_backend_t *backend);
void motor_set_backend(const pwm_backend_t *backend);
const pwm_backend_t *motor_backend(void);

/* Mueve el motor: si cambia el sentido baja a 0 % primero (rampa) */
void motor_mover(uint32_t porcentaje, motor_dir_t dir);
void motor_stop(void);
void motor_mantener(uint32_t ms);

/* Opciones de configuracion del PWM. Regresan false si no son validas. */
bool motor_set_freq(uint32_t freq_hz);
bool motor_set_top(uint32_t top);
bool motor_set_fase(bool fase);
bool motor_set_inv(bool inv);

uint32_t motor_velocidad(void);
motor_dir_t motor_direccion(void);
const char *motor_dir_nombre(motor_dir_t dir);
const pwm_cfg_t *motor_cfg(void);
uint16_t motor_cc_para(uint32_t porcentaje);

#endif /* MOTOR_H */
