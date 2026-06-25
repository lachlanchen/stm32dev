#!/usr/bin/env bash
set -euo pipefail

PORT="${1:-/dev/ttyACM0}"
BAUD="${2:-115200}"

exec picocom -b "$BAUD" "$PORT"
