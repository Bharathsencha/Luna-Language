#!/bin/sh
# Every program is compiled to a native binary and run, and stdout, stderr and the exit code
# must equal the recorded files in test/expected/. Files made of "# CASE <name>" sections run
# each case on its own.
# Usage: compiler/run_tests.sh [file.lu ...]   (default: test/*.lu)
# RECORD=1 compiler/run_tests.sh   writes test/expected/ from the compiled programs. Read the
#                                  diff (git diff test/expected) before committing it.
ROOT=$(cd "$(dirname "$0")/.." && pwd)
cd "$ROOT" || exit 1
TMP=$(mktemp -d)
trap 'rm -rf "$TMP"' EXIT
[ $# -eq 0 ] && set -- test/*.lu
RT=compiler/runtime
EXT=ll; CC="clang"
RTSRC=$("$ROOT/compiler/build_runtime.sh") || exit 1
CASE_FILE="$TMP/case.lu"
EXPECT=test/expected
mkdir -p "$EXPECT"
LUNAC="$ROOT/bin/luna"
[ -x "$LUNAC" ] || { echo "[ERROR] $LUNAC is not built; run make"; exit 1; }
pass=0; fail=0; skip=0

# check_one <source> <label>: compiles and runs <source> on both engines.
check_one() {
    src=$1; label=$2; name=bin_$$
    if ! $LUNAC "$src" "$TMP/$name.$EXT" "$ROOT" > "$TMP/cerr" 2>&1; then
        if grep -q "not supported by lunac" "$TMP/cerr"; then
            skip=$((skip + 1)); echo "[SKIP] $label"; return
        fi
        fail=$((fail + 1)); echo "[FAIL] $label (lunac)"; head -3 "$TMP/cerr"; return
    fi
    if ! $CC -O1 -w "$TMP/$name.$EXT" $RTSRC \
         -lm -lpthread -ldl -o "$TMP/$name" 2> "$TMP/gerr"; then
        fail=$((fail + 1)); echo "[FAIL] $label (clang)"; head -5 "$TMP/gerr"; return
    fi
    key=$(printf '%s' "$label" | sed 's/[^A-Za-z0-9._-]/_/g')
    timeout 60 "$TMP/$name" > "$TMP/c.out" 2> "$TMP/c.err" < /dev/null; cc=$?
    # Timings differ between runs.
    sed -E 's/[0-9.e+-]+ seconds/N seconds/; s/^[0-9]+\.[0-9]{5,}$/N/' "$TMP/c.out" > "$TMP/c.out.n"
    mv "$TMP/c.out.n" "$TMP/c.out"
    # Error messages name the source file; the recorded outputs use this fixed name.
    sed -i "s|$CASE_FILE|/tmp/luna_test_case.lu|g" "$TMP/c.out" "$TMP/c.err"
    if [ -n "$RECORD" ]; then
        rm -f "$EXPECT/$key".*
        cp "$TMP/c.out" "$EXPECT/$key.out"; cp "$TMP/c.err" "$EXPECT/$key.err"; echo "$cc" > "$EXPECT/$key.rc"
        pass=$((pass + 1)); echo "[REC ] $label"
        return
    fi
    if [ -f "$EXPECT/$key.skip" ]; then
        skip=$((skip + 1)); echo "[SKIP] $label"; return
    fi
    if [ ! -f "$EXPECT/$key.rc" ]; then
        fail=$((fail + 1)); echo "[FAIL] $label (no recorded output; run RECORD=1 compiler/run_tests.sh)"; return
    fi
    want=$(cat "$EXPECT/$key.rc")
    if [ "$want" -eq "$cc" ] && cmp -s "$EXPECT/$key.out" "$TMP/c.out" && cmp -s "$EXPECT/$key.err" "$TMP/c.err"; then
        pass=$((pass + 1)); echo "[PASS] $label"
    else
        fail=$((fail + 1)); echo "[FAIL] $label (exit want=$want got=$cc)"
        diff "$EXPECT/$key.out" "$TMP/c.out" | head -6
        diff "$EXPECT/$key.err" "$TMP/c.err" | head -12
    fi
}

# check_golden <source>: compiled output must equal <source minus .lu>.golden.
check_golden() {
    src=$1; name=gold_$$
    gold="${src%.lu}.golden"
    if ! $LUNAC "$src" "$TMP/$name.$EXT" "$ROOT" > "$TMP/cerr" 2>&1 ||
       ! $CC -O1 -w "$TMP/$name.$EXT" $RTSRC \
            -lm -lpthread -ldl -o "$TMP/$name" 2> "$TMP/gerr"; then
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
