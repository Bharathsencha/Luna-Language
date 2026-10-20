#!/bin/sh
# Differential test: every program is run on the bytecode VM and as compiled
# C, and stdout, stderr and the exit code must match. Files made of
# "# CASE <name>" sections run each case on its own, like test_runner.sh.
# Usage: compiler/run_tests.sh [file.lu ...]   (default: test/*.lu)
# LUNAC_BACKEND=llvm tests the LLVM IR backend (built with clang) instead of the C backend.
ROOT=$(cd "$(dirname "$0")/.." && pwd)
cd "$ROOT" || exit 1
TMP=$(mktemp -d)
trap 'rm -rf "$TMP"' EXIT
[ $# -eq 0 ] && set -- test/*.lu
RT=compiler/runtime
EXT=c; CC="gcc -std=gnu11"
if [ "$LUNAC_BACKEND" = "llvm" ]; then EXT=ll; CC="clang"; fi
CASE_FILE=/tmp/luna_test_case.lu
pass=0; fail=0; skip=0

# check_one <source> <label>: compiles and runs <source> on both engines.
check_one() {
    src=$1; label=$2; name=bin_$$
    if ! "$ROOT/bin/luna" compiler/lunac.lu "$src" "$TMP/$name.$EXT" "$ROOT" > "$TMP/cerr" 2>&1; then
        if grep -q "not supported by lunac" "$TMP/cerr"; then
            skip=$((skip + 1)); echo "[SKIP] $label"; return
        fi
        fail=$((fail + 1)); echo "[FAIL] $label (lunac)"; head -3 "$TMP/cerr"; return
    fi
    if ! $CC -O1 -w -I $RT "$TMP/$name.$EXT" $RT/luna_rt.c $RT/luna_rt_data.c $RT/luna_gc.c $RT/luna_rt_ir.c \
         -L lib -lluna_data_rt -lluna_memory_rt -lm -lpthread -ldl -o "$TMP/$name" 2> "$TMP/gerr"; then
        fail=$((fail + 1)); echo "[FAIL] $label (gcc)"; head -5 "$TMP/gerr"; return
    fi
    timeout 60 "$ROOT/bin/luna" "$src" > "$TMP/vm.out" 2> "$TMP/vm.err" < /dev/null; vm=$?
    timeout 60 "$TMP/$name" > "$TMP/c.out" 2> "$TMP/c.err" < /dev/null; cc=$?
    # Timings differ between runs and engines.
    for side in vm c; do
        sed -E 's/[0-9.e+-]+ seconds/N seconds/; s/^[0-9]+\.[0-9]{5,}$/N/' "$TMP/$side.out" > "$TMP/$side.out.n"
        mv "$TMP/$side.out.n" "$TMP/$side.out"
    done
    if [ $vm -eq 0 ] && [ $cc -ne 0 ] && grep -q "is not defined" "$TMP/c.err"; then
        skip=$((skip + 1)); echo "[SKIP] $label (builtin only in the VM)"; return
    fi
    if [ $vm -eq $cc ] && cmp -s "$TMP/vm.out" "$TMP/c.out" && cmp -s "$TMP/vm.err" "$TMP/c.err"; then
        pass=$((pass + 1)); echo "[PASS] $label"
    else
        fail=$((fail + 1)); echo "[FAIL] $label (exit vm=$vm c=$cc)"
        diff "$TMP/vm.out" "$TMP/c.out" | head -6
        diff "$TMP/vm.err" "$TMP/c.err" | head -12
    fi
}

# check_golden <source>: compiled output must equal <source minus .lu>.golden.
check_golden() {
    src=$1; name=gold_$$
    gold="${src%.lu}.golden"
    if ! "$ROOT/bin/luna" compiler/lunac.lu "$src" "$TMP/$name.$EXT" "$ROOT" > "$TMP/cerr" 2>&1 ||
       ! $CC -O1 -w -I $RT "$TMP/$name.$EXT" $RT/luna_rt.c $RT/luna_rt_data.c $RT/luna_gc.c $RT/luna_rt_ir.c \
            -L lib -lluna_data_rt -lluna_memory_rt -lm -lpthread -ldl -o "$TMP/$name" 2> "$TMP/gerr"; then
        fail=$((fail + 1)); echo "[FAIL] $src (build)"; head -3 "$TMP/cerr" "$TMP/gerr"; return
    fi
    "$TMP/$name" > "$TMP/c.out" 2> "$TMP/c.err" < /dev/null
    if cmp -s "$TMP/c.out" "$gold"; then
        pass=$((pass + 1)); echo "[PASS] $src (golden)"
    else
        fail=$((fail + 1)); echo "[FAIL] $src (golden)"; diff "$gold" "$TMP/c.out" | head -6
    fi
}

for f in "$@"; do
    if [ -f "${f%.lu}.golden" ]; then
        check_golden "$f"
    elif grep -q '^# CASE ' "$f"; then
        # Run each case in this shell so the counters survive.
        i=0
        total=$(grep -c '^# CASE ' "$f")
        while [ $i -lt $total ]; do
            i=$((i + 1))
            awk -v idx=$i '
                /^# CASE / { n++; next }
                n == idx { print }
            ' "$f" > "$CASE_FILE"
            label="$f:case $i ($(grep '^# CASE ' "$f" | sed -n "${i}p" | sed 's/^# CASE //'))"
            check_one "$CASE_FILE" "$label"
        done
    else
        check_one "$f" "$f"
    fi
done
rm -f "$CASE_FILE"
echo "passed $pass, failed $fail, skipped $skip"
[ $fail -eq 0 ]
