#!/bin/bash

# Configuration
BIN="./bin/luna"
TEST_DIR="test"
TEMP_OUT="/tmp/luna_test.out"
TEMP_CASE="/tmp/luna_test_case.lu"

# Colors for pretty output
GREEN='\033[0;32m'
RED='\033[0;31m'
YELLOW='\033[1;33m'
NC='\033[0m' # No Color

# Check if binary exists
if [ ! -f "$BIN" ]; then
    echo -e "${RED}Error: Binary '$BIN' not found.${NC}"
    echo "Run 'make' first."
    exit 1
fi

echo "========================================"
echo "  Running Luna Test Suite (Bash)"
echo "========================================"

FAILED=0
PASSED=0

run_luna() {
    local src="$1"
    if [[ "$(basename "$src")" == test_gc_* ]]; then
        env LUNA_GC_STRESS=1 LUNA_GC_VERIFY=1 "$BIN" "$src"
    else
        "$BIN" "$src"
    fi
}

run_split_golden_test() {
    local src="$1"
    local expect_file="$2"
    local base_name
    base_name=$(basename "$src")

    local failed=0
    local case_name
    mapfile -t case_names < <(grep '^# CASE ' "$src" | sed 's/^# CASE //')

    for case_name in "${case_names[@]}"; do
        awk -v case_name="$case_name" '
            $0 == "# CASE " case_name { capture=1; next }
            /^# CASE / && capture { exit }
            capture { print }
        ' "$src" > "$TEMP_CASE"

        run_luna "$TEMP_CASE" > "$TEMP_OUT" 2>&1

        awk -v case_name="$case_name" '
            $0 == "# CASE " case_name { capture=1; next }
            /^# CASE / && capture { exit }
            capture { print }
        ' "$expect_file" > "${TEMP_OUT}.expect"

        diff -q --strip-trailing-cr "$TEMP_OUT" "${TEMP_OUT}.expect" > /dev/null
        if [ $? -ne 0 ]; then
            echo -e "${RED}[FAIL] ${base_name}:${case_name} (Output Mismatch)${NC}"
            echo -e "${YELLOW}Expected:${NC}"
            cat "${TEMP_OUT}.expect"
            echo -e "${YELLOW}Actual:${NC}"
            cat "$TEMP_OUT"
            ((FAILED++))
            failed=1
        fi
    done

    rm -f "${TEMP_OUT}.expect" "$TEMP_CASE"

    if [ $failed -eq 0 ]; then
        echo -e "${GREEN}[PASS] $base_name (Split Golden)${NC}"
        ((PASSED++))
    fi
}

# Loop through all .lu files
for src in "$TEST_DIR"/*.lu; do
    # Check if glob found nothing
    [ -e "$src" ] || continue

    base_name=$(basename "$src")
    expect_file="${src%.lu}.expect"

    if [ "$base_name" = "test_unsafe_errors.lu" ]; then
        run_split_golden_test "$src" "$expect_file"
        continue
    fi

    # --- CASE 1: Golden File Test (Compare Output) ---
    if [ -f "$expect_file" ]; then
        # Run and save output to temp file
        run_luna "$src" > "$TEMP_OUT" 2>&1
        
        # Compare output (ignoring trailing whitespace issues)
        diff -q --strip-trailing-cr "$TEMP_OUT" "$expect_file" > /dev/null
        
        if [ $? -eq 0 ]; then
            echo -e "${GREEN}[PASS] $base_name (Output Match)${NC}"
            ((PASSED++))
        else
            echo -e "${RED}[FAIL] $base_name (Output Mismatch)${NC}"
            echo -e "${YELLOW}Expected:${NC}"
            cat "$expect_file"
            echo -e "${YELLOW}Actual:${NC}"
            cat "$TEMP_OUT"
            ((FAILED++))
        fi

    # --- CASE 2: Assertion Test (Check Exit Code) ---
    else
        # Run and capture stderr just in case it fails
        run_luna "$src" > /dev/null 2> "$TEMP_OUT"
        EXIT_CODE=$?

        if [ $EXIT_CODE -eq 0 ]; then
            echo -e "${GREEN}[PASS] $base_name (Assertion)${NC}"
            ((PASSED++))
        else
            echo -e "${RED}[FAIL] $base_name (Assertion Failed)${NC}"
            cat "$TEMP_OUT"
            ((FAILED++))
        fi
    fi
done

# Cleanup
rm -f "$TEMP_OUT"
rm -f "$TEMP_CASE" "${TEMP_OUT}.expect"

echo "========================================"
echo "Summary: $PASSED Passed, $FAILED Failed"

if [ $FAILED -gt 0 ]; then
    exit 1
else
    exit 0
fi
