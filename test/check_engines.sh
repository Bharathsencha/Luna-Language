#!/bin/bash
# Holds the legacy interpreter (LUNA_USE_INTERPRETER=1) to the same data and
# memory rules as the bytecode VM:
#   1. test/test_rules.lu must pass on the interpreter.
#   2. Every case in test/test_unsafe_errors.lu must report the same first
#      error message as the VM's golden output.

BIN="./bin/luna"
SRC="test/test_unsafe_errors.lu"
EXPECT="test/test_unsafe_errors.expect"
CASE_FILE="/tmp/luna_engine_case.lu"
FAILED=0

GREEN='\033[0;32m'
RED='\033[0;31m'
NC='\033[0m'

if ! LUNA_USE_INTERPRETER=1 "$BIN" test/test_rules.lu > /dev/null 2>&1; then
    echo -e "${RED}[FAIL] test_rules.lu on the interpreter${NC}"
    FAILED=1
fi

# First message line of an error report: the indented line after "... Error in".
first_message() {
    awk '/ Error / && / at line / { getline; print; exit }'
}

mapfile -t cases < <(grep '^# CASE ' "$SRC" | sed 's/^# CASE //')
for name in "${cases[@]}"; do
    awk -v c="$name" '$0 == "# CASE " c { f = 1; next } /^# CASE / && f { exit } f { print }' \
        "$SRC" > "$CASE_FILE"
    want=$(awk -v c="$name" '$0 == "# CASE " c { f = 1; next } /^# CASE / && f { exit } f { print }' \
        "$EXPECT" | first_message)
    got=$(LUNA_USE_INTERPRETER=1 "$BIN" "$CASE_FILE" 2>&1 | first_message)
    if [ "$want" != "$got" ]; then
        echo -e "${RED}[FAIL] interpreter: ${name}${NC}"
        echo "  expected: $want"
        echo "  actual:   $got"
        FAILED=1
    fi
done
rm -f "$CASE_FILE"

if [ $FAILED -eq 0 ]; then
    echo -e "${GREEN}[PASS] interpreter follows the same rules (${#cases[@]} error cases + test_rules.lu)${NC}"
fi
exit $FAILED
