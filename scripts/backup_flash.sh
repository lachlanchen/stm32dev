#!/usr/bin/env bash
set -euo pipefail

ROOT_DIR="$(cd "$(dirname "${BASH_SOURCE[0]}")/.." && pwd)"
source "$ROOT_DIR/scripts/env.sh"

mkdir -p "$ROOT_DIR/backups"
OUT="${1:-$ROOT_DIR/backups/stm32h7_flash_$(date +%Y%m%d_%H%M%S).bin}"

st-flash read "$OUT" 0x08000000 0x200000
sha256sum "$OUT"
