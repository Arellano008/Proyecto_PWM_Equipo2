#!/usr/bin/env bash
# Compila el firmware para las dos tarjetas y deja los .uf2 en uf2/
set -e
cd "$(dirname "$0")"

export PICO_SDK_PATH="${PICO_SDK_PATH:-$HOME/.pico-sdk/sdk/2.3.1}"
export PICO_TOOLCHAIN_PATH="${PICO_TOOLCHAIN_PATH:-$HOME/.pico-sdk/toolchain/15_2_Rel1}"
export PATH="$PICO_TOOLCHAIN_PATH/bin:$HOME/.pico-sdk/cmake/v4.3.4/bin:$HOME/.pico-sdk/ninja/v1.13.2:$PATH"

mkdir -p uf2
for board in pico pico2_w; do
    case "$board" in pico) chip=RP2040 ;; *) chip=RP2350 ;; esac
    echo "==== PICO_BOARD=$board ($chip) ===="
    cmake -S firmware -B "build_$board" -G Ninja -DPICO_BOARD="$board"
    cmake --build "build_$board"
    cp "build_$board/Motor_PWM.uf2" "uf2/Motor_PWM_$chip.uf2"
done
ls -l uf2/
