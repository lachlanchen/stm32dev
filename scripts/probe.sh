#!/usr/bin/env bash
set -euo pipefail

ROOT_DIR="$(cd "$(dirname "${BASH_SOURCE[0]}")/.." && pwd)"
source "$ROOT_DIR/scripts/env.sh"

echo "== USB =="
lsusb -d 0483:374b || true

echo
echo "== ST-Link =="
st-info --probe

echo
echo "== OpenOCD attach =="
openocd -f interface/stlink.cfg -f target/stm32h7x.cfg \
  -c "adapter speed 1000" \
  -c "init" \
  -c "targets" \
  -c "shutdown"
