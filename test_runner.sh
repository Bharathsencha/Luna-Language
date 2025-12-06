#!/bin/bash

# Configuration
BIN="./bin/luna"
TEST_DIR="test"
TEMP_OUT="/tmp/luna_test.out"

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

# Loop through all .lu files
for src in "$TEST_DIR"/*.lu; do
    # Check if glob found nothing
    [ -e "$src" ] || continue

    base_name=$(basename "$src")
    expect_file="${src%.lu}.expect"

    # --- CASE 1: Golden File Test (Compare Output) ---
    if [ -f "$expect_file" ]; then
        # Run and save output to temp file
        $BIN "$src" > "$TEMP_OUT" 2>&1
        
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
        $BIN "$src" > /dev/null 2> "$TEMP_OUT"
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

echo "========================================"
echo "Summary: $PASSED Passed, $FAILED Failed"

if [ $FAILED -gt 0 ]; then
    exit 1
else
    exit 0
fi