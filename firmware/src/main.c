/* =========================================================================
 *  Actividad integradora - Equipo 2 - PWM + Puente H L298N + Motor DC
 *  Monitor REPL del curso + comandos motor / pwm / demo
 *
 *  Compila para Raspberry Pi Pico (RP2040) y Pico 2 W (RP2350) con el
 *  mismo codigo; lo que cambia entre chips esta en plataforma.h.
 *
 *  Cadena: comando REPL -> C (argc/argv) -> backend (regs o SDK)
 *          -> registros PWM/SIO -> L298N -> motor DC
 * ========================================================================= */
#include <stdio.h>
#include <string.h>
#include <stdint.h>
#include <stdlib.h>

#include "pico/stdlib.h"
#include "pico/stdio_usb.h"
#include "hardware/gpio.h"
#include "hardware/clocks.h"
#include "hardware/structs/sio.h"

#include "plataforma.h"
#include "motor.h"

#define LED_PIN 15   /* LED externo (en la Pico 2 W el LED integrado no esta en GPIO25) */
#define BUTTON_PIN 13
#define LED_MASK (1u << LED_PIN)
#define BUTTON_MASK (1u << BUTTON_PIN)

#define LINE_BUFFER_SIZE 64
#define MAX_ARGS 4

typedef int (*command_handler_t)(int argc, char **argv);

typedef struct {
    const char *name;
    const char *help;
    command_handler_t handler;
} command_t;

static int cmd_help(int argc, char **argv);
static int cmd_on(int argc, char **argv);
static int cmd_off(int argc, char **argv);
static int cmd_toggle(int argc, char **argv);
static int cmd_read(int argc, char **argv);
static int cmd_sio(int argc, char **argv);
static int cmd_mask(int argc, char **argv);
static int cmd_status(int argc, char **argv);
static int cmd_motor(int argc, char **argv);
static int cmd_pwm(int argc, char **argv);
static int cmd_demo(int argc, char **argv);

static const command_t commands[] = {
    {"help",   "Muestra la lista de comandos ",          cmd_help},
    {"on",     "Enciende el LED externo GPIO15",       cmd_on},
    {"off",    "Apaga el LED externo GPIO15",          cmd_off},
    {"toggle", "Invierte el estado del LED",           cmd_toggle},
    {"read",   "Lee el boton conectado a GPIO13",      cmd_read},
    {"sio",    "Muestra GPIO_IN, GPIO_OUT y GPIO_OE",  cmd_sio},
    {"mask",   "Muestra la mascara para GPIO15",        cmd_mask},
    {"status", "Estado del LED GPIO15 y del boton GPIO13", cmd_status},
    {"motor",  "motor <0-100> <derecha|izquierda> | motor stop | motor estado", cmd_motor},
    {"pwm",    "pwm regs|info | pwm modo <regs|sdk> | pwm freq|top <n> | pwm fase|inv <on|off>", cmd_pwm},
    {"demo",   "Secuencia CHALLENGE 06 (derecha 100 %, izquierda 75 %)", cmd_demo},
};

#define COMMAND_COUNT (sizeof(commands) / sizeof(commands[0]))

static void print_binary32(uint32_t value) {
    for (int bit = 31; bit >= 0; --bit) {
        putchar((value & (1u << bit)) ? '1' : '0');
        if ((bit % 4) == 0 && bit != 0) putchar(' ');
    }
}

static void read_line(char *buffer, size_t size) {
    size_t pos = 0;

    while (true) {
        int c = getchar();

        if (c == '\r' || c == '\n') {
            putchar('\n');
            buffer[pos] = '\0';
            return;
        }

        if (c == '\b' || c == 127) {
            if (pos > 0) {
                pos--;
                printf("\b \b");
            }
            continue;
        }

        if (pos < size - 1) {
            buffer[pos++] = (char)c;
            putchar(c);
        }
    }
}

static int split_line(char *line, char **argv, int max_args) {
    int argc = 0;
    char *token = strtok(line, " \t");

    while (token != NULL && argc < max_args) {
        argv[argc++] = token;
        token = strtok(NULL, " \t");
    }
    return argc;
}

static void execute_command(int argc, char **argv) {
    if (argc == 0) return;

    for (size_t i = 0; i < COMMAND_COUNT; ++i) {
        if (strcmp(argv[0], commands[i].name) == 0) {
            commands[i].handler(argc, argv);
            return;
        }
    }

    printf("Comando desconocido: %s\n", argv[0]);
    printf("Escribe 'help'.\n");
}

