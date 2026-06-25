#!/usr/bin/env bash
set -euo pipefail

ROOT_DIR="$(cd "$(dirname "${BASH_SOURCE[0]}")/.." && pwd)"
source "$ROOT_DIR/scripts/env.sh"

if [[ $# -ne 1 ]]; then
  echo "usage: $0 path/to/flash-backup.bin" >&2
  exit 2
fi

BIN="$1"
if [[ ! -f "$BIN" ]]; then
  echo "not found: $BIN" >&2
  exit 2
fi

st-flash write "$BIN" 0x08000000
