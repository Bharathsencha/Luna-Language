#!/bin/sh
# Differential test of luna_in_luna.lu: each program runs on the bytecode VM
# directly and through the interpreter written in Luna (also on the VM, or as a
# compiled binary when LUNA_IN_LUNA_BIN is set). stdout, stderr and the exit
# code must match. "# CASE" files run case by case.
# Usage: compiler/run_interp_tests.sh [file.lu ...]   (default: test/*.lu)
ROOT=$(cd "$(dirname "$0")/.." && pwd)
cd "$ROOT" || exit 1
TMP=$(mktemp -d)
trap 'rm -rf "$TMP"' EXIT
[ $# -eq 0 ] && set -- test/*.lu
CASE_FILE=/tmp/luna_test_case.lu
LIMIT=${LUNA_TEST_TIMEOUT:-120}
pass=0; fail=0

run_interp() {
    if [ -n "$LUNA_IN_LUNA_BIN" ]; then
        timeout $LIMIT "$LUNA_IN_LUNA_BIN" "$1"
    else
        timeout $LIMIT "$ROOT/bin/luna" "$ROOT/luna_in_luna.lu" "$1"
    fi
}

check_one() {
    src=$1; label=$2
    timeout $LIMIT "$ROOT/bin/luna" "$src" > "$TMP/vm.out" 2> "$TMP/vm.err" < /dev/null; vm=$?
    run_interp "$src" > "$TMP/li.out" 2> "$TMP/li.err" < /dev/null; li=$?
    sed -E -i 's/[0-9.e+-]+ seconds/N seconds/; s/^[0-9]+\.[0-9]{5,}$/N/' "$TMP/vm.out" "$TMP/li.out"
    if [ $vm -eq $li ] && cmp -s "$TMP/vm.out" "$TMP/li.out" && cmp -s "$TMP/vm.err" "$TMP/li.err"; then
        pass=$((pass + 1)); echo "[PASS] $label"
    else
        fail=$((fail + 1)); echo "[FAIL] $label (exit vm=$vm interp=$li)"
        diff "$TMP/vm.out" "$TMP/li.out" | head -6
        diff "$TMP/vm.err" "$TMP/li.err" | head -10
    fi
}

for f in "$@"; do
    if grep -q '^# CASE ' "$f"; then
        total=$(grep -c '^# CASE ' "$f")
        i=0
        while [ $i -lt $total ]; do
            i=$((i + 1))
            awk -v idx=$i '/^# CASE /{n++;next} n==idx{print}' "$f" > "$CASE_FILE"
            check_one "$CASE_FILE" "$f:case $i ($(grep '^# CASE ' "$f" | sed -n "${i}p" | sed 's/^# CASE //'))"
        done
    else
        check_one "$f" "$f"
    fi
done
rm -f "$CASE_FILE"
echo "passed $pass, failed $fail"
[ $fail -eq 0 ]