/* Convierte argv[i] a entero y valida el rango. strtol + endptr detecta
 * texto como "50x" o "abc" que atoi() aceptaria en silencio. */
static bool parse_uint(const char *txt, uint32_t min, uint32_t max, uint32_t *out) {
    char *fin;
    long v = strtol(txt, &fin, 10);
    if (fin == txt || *fin != '\0' || v < (long)min || v > (long)max) return false;
    *out = (uint32_t)v;
    return true;
}

static bool parse_on_off(const char *txt, bool *out) {
    if (strcmp(txt, "on") == 0)  { *out = true;  return true; }
    if (strcmp(txt, "off") == 0) { *out = false; return true; }
    return false;
}

/* ---------------- Comandos originales del REPL ---------------- */
static int cmd_help(int argc, char **argv) {
    (void)argc; (void)argv;
    printf("\nComandos disponibles:\n");
    for (size_t i = 0; i < COMMAND_COUNT; ++i)
        printf("  %-8s - %s\n", commands[i].name, commands[i].help);
    return 0;
}

static int cmd_on(int argc, char **argv) {
    (void)argc; (void)argv;
    sio_hw->gpio_set = LED_MASK;
    printf("LED GPIO%d = 1\n", LED_PIN);
    printf("sio_hw->gpio_set = 1u << %d;\n", LED_PIN);
    return 0;
}

static int cmd_off(int argc, char **argv) {
    (void)argc; (void)argv;
    sio_hw->gpio_clr = LED_MASK;
    printf("LED GPIO%d = 0\n", LED_PIN);
    printf("sio_hw->gpio_clr = 1u << %d;\n", LED_PIN);
    return 0;
}

static int cmd_toggle(int argc, char **argv) {
    (void)argc; (void)argv;
    sio_hw->gpio_togl = LED_MASK;
    printf("GPIO%d invertido con GPIO_OUT_XOR\n", LED_PIN);
    return 0;
}

static int cmd_read(int argc, char **argv) {
    (void)argc; (void)argv;
    uint32_t entradas = sio_hw->gpio_in;
    bool boton = (entradas & BUTTON_MASK) != 0;

    printf("GPIO_IN = 0x%08lx\n", (unsigned long)entradas);
    printf("GPIO%d = %d\n", BUTTON_PIN, boton ? 1 : 0);
    return 0;
}

static int cmd_sio(int argc, char **argv) {
    (void)argc; (void)argv;

    uint32_t gpio_in  = sio_hw->gpio_in;
    uint32_t gpio_out = sio_hw->gpio_out;
    uint32_t gpio_oe  = sio_hw->gpio_oe;

    printf("\nSIO_BASE = 0xD0000000\n");

    printf("GPIO_IN  = 0x%08lx  ", (unsigned long)gpio_in);
    print_binary32(gpio_in);
    printf("\n");

    printf("GPIO_OUT = 0x%08lx  ", (unsigned long)gpio_out);
    print_binary32(gpio_out);
    printf("\n");

    printf("GPIO_OE  = 0x%08lx  ", (unsigned long)gpio_oe);
    print_binary32(gpio_oe);
    printf("\n");

    return 0;
}

static int cmd_mask(int argc, char **argv) {
    (void)argc; (void)argv;

    printf("1u << %d\n", LED_PIN);
    printf("Hex: 0x%08lx\n", (unsigned long)LED_MASK);
    printf("Bin: ");
    print_binary32(LED_MASK);
    printf("\n");

    return 0;
}

static int cmd_status(int argc, char **argv) {
    (void)argc; (void)argv;

    uint32_t salidas  = sio_hw->gpio_out;
    uint32_t entradas = sio_hw->gpio_in;

    uint32_t led   = (salidas  & LED_MASK)    >> LED_PIN;
    uint32_t boton = (entradas & BUTTON_MASK) >> BUTTON_PIN;

    printf("GPIO%d - LED   : %lu\n", LED_PIN,    (unsigned long)led);
    printf("GPIO%d - BOTON : %lu\n", BUTTON_PIN, (unsigned long)boton);

    return 0;
}

