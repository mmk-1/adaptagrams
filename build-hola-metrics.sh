#!/usr/bin/env bash
set -euo pipefail

ADAPTAGRAMS_ROOT="$(cd "$(dirname "${BASH_SOURCE[0]}")" && pwd)"
BIN_PATH="$ADAPTAGRAMS_ROOT/hola_metrics"

echo "Building HOLA metrics runner..."
echo "  ADAPTAGRAMS_ROOT: $ADAPTAGRAMS_ROOT"

make -C "$ADAPTAGRAMS_ROOT"

echo "Built binary: $BIN_PATH"
