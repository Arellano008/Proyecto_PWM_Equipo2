/* =========================================================================
 *  verifica_mapa.c - Comprueba EN COMPILACION que el mapa escrito a mano
 *  (plataforma.h) coincide con los headers oficiales del SDK de cada chip.
 *  Si alguna direccion u offset estuviera mal, el programa no compila.
 *  No genera codigo.
 * ========================================================================= */
#include "hardware/regs/addressmap.h"
#include "hardware/regs/pwm.h"
#include "hardware/regs/resets.h"
#include "hardware/regs/sio.h"
#include "hardware/regs/io_bank0.h"
#include "hardware/regs/pads_bank0.h"

#include "plataforma.h"

_Static_assert(PWM_BASE_ADDR        == PWM_BASE,        "PWM_BASE");
_Static_assert(RESETS_BASE_ADDR     == RESETS_BASE,     "RESETS_BASE");
_Static_assert(IO_BANK0_BASE_ADDR   == IO_BANK0_BASE,   "IO_BANK0_BASE");
_Static_assert(PADS_BANK0_BASE_ADDR == PADS_BANK0_BASE, "PADS_BANK0_BASE");
_Static_assert(SIO_BASE_ADDR        == SIO_BASE,        "SIO_BASE");

_Static_assert((1u << RESETS_PWM_BIT) == RESETS_RESET_PWM_BITS, "bit PWM en RESETS");
_Static_assert(RESETS_RESET_DONE_OFS  == RESETS_RESET_DONE_OFFSET, "RESET_DONE");

_Static_assert(PWM_SLICE_REG(2, PWM_CSR_OFS) - PWM_BASE_ADDR == PWM_CH2_CSR_OFFSET, "CH2_CSR");
_Static_assert(PWM_SLICE_REG(2, PWM_DIV_OFS) - PWM_BASE_ADDR == PWM_CH2_DIV_OFFSET, "CH2_DIV");
_Static_assert(PWM_SLICE_REG(2, PWM_CTR_OFS) - PWM_BASE_ADDR == PWM_CH2_CTR_OFFSET, "CH2_CTR");
_Static_assert(PWM_SLICE_REG(2, PWM_CC_OFS)  - PWM_BASE_ADDR == PWM_CH2_CC_OFFSET,  "CH2_CC");
_Static_assert(PWM_SLICE_REG(2, PWM_TOP_OFS) - PWM_BASE_ADDR == PWM_CH2_TOP_OFFSET, "CH2_TOP");
_Static_assert(PWM_EN_OFS   == PWM_EN_OFFSET,   "PWM EN");
_Static_assert(PWM_INTR_OFS == PWM_INTR_OFFSET, "PWM INTR");

_Static_assert(CSR_EN          == PWM_CH0_CSR_EN_BITS,         "CSR.EN");
_Static_assert(CSR_PH_CORRECT  == PWM_CH0_CSR_PH_CORRECT_BITS, "CSR.PH_CORRECT");
_Static_assert(CSR_A_INV       == PWM_CH0_CSR_A_INV_BITS,      "CSR.A_INV");
_Static_assert(CSR_DIVMODE_MASK == PWM_CH0_CSR_DIVMODE_BITS,   "CSR.DIVMODE");
_Static_assert(DIV_INT_MASK    == PWM_CH0_DIV_INT_BITS,        "DIV.INT");
_Static_assert(DIV_FRAC_MASK   == PWM_CH0_DIV_FRAC_BITS,       "DIV.FRAC");

_Static_assert(SIO_GPIO_OUT_SET_OFS == SIO_GPIO_OUT_SET_OFFSET, "SIO OUT_SET");
_Static_assert(SIO_GPIO_OUT_CLR_OFS == SIO_GPIO_OUT_CLR_OFFSET, "SIO OUT_CLR");
_Static_assert(SIO_GPIO_OE_SET_OFS  == SIO_GPIO_OE_SET_OFFSET,  "SIO OE_SET");

_Static_assert(IO_GPIO_CTRL_OFS(4) == IO_BANK0_GPIO4_CTRL_OFFSET, "GPIO4_CTRL");
_Static_assert(IO_GPIO_CTRL_OFS(2) == IO_BANK0_GPIO2_CTRL_OFFSET, "GPIO2_CTRL");
_Static_assert(FUNCSEL_PWM == IO_BANK0_GPIO4_CTRL_FUNCSEL_VALUE_PWM_A_2, "FUNCSEL PWM");
_Static_assert(FUNCSEL_SIO == IO_BANK0_GPIO2_CTRL_FUNCSEL_VALUE_SIO_2,   "FUNCSEL SIO");
_Static_assert(PADS_GPIO_OFS(4) == PADS_BANK0_GPIO4_OFFSET, "PADS GPIO4");
_Static_assert(PAD_OD_BIT == PADS_BANK0_GPIO4_OD_BITS, "PAD OD");
_Static_assert(PAD_IE_BIT == PADS_BANK0_GPIO4_IE_BITS, "PAD IE");
#if PADS_TIENE_ISO
_Static_assert(PAD_ISO_BIT == PADS_BANK0_GPIO4_ISO_BITS, "PAD ISO");
#endif