/* ---------------- Comando del equipo: motor ---------------- */
static void motor_uso(void) {
    printf("Uso: motor <0-100> <derecha|izquierda>\n");
    printf("     motor stop\n");
    printf("     motor estado\n");
    printf("Ej.: motor 50 derecha\n");
}

static void motor_imprimir_estado(void) {
    const pwm_cfg_t *c = motor_cfg();
    uint32_t v = motor_velocidad();
    uint32_t cc = motor_cc_para(v);

    printf("Motor     : %s a %lu %% logico\n", motor_dir_nombre(motor_direccion()),
           (unsigned long)v);
    printf("ENA GPIO%u : CC.A = %lu / TOP+1 = %lu  -> duty real %lu %%\n", PIN_ENA,
           (unsigned long)cc, (unsigned long)c->top + 1u,
           (unsigned long)((cc * 100u) / ((uint32_t)c->top + 1u)));
    printf("PWM       : %lu Hz, backend %s\n", (unsigned long)c->freq_hz,
           motor_backend()->nombre);
}

static int cmd_motor(int argc, char **argv) {
    if (argc == 2 && strcmp(argv[1], "stop") == 0) {
        motor_stop();
        return 0;
    }
    if (argc == 2 && strcmp(argv[1], "estado") == 0) {
        motor_imprimir_estado();
        return 0;
    }
    if (argc != 3) {
        motor_uso();
        return -1;
    }

    uint32_t vel;
    if (!parse_uint(argv[1], 0u, 100u, &vel)) {
        printf("Error: velocidad '%s' invalida (entero 0-100)\n", argv[1]);
        return -1;
    }

    motor_dir_t dir;
    if (strcmp(argv[2], "derecha") == 0 || strcmp(argv[2], "adelante") == 0) {
        dir = DERECHA;
    } else if (strcmp(argv[2], "izquierda") == 0 || strcmp(argv[2], "atras") == 0) {
        dir = IZQUIERDA;
    } else {
        printf("Error: direccion '%s' invalida (derecha|izquierda)\n", argv[2]);
        return -1;
    }

    printf("argv[1] = \"%s\" -> %lu %%   argv[2] = \"%s\" -> %s\n", argv[1],
           (unsigned long)vel, argv[2], motor_dir_nombre(dir));
    motor_mover(vel, dir);
    motor_imprimir_estado();
    return 0;
}

/* ---------------- Comando del equipo: pwm ---------------- */
static void pwm_uso(void) {
    printf("Uso: pwm regs              registros del slice (lectura con punteros)\n");
    printf("     pwm info              direcciones y diferencias de este chip\n");
    printf("     pwm modo <regs|sdk>   implementacion activa\n");
    printf("     pwm freq <hz>         frecuencia (recalcula DIV)\n");
    printf("     pwm top <99-65534>    resolucion (TOP)\n");
    printf("     pwm fase <on|off>     phase-correct (cuenta arriba/abajo)\n");
    printf("     pwm inv <on|off>      invierte la salida del canal A\n");
}

static void pwm_info(void) {
    printf("\nTarjeta   : %s (%s)\n", TARJETA_NOMBRE, CHIP_NOMBRE);
    printf("clk_sys   : %lu Hz\n", (unsigned long)clock_get_hz(clk_sys));
    printf("PWM_BASE  : 0x%08lx   slices: %u   EN @ +0x%03x\n",
           (unsigned long)PWM_BASE_ADDR, PWM_NUM_SLICES, PWM_EN_OFS);
    printf("RESETS    : 0x%08lx   bit PWM = %u\n",
           (unsigned long)RESETS_BASE_ADDR, RESETS_PWM_BIT);
    printf("IO_BANK0  : 0x%08lx   PADS_BANK0: 0x%08lx  (ISO en pads: %s)\n",
           (unsigned long)IO_BANK0_BASE_ADDR, (unsigned long)PADS_BANK0_BASE_ADDR,
           PADS_TIENE_ISO ? "si" : "no");
    printf("SIO       : 0x%08lx   OUT_SET +0x%03x  OUT_CLR +0x%03x  OE_SET +0x%03x\n",
           (unsigned long)SIO_BASE_ADDR, SIO_GPIO_OUT_SET_OFS, SIO_GPIO_OUT_CLR_OFS,
           SIO_GPIO_OE_SET_OFS);
    printf("GPIO%u -> slice %u canal A   (slice = (gpio >> 1) & 7)\n",
           PIN_ENA, PWM_SLICE_ENA);
}

