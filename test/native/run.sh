#!/bin/sh
# Builds test/native/rules_unit.lu together with compiler/runtime/luna_rt_rules.lu in the
# native dialect and runs it. Exits non-zero when a check fails.
ROOT=$(cd "$(dirname "$0")/../.." && pwd)
TMP=$(mktemp -d)
trap 'rm -rf "$TMP"' EXIT
LUNAC=${LUNAC:-$ROOT/bin/luna}
[ -x "$LUNAC" ] || { echo "[ERROR] $LUNAC is not built; run make"; exit 1; }
"$LUNAC" --native "$ROOT/compiler/runtime/luna_rt_rules.lu" "$ROOT/test/native/rules_unit.lu" -o "$TMP/rules_unit.ll" || exit 1
clang -O1 -w "$TMP/rules_unit.ll" -o "$TMP/rules_unit" || exit 1
"$TMP/rules_unit"
