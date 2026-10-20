#!/bin/sh
# Usage: compiler/lunac.sh input.lu output_binary
# Compiles a Luna program to C with compiler/lunac.lu, then builds it with gcc.
# LUNAC_LUNA overrides the program that runs lunac.lu (default: bin/luna).
set -e
ROOT=$(cd "$(dirname "$0")/.." && pwd)
IN=$1
OUT=$2
if [ -z "$IN" ] || [ -z "$OUT" ]; then
    echo "usage: compiler/lunac.sh input.lu output_binary" >&2
    exit 2
fi
LUNA=${LUNAC_LUNA:-$ROOT/bin/luna}
C_FILE="$OUT.c"
"$LUNA" "$ROOT/compiler/lunac.lu" "$IN" "$C_FILE" "$(pwd)"
RT="$ROOT/compiler/runtime"
gcc -std=gnu11 -O2 -w -I"$RT" "$C_FILE" "$RT/luna_rt.c" "$RT/luna_rt_data.c" "$RT/luna_gc.c" \
    -L"$ROOT/lib" -lluna_data_rt -lluna_memory_rt -lm -lpthread -ldl -o "$OUT"
