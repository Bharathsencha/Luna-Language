#!/bin/sh
# Builds the runtime and prints what a program must link against.
#
# The runtime is written in Luna (compiler/runtime/*.lu). It is compiled with `luna --native`
# and clang into C/luna_rt_native.o, rebuilt only when a runtime file or a compiler file is
# newer than the object.
#
# Usage: compiler/build_runtime.sh     prints the link inputs, one line
# LUNAC=path names the compiler to use (default: bin/luna).
ROOT=$(cd "$(dirname "$0")/.." && pwd)
RT="$ROOT/compiler/runtime"
OUT="$ROOT/C"
OBJ="$OUT/luna_rt_native.o"
LL="$OUT/luna_rt_native.ll"
LUNAC=${LUNAC:-$ROOT/bin/luna}

mkdir -p "$OUT"
NATIVE=$(ls "$RT"/*.lu | sort)
if [ ! -f "$OBJ" ] || [ -n "$(find $NATIVE "$ROOT"/compiler/*.lu -newer "$OBJ" 2>/dev/null)" ]; then
    if [ ! -x "$LUNAC" ]; then
        echo "[ERROR] the compiler $LUNAC is not built; run make" >&2
        exit 1
    fi
    rm -f "$LL"
    "$LUNAC" --native $NATIVE -o "$LL" >&2 || exit 1
    clang -O2 -w -c "$LL" -o "$OBJ" >&2 || exit 1
fi
echo "$OBJ"
