#!/usr/bin/env bash
set -euo pipefail

ROOT_DIR="$(cd "$(dirname "${BASH_SOURCE[0]}")/.." && pwd)"
source "$ROOT_DIR/scripts/env.sh"

if [[ $# -lt 1 || $# -gt 2 ]]; then
  echo "usage: $0 firmware.bin [address]" >&2
  exit 2
fi

BIN="$1"
ADDR="${2:-0x08000000}"

if [[ ! -f "$BIN" ]]; then
  echo "not found: $BIN" >&2
  exit 2
fi

st-flash write "$BIN" "$ADDR"
