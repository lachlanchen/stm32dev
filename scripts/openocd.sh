#!/usr/bin/env bash
set -euo pipefail

ROOT_DIR="$(cd "$(dirname "${BASH_SOURCE[0]}")/.." && pwd)"
source "$ROOT_DIR/scripts/env.sh"

exec openocd -f interface/stlink.cfg -f target/stm32h7x.cfg -c "adapter speed 1000"
