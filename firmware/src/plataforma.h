/* =========================================================================
 *  plataforma.h - Mapa de registros escrito A MANO para RP2040 y RP2350
 *
 *  A proposito NO se usan los headers hardware/regs/... del SDK: cada
 *  direccion, offset y bit sale del datasheet y se escribe aqui para poder
 *  explicarlo y compararlo entre chips.
 *
 *  Fuentes:
 *    RP2040 Datasheet  - 2.14 Subsystem Resets, 2.19 GPIO, 4.5 PWM
 *    RP2350 Datasheet  - 7.5 Resets, 9 GPIO, 12.5 PWM
 *
 *  Secuencia: direccion base -> offset -> direccion del registro
 *             -> puntero volatile -> valor -> periferico
 * ========================================================================= */
#ifndef PLATAFORMA_H
#define PLATAFORMA_H

#include <stdint.h>

/* ---------------- Lo que CAMBIA entre chips ---------------- */
#if PICO_RP2040

#define CHIP_NOMBRE            "RP2040"
#define TARJETA_NOMBRE         "Raspberry Pi Pico"

#define RESETS_BASE_ADDR       0x4000C000u
#define IO_BANK0_BASE_ADDR     0x40014000u
#define PADS_BANK0_BASE_ADDR   0x4001C000u
#define PWM_BASE_ADDR          0x40050000u

#define RESETS_PWM_BIT         14u      /* RESETS.RESET bit 14 = pwm        */
#define PWM_NUM_SLICES         8u       /* 8 slices x 2 canales = 16 salidas */

#define PWM_EN_OFS             0x0A0u   /* EN   : 1 bit por slice [7:0]     */
#define PWM_INTR_OFS           0x0A4u   /* INTR : wrap pendiente            */
#define PWM_INTE_OFS           0x0A8u   /* una sola linea IRQ (PWM_IRQ_WRAP)*/

#define SIO_GPIO_OUT_SET_OFS   0x014u
#define SIO_GPIO_OUT_CLR_OFS   0x018u
#define SIO_GPIO_OE_SET_OFS    0x024u

#define PADS_TIENE_ISO         0        /* RP2040 no tiene bit ISO en pads  */

#elif PICO_RP2350

#define CHIP_NOMBRE            "RP2350"
#define TARJETA_NOMBRE         "Raspberry Pi Pico 2 W"

#define RESETS_BASE_ADDR       0x40020000u
#define IO_BANK0_BASE_ADDR     0x40028000u
#define PADS_BANK0_BASE_ADDR   0x40038000u
#define PWM_BASE_ADDR          0x400A8000u

#define RESETS_PWM_BIT         16u      /* RESETS.RESET bit 16 = pwm        */
#define PWM_NUM_SLICES         12u      /* 12 slices (8-11 solo en QFN-80)  */

#define PWM_EN_OFS             0x0F0u   /* EN   : 1 bit por slice [11:0]    */
#define PWM_INTR_OFS           0x0F4u
#define PWM_INTE_OFS           0x0F8u   /* IRQ0_INTE (hay IRQ0 e IRQ1)      */

#define SIO_GPIO_OUT_SET_OFS   0x018u
#define SIO_GPIO_OUT_CLR_OFS   0x020u
#define SIO_GPIO_OE_SET_OFS    0x038u

#define PADS_TIENE_ISO         1        /* pads arrancan aislados (ISO=1)   */

#else
#error "Plataforma no soportada: compila con PICO_BOARD=pico o pico2_w"
#endif

/* ---------------- Lo que es IGUAL en ambos chips ---------------- */
#define SIO_BASE_ADDR          0xD0000000u

/* Alias atomicos del bus APB/AHB: escribir en base+0x2000 hace OR (SET),
 * en base+0x3000 hace AND-NOT (CLR). Evitan leer-modificar-escribir. */
#define ALIAS_XOR              0x1000u
#define ALIAS_SET              0x2000u
#define ALIAS_CLR              0x3000u

/* RESETS */
#define RESETS_RESET_OFS       0x000u   /* RW: 1 = periferico en reset      */
#define RESETS_RESET_DONE_OFS  0x008u   /* RO: 1 = ya salio de reset        */

/* IO_BANK0: GPIOx_STATUS = base + 8x, GPIOx_CTRL = base + 8x + 4 */
#define IO_GPIO_CTRL_OFS(x)    (0x008u * (x) + 0x004u)
#define IO_CTRL_FUNCSEL_MASK   0x1Fu    /* bits [4:0]                       */
#define FUNCSEL_PWM            4u
#define FUNCSEL_SIO            5u

/* PADS_BANK0: VOLTAGE_SELECT en 0x00, GPIOx = base + 4 + 4x */
#define PADS_GPIO_OFS(x)       (0x004u + 0x004u * (x))
#define PAD_ISO_BIT            (1u << 8) /* solo RP2350                     */
#define PAD_OD_BIT             (1u << 7) /* Output Disable                  */
#define PAD_IE_BIT             (1u << 6) /* Input Enable                    */

/* PWM: cada slice ocupa 0x14 bytes (5 registros de 32 bits) */
#define PWM_CH_STRIDE          0x014u
#define PWM_CSR_OFS            0x000u   /* control y estado                 */
#define PWM_DIV_OFS            0x004u   /* divisor de reloj 8.4             */
#define PWM_CTR_OFS            0x008u   /* contador actual                  */
#define PWM_CC_OFS             0x00Cu   /* comparadores A [15:0] B [31:16]  */
#define PWM_TOP_OFS            0x010u   /* valor de wrap                    */

/* Campos de CHn_CSR */
#define CSR_EN                 (1u << 0)
#define CSR_PH_CORRECT         (1u << 1)
#define CSR_A_INV              (1u << 2)
#define CSR_B_INV              (1u << 3)
#define CSR_DIVMODE_LSB        4u        /* [5:4] 0=libre 1=gate B 2=sube B 3=baja B */
#define CSR_DIVMODE_MASK       (3u << CSR_DIVMODE_LSB)
#define CSR_PH_RET             (1u << 6)
#define CSR_PH_ADV             (1u << 7)

/* Campos de CHn_DIV: INT [11:4], FRAC [3:0] -> divisor = INT + FRAC/16 */
#define DIV_INT_LSB            4u
#define DIV_INT_MASK           (0xFFu << DIV_INT_LSB)
#define DIV_FRAC_MASK          0x0Fu

/* Acceso a un registro de 32 bits a partir de su direccion absoluta */
#define REG32(addr)            (*(volatile uint32_t *)(uintptr_t)(addr))

/* Direccion absoluta de un registro de un slice */
#define PWM_SLICE_REG(slice, ofs) \
    (PWM_BASE_ADDR + PWM_CH_STRIDE * (slice) + (ofs))

#endif /* PLATAFORMA_H */
