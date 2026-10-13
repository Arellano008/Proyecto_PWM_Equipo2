# == DO NOT EDIT THE FOLLOWING LINES for the Raspberry Pi Pico VS Code Extension to work ==
if(WIN32)
    set(USERHOME $ENV{USERPROFILE})
else()
    set(USERHOME $ENV{HOME})
endif()
set(sdkVersion 2.3.1)
set(toolchainVersion 15_2_Rel1)
set(picotoolVersion 2.3.1)
set(picoVscode ${USERHOME}/.pico-sdk/cmake/pico-vscode.cmake)
if (EXISTS ${picoVscode})
    include(${picoVscode})
endif()
# ====================================================================================
cmake_minimum_required(VERSION 3.13)

# pico    -> Raspberry Pi Pico   (RP2040)
# pico2_w -> Raspberry Pi Pico 2 W (RP2350)
set(PICO_BOARD pico2_w CACHE STRING "Board type")

include(pico_sdk_import.cmake)

project(Motor_PWM C CXX ASM)

set(CMAKE_C_STANDARD 11)
set(CMAKE_CXX_STANDARD 17)

pico_sdk_init()

# El ejecutable se llama igual que el proyecto (Motor_PWM.elf / .uf2) porque
# la extension de VS Code busca build/<proyecto>.elf para cargarlo.
# build.sh copia el .uf2 a uf2/ con el chip en el nombre.
if (PICO_PLATFORM STREQUAL "rp2040")
    set(CHIP RP2040)
else()
    set(CHIP RP2350)
endif()
set(TARGET Motor_PWM)

add_executable(${TARGET}
    src/main.c
    src/motor.c
    src/pwm_regs.c
    src/pwm_sdk.c
    src/verifica_mapa.c
)

target_include_directories(${TARGET} PRIVATE src)
target_compile_options(${TARGET} PRIVATE -Wall -Wextra)

target_link_libraries(${TARGET}
    pico_stdlib
    hardware_gpio
    hardware_pwm
    hardware_clocks
)

pico_enable_stdio_usb(${TARGET} 1)
pico_enable_stdio_uart(${TARGET} 0)

pico_set_program_name(${TARGET} "Motor_PWM_${CHIP}")

pico_add_extra_outputs(${TARGET})
