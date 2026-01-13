#!/bin/bash

# Master script to run public NES test ROMs
# Usage: ./scripts/test_public_roms.sh [filter]

EMULATOR="./build/NEStupid.app/Contents/MacOS/NEStupid"
BASE_DIR="tests/roms/nes-test-roms"
RESULTS_FILE="test_results.txt"
PASSED=0
FAILED=0

# Ensure build exists
if [ ! -f "$EMULATOR" ]; then
    echo "Emulator binary not found at $EMULATOR"
    echo "Please build the project first."
    exit 1
fi

echo "Running Public Test ROMs..." > "$RESULTS_FILE"
echo "Date: $(date)" >> "$RESULTS_FILE"
echo "----------------------------------------" >> "$RESULTS_FILE"

run_test_dir() {
    local dir="$1"
    local name="$2"
    
    echo "--> Running $name Tests ($dir)..."
    echo "--> Section: $name" >> "$RESULTS_FILE"
    
    for rom in "$dir"/*.nes; do
        # Skip if no files found
        [ -e "$rom" ] || continue
        
        test_name=$(basename "$rom")
        echo -n "  Testing $test_name... "
        
        # Run emulator in headless mode
        # Capture output. We look for "Passed" text or Status 00.
        # Ideally, tests output "Passed" or we check $6000 status.
        
        output=$( "$EMULATOR" "$rom" --headless 2>&1 )
        
        # Check for explicitly known success indicators
        # 1. "$6000 Status: 00" (Standard Blargg Pass)
        # 2. "Passed" (Text output)
        
        if echo "$output" | grep -q "Status: 00"; then
            echo "PASS"
            echo "  [PASS] $test_name" >> "$RESULTS_FILE"
            ((PASSED++))
        elif echo "$output" | grep -i -q "Passed"; then
            echo "PASS"
            echo "  [PASS] $test_name" >> "$RESULTS_FILE"
            ((PASSED++))
        else
            # Try to extract failure reason (last status or text)
            status=$(echo "$output" | grep "Status:" | tail -n 1)
            failure=$(echo "$output" | tail -n 5) # Capture last few lines
            
             echo "FAIL"
             echo "  [FAIL] $test_name" >> "$RESULTS_FILE"
             echo "    Output: $status" >> "$RESULTS_FILE"
             # echo "    Details: $failure" >> "$RESULTS_FILE"
             ((FAILED++))
        fi
    done
}

# 1. Instruction Tests (v5 singles)
run_test_dir "$BASE_DIR/instr_test-v5/rom_singles" "CPU Instructions"

# 2. MMC3 Tests (Blargg)
run_test_dir "$BASE_DIR/mmc3_test" "MMC3"

# 3. Sprite Hit (Checking availability)
run_test_dir "$BASE_DIR/sprite_hit_tests_2005.10.05" "Sprite Hit"

# 4. MMC1 Tests
# Note: mmc1_a12.nes is a single file in the directory
echo "--> Running MMC1 Tests (tests/roms/nes-test-roms/MMC1_A12)..."
echo "--> Section: MMC1" >> "$RESULTS_FILE"
mmc1_rom="$BASE_DIR/MMC1_A12/mmc1_a12.nes"
if [ -f "$mmc1_rom" ]; then
    # Manually call the inner logic or wrap it. 
    # For simplicity, just run it here since run_test_dir iterates *
    
    test_name=$(basename "$mmc1_rom")
    echo -n "  Testing $test_name... "
    output=$( "$EMULATOR" "$mmc1_rom" --headless 2>&1 )
    
    if echo "$output" | grep -q "Status: 00" || echo "$output" | grep -i -q "Passed"; then
        echo "PASS"
        echo "  [PASS] $test_name" >> "$RESULTS_FILE"
        ((PASSED++))
    else
        status=$(echo "$output" | grep "Status:" | tail -n 1)
        echo "FAIL"
        echo "  [FAIL] $test_name" >> "$RESULTS_FILE"
        echo "    Output: $status" >> "$RESULTS_FILE"
        ((FAILED++))
    fi
fi

# 5. MMC3 IRQ Tests
run_test_dir "$BASE_DIR/mmc3_irq_tests" "MMC3 IRQ"


echo ""
echo "========================================"
echo "Summary: $PASSED Passed, $FAILED Failed"
echo "See $RESULTS_FILE for details."
echo "========================================"