static int cmd_pwm(int argc, char **argv) {
    if (argc == 2 && strcmp(argv[1], "regs") == 0) {
        pwm_regs_dump();
        return 0;
    }
    if (argc == 2 && strcmp(argv[1], "info") == 0) {
        pwm_info();
        return 0;
    }
    if (argc != 3) {
        pwm_uso();
        return -1;
    }

    uint32_t n;
    bool b;
    bool ok;

    if (strcmp(argv[1], "modo") == 0) {
        if (strcmp(argv[2], "regs") == 0)      motor_set_backend(&pwm_backend_regs);
        else if (strcmp(argv[2], "sdk") == 0)  motor_set_backend(&pwm_backend_sdk);
        else { printf("Error: modo '%s' invalido (regs|sdk)\n", argv[2]); return -1; }
        printf("Backend activo: %s\n", motor_backend()->nombre);
        return 0;
    }

    if (strcmp(argv[1], "freq") == 0) {
        ok = parse_uint(argv[2], 1u, 1000000u, &n) && motor_set_freq(n);
    } else if (strcmp(argv[1], "top") == 0) {
        ok = parse_uint(argv[2], 99u, 65534u, &n) && motor_set_top(n);
    } else if (strcmp(argv[1], "fase") == 0) {
        ok = parse_on_off(argv[2], &b) && motor_set_fase(b);
    } else if (strcmp(argv[1], "inv") == 0) {
        ok = parse_on_off(argv[2], &b) && motor_set_inv(b);
    } else {
        pwm_uso();
        return -1;
    }

    if (!ok) {
        printf("Error: '%s %s' fuera de rango. El divisor debe quedar entre 1.0 y 255.94\n",
               argv[1], argv[2]);
        printf("       (f = clk_sys / (DIV * (TOP+1) * fase)). Prueba 'pwm info'.\n");
        return -1;
    }

    const pwm_cfg_t *c = motor_cfg();
    printf("OK: f = %lu Hz  TOP = %u  DIV = %u + %u/16  fase=%d inv=%d\n",
           (unsigned long)c->freq_hz, c->top, c->div_int, c->div_frac, c->fase, c->inv);
    return 0;
}

/* ---------------- Comando del equipo: demo ---------------- */
static int cmd_demo(int argc, char **argv) {
    (void)argc; (void)argv;
    printf("\n--- CHALLENGE 06 ---\n");
    motor_mover(100u, DERECHA);       /* 1-2) derecha, rampa 0 -> 100  */
    motor_mantener(2000u);            /* 3)   mantener 2 s             */
    motor_mover(75u, IZQUIERDA);      /* 4-6) rampa a 0, invertir, 75  */
    motor_mantener(2000u);            /* 7)   mantener 2 s             */
    motor_stop();                     /* 8-9) rampa a 0 y STOP         */
    return 0;
}

static void hardware_init(void) {
    gpio_set_function(LED_PIN, GPIO_FUNC_SIO);
    sio_hw->gpio_clr = LED_MASK;
    sio_hw->gpio_oe_set = LED_MASK;

    gpio_set_function(BUTTON_PIN, GPIO_FUNC_SIO);
    sio_hw->gpio_oe_clr = BUTTON_MASK;
    gpio_pull_down(BUTTON_PIN);

    /* Motor quieto desde el arranque; por defecto acceso directo a registros */
    motor_init(&pwm_backend_regs);
}

int main(void) {
    stdio_init_all();
    hardware_init();

    /* Esperar a que se abra el monitor serie para no perder el banner
     * (el motor ya esta detenido desde hardware_init) */
    while (!stdio_usb_connected()) {
        sleep_ms(50);
    }
    sleep_ms(200);

    printf("\n=====================================\n");
    printf(" Monitor REPL %s - PWM Motor DC\n", CHIP_NOMBRE);
    printf(" Equipo 2 - L298N: IN1=GP%u IN2=GP%u ENA=GP%u\n", PIN_IN1, PIN_IN2, PIN_ENA);
    printf("=====================================\n");
    printf("Escribe 'help' para comenzar.\n\n");

    char line[LINE_BUFFER_SIZE];
    char *argv[MAX_ARGS];

    while (true) {
        printf("%s> ", CHIP_NOMBRE);
        read_line(line, sizeof(line));
        int argc = split_line(line, argv, MAX_ARGS);
        execute_command(argc, argv);
    }
}
