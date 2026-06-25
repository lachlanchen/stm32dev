#!/usr/bin/env bash
set -euo pipefail

ROOT_DIR="$(cd "$(dirname "${BASH_SOURCE[0]}")/.." && pwd)"

export STM32_ROOT="$ROOT_DIR"
export OPENOCD_SCRIPTS="$ROOT_DIR/tools/xpack-openocd-0.12.0-7/openocd/scripts"

if [[ -x "$ROOT_DIR/tools/xpack-arm-none-eabi-gcc-15.2.1-1.1/bin/arm-none-eabi-gcc" ]]; then
  export PATH="$ROOT_DIR/tools/xpack-arm-none-eabi-gcc-15.2.1-1.1/bin:$PATH"
fi

if [[ -x "$ROOT_DIR/tools/xpack-openocd-0.12.0-7/bin/openocd" ]]; then
  export PATH="$ROOT_DIR/tools/xpack-openocd-0.12.0-7/bin:$PATH"
fi
