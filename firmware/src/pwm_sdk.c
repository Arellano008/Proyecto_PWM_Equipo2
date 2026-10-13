/* =========================================================================
 *  pwm_sdk.c - La MISMA configuracion que pwm_regs.c, ahora con el Pico SDK
 *
 *  Cada funcion del SDK termina escribiendo los mismos registros:
 *    gpio_set_function()       -> PADS_BANK0 GPIOx + IO_BANK0 GPIOx_CTRL
 *    pwm_gpio_to_slice_num()   -> (gpio >> 1) & 7
 *    pwm_config_set_clkdiv_*() -> CHn_DIV
 *    pwm_config_set_wrap()     -> CHn_TOP
 *    pwm_config_set_*()        -> bits de CHn_CSR
 *    pwm_init()                -> CSR=0, CTR=0, CC=0, TOP, DIV, CSR|EN
 *    pwm_set_chan_level()      -> CHn_CC (16 bits del canal)
 *    gpio_put()                -> SIO GPIO_OUT_SET / GPIO_OUT_CLR
 *  El SDK ya sabe la direccion base y los offsets de cada chip.
 * ========================================================================= */
#include "pico/stdlib.h"
#include "hardware/gpio.h"
#include "hardware/pwm.h"

#include "pwm_backend.h"

static void sdk_init(void) {
    gpio_init(PIN_IN1);
    gpio_init(PIN_IN2);
    gpio_put(PIN_IN1, 0);
    gpio_put(PIN_IN2, 0);
    gpio_set_dir(PIN_IN1, GPIO_OUT);
    gpio_set_dir(PIN_IN2, GPIO_OUT);

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

    pwm_init(slice, &pc, true);     /* true = arrancar (CSR.EN = 1) */
}

static void sdk_nivel(uint16_t cc_a) {
    pwm_set_gpio_level(PIN_ENA, cc_a);
}

static void sdk_direccion(bool in1, bool in2) {
    gpio_put(PIN_IN1, in1);
    gpio_put(PIN_IN2, in2);
}

const pwm_backend_t pwm_backend_sdk = {
    .nombre    = "sdk (hardware_pwm)",
    .init      = sdk_init,
    .aplicar   = sdk_aplicar,
    .nivel     = sdk_nivel,
    .direccion = sdk_direccion,
};
