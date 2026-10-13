/* =========================================================================
 *  pwm_regs.c - PWM + GPIO del puente H con acceso DIRECTO a registros
 *
 *  Todo pasa por punteros volatile a direcciones de plataforma.h.
 *  volatile: le dice al compilador que el valor puede cambiar fuera del
 *  programa (lo cambia el hardware) y que cada lectura/escritura debe
 *  hacerse de verdad, en orden, sin guardarla en un registro del CPU.
 * ========================================================================= */
#include <stdio.h>

#include "hardware/clocks.h"     /* solo para clock_get_hz(clk_sys) */

#include "plataforma.h"
#include "pwm_backend.h"

/* ---------------- Punteros a los registros del slice 2 ---------------- */
static volatile uint32_t *const pwm_csr = (volatile uint32_t *)PWM_SLICE_REG(PWM_SLICE_ENA, PWM_CSR_OFS);
static volatile uint32_t *const pwm_div = (volatile uint32_t *)PWM_SLICE_REG(PWM_SLICE_ENA, PWM_DIV_OFS);
static volatile uint32_t *const pwm_ctr = (volatile uint32_t *)PWM_SLICE_REG(PWM_SLICE_ENA, PWM_CTR_OFS);
static volatile uint32_t *const pwm_cc  = (volatile uint32_t *)PWM_SLICE_REG(PWM_SLICE_ENA, PWM_CC_OFS);
static volatile uint32_t *const pwm_top = (volatile uint32_t *)PWM_SLICE_REG(PWM_SLICE_ENA, PWM_TOP_OFS);
static volatile uint32_t *const pwm_en  = (volatile uint32_t *)(PWM_BASE_ADDR + PWM_EN_OFS);

/* ---------------- Funciones auxiliares ---------------- */

/* Saca al PWM de reset: escribe 1 en el alias CLR de RESETS.RESET y espera
 * RESET_DONE. (El SDK ya lo hace al arrancar; repetirlo no hace dano.) */
static void pwm_quitar_reset(void) {
    REG32(RESETS_BASE_ADDR + ALIAS_CLR + RESETS_RESET_OFS) = 1u << RESETS_PWM_BIT;
    while ((REG32(RESETS_BASE_ADDR + RESETS_RESET_DONE_OFS) & (1u << RESETS_PWM_BIT)) == 0u) {
        /* espera activa */
    }
}

/* Equivalente a gpio_set_function(): pad listo + FUNCSEL en IO_BANK0 */
static void gpio_funcsel(uint32_t pin, uint32_t funcsel) {
    volatile uint32_t *pad  = (volatile uint32_t *)(PADS_BANK0_BASE_ADDR + PADS_GPIO_OFS(pin));
    volatile uint32_t *ctrl = (volatile uint32_t *)(IO_BANK0_BASE_ADDR + IO_GPIO_CTRL_OFS(pin));

    *pad  = (*pad & ~PAD_OD_BIT) | PAD_IE_BIT;     /* salida habilitada    */
    *ctrl = funcsel & IO_CTRL_FUNCSEL_MASK;         /* resto de campos = 0  */
#if PADS_TIENE_ISO
    /* RP2350: el pad sale de reset aislado; hasta limpiar ISO no conecta */
    REG32(PADS_BANK0_BASE_ADDR + ALIAS_CLR + PADS_GPIO_OFS(pin)) = PAD_ISO_BIT;
#endif
}

static void sio_set(uint32_t mask) { REG32(SIO_BASE_ADDR + SIO_GPIO_OUT_SET_OFS) = mask; }
static void sio_clr(uint32_t mask) { REG32(SIO_BASE_ADDR + SIO_GPIO_OUT_CLR_OFS) = mask; }

/* ---------------- Backend ---------------- */
static void regs_direccion(bool in1, bool in2);

static void regs_init(void) {
    const uint32_t dir_mask = (1u << PIN_IN1) | (1u << PIN_IN2);

    pwm_quitar_reset();

    /* IN1 / IN2: SIO, salida, en 0 antes de habilitar la salida */
    sio_clr(dir_mask);
    gpio_funcsel(PIN_IN1, FUNCSEL_SIO);
    gpio_funcsel(PIN_IN2, FUNCSEL_SIO);
    REG32(SIO_BASE_ADDR + SIO_GPIO_OE_SET_OFS) = dir_mask;

    /* ENA: CC.A = 0 primero, luego conectar GPIO4 al PWM */
    *pwm_cc = 0u;
    gpio_funcsel(PIN_ENA, FUNCSEL_PWM);
}

static void regs_aplicar(const pwm_cfg_t *c) {
    uint32_t csr = CSR_EN;                       /* DIVMODE = 0: libre */
    if (c->fase) csr |= CSR_PH_CORRECT;
    if (c->inv)  csr |= CSR_A_INV;

    *pwm_csr = 0u;                               /* apagar mientras se configura */
    *pwm_div = ((uint32_t)c->div_int << DIV_INT_LSB) | (c->div_frac & DIV_FRAC_MASK);
    *pwm_top = c->top;
    *pwm_ctr = 0u;
    *pwm_csr = csr;                              /* EN = 1 arranca el contador */
}

static void regs_nivel(uint16_t cc_a) {
    /* CC.A son los 16 bits bajos; se conserva CC.B (bits altos) */
    *pwm_cc = (*pwm_cc & 0xFFFF0000u) | cc_a;
}

static void regs_direccion(bool in1, bool in2) {
    if (in1) sio_set(1u << PIN_IN1); else sio_clr(1u << PIN_IN1);
    if (in2) sio_set(1u << PIN_IN2); else sio_clr(1u << PIN_IN2);
}

const pwm_backend_t pwm_backend_regs = {
    .nombre    = "regs (punteros)",
    .init      = regs_init,
    .aplicar   = regs_aplicar,
    .nivel     = regs_nivel,
    .direccion = regs_direccion,
};

/* ---------------- Lectura de registros (para el comando pwm regs) ---------------- */
static void dump_reg(const char *nombre, uintptr_t addr) {
    printf("  %-14s 0x%08lx = 0x%08lx\n", nombre, (unsigned long)addr,
           (unsigned long)REG32(addr));
}

void pwm_regs_dump(void) {
    uint32_t csr = *pwm_csr;
    uint32_t div = *pwm_div;
    uint32_t top = *pwm_top & 0xFFFFu;
    uint32_t cc  = *pwm_cc;
    uint32_t cca = cc & 0xFFFFu;
    uint32_t clk = clock_get_hz(clk_sys);

    uint32_t div_int  = (div & DIV_INT_MASK) >> DIV_INT_LSB;
    uint32_t div_frac = div & DIV_FRAC_MASK;
    uint32_t fase     = (csr & CSR_PH_CORRECT) ? 2u : 1u;

    printf("\n%s  PWM_BASE = 0x%08lx  slice %u (GPIO%u = canal A)\n",
           CHIP_NOMBRE, (unsigned long)PWM_BASE_ADDR, PWM_SLICE_ENA, PIN_ENA);
    printf("  offset del slice = 0x14 * %u = 0x%02x\n",
           PWM_SLICE_ENA, PWM_CH_STRIDE * PWM_SLICE_ENA);
    dump_reg("CH2_CSR", PWM_SLICE_REG(PWM_SLICE_ENA, PWM_CSR_OFS));
    dump_reg("CH2_DIV", PWM_SLICE_REG(PWM_SLICE_ENA, PWM_DIV_OFS));
    dump_reg("CH2_CTR", PWM_SLICE_REG(PWM_SLICE_ENA, PWM_CTR_OFS));
    dump_reg("CH2_CC",  PWM_SLICE_REG(PWM_SLICE_ENA, PWM_CC_OFS));
    dump_reg("CH2_TOP", PWM_SLICE_REG(PWM_SLICE_ENA, PWM_TOP_OFS));
    dump_reg("EN",      PWM_BASE_ADDR + PWM_EN_OFS);
    dump_reg("GPIO4_CTRL", IO_BANK0_BASE_ADDR + IO_GPIO_CTRL_OFS(PIN_ENA));
    dump_reg("PADS GPIO4", PADS_BANK0_BASE_ADDR + PADS_GPIO_OFS(PIN_ENA));

    printf("\n  CSR: EN=%lu PH_CORRECT=%lu A_INV=%lu B_INV=%lu DIVMODE=%lu\n",
           (unsigned long)(csr & CSR_EN),
           (unsigned long)((csr & CSR_PH_CORRECT) != 0u),
           (unsigned long)((csr & CSR_A_INV) != 0u),
           (unsigned long)((csr & CSR_B_INV) != 0u),
           (unsigned long)((csr & CSR_DIVMODE_MASK) >> CSR_DIVMODE_LSB));
    printf("  DIV: INT=%lu FRAC=%lu/16   TOP=%lu   CC.A=%lu  CC.B=%lu\n",
           (unsigned long)div_int, (unsigned long)div_frac, (unsigned long)top,
           (unsigned long)cca, (unsigned long)(cc >> 16));
    printf("  EN bit %u = %lu   (EN refleja CSR.EN de cada slice)\n", PWM_SLICE_ENA,
           (unsigned long)((*pwm_en >> PWM_SLICE_ENA) & 1u));

    /* f = clk / (DIV * (TOP+1) * fase), con DIV = INT + FRAC/16 */
    uint32_t div16 = div_int * 16u + div_frac;
    if (div16 != 0u) {
        uint64_t den = (uint64_t)div16 * (top + 1u) * fase;
        uint32_t f_centi = (uint32_t)(((uint64_t)clk * 1600u) / den);
        uint32_t duty_x10 = (cca >= top + 1u) ? 1000u : (cca * 1000u) / (top + 1u);
        printf("  clk_sys = %lu Hz  ->  f = %lu.%02lu Hz   duty = %lu.%lu %%\n",
               (unsigned long)clk, (unsigned long)(f_centi / 100u),
               (unsigned long)(f_centi % 100u),
               (unsigned long)(duty_x10 / 10u), (unsigned long)(duty_x10 % 10u));
    }
}
