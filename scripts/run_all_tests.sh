#!/bin/bash
echo "Running all tests..."

# Build if needed
mkdir -p build
cd build
cmake -DCMAKE_BUILD_TYPE=Debug ..
make
cd ..


# Run LLDB Test (ROM Loading)
echo "Running ROM Loading Test..."
lldb -b -o "command script import tests/lldb/test_rom_loading.py" \
        -o "breakpoint set -n gui_init" \
        -o "process launch -- tests/roms/nes-test-roms/other/nestest.nes" \
        -o "test_rom_loading" \
        -o "quit" \
        ./build/NEStupid.app/Contents/MacOS/NEStupid | grep "SUCCESS"

if [ $? -ne 0 ]; then
    echo "ROM Loading Test Failed"
    exit 1
fi
echo "ROM Loading Test Passed"

# Run LLDB Test (CPU Exec)
echo "Running CPU Exec Test..."
lldb -b -o "command script import tests/lldb/test_cpu_exec.py" \
        -o "breakpoint set -n gui_init" \
        -o "process launch -- tests/roms/nes-test-roms/other/nestest.nes" \
        -o "test_cpu_exec" \
        -o "quit" \
        ./build/NEStupid.app/Contents/MacOS/NEStupid | grep "SUCCESS"

if [ $? -ne 0 ]; then
    echo "CPU Exec Test Failed"
    exit 1
fi
echo "CPU Exec Test Passed"

# Run LLDB Test (PPU Regs)
echo "Running PPU Regs Test..."
lldb -b -o "command script import tests/lldb/test_ppu_regs.py" \
        -o "breakpoint set -n gui_init" \
        -o "process launch -- tests/roms/nes-test-roms/other/nestest.nes" \
        -o "test_ppu_regs" \
        -o "quit" \
        ./build/NEStupid.app/Contents/MacOS/NEStupid | grep "SUCCESS"

if [ $? -ne 0 ]; then
    echo "PPU Regs Test Failed"
    exit 1
fi
echo "PPU Regs Test Passed"

# Run LLDB Test (PPU Loop)
echo "Running PPU Loop Test..."
lldb -b -o "command script import tests/lldb/test_ppu_loop.py" \
        -o "breakpoint set -n gui_init" \
        -o "process launch -- tests/roms/nes-test-roms/other/nestest.nes" \
        -o "test_ppu_loop" \
        -o "quit" \
        ./build/NEStupid.app/Contents/MacOS/NEStupid | grep "SUCCESS"

if [ $? -ne 0 ]; then
    echo "PPU Loop Test Failed"
    exit 1
fi
echo "PPU Loop Test Passed"

# Run LLDB Test (PPU Sprites)
echo "Running PPU Sprites Test..."
lldb -b -o "command script import tests/lldb/test_ppu_sprites.py" \
        -o "breakpoint set -n gui_init" \
        -o "process launch -- tests/roms/nes-test-roms/other/nestest.nes" \
        -o "test_ppu_sprites" \
        -o "quit" \
        ./build/NEStupid.app/Contents/MacOS/NEStupid | grep "SUCCESS"

if [ $? -eq 0 ]; then
    echo "PPU Sprites Test Passed"
    exit 0
else
    echo "PPU Sprites Test Failed"
    exit 1
fi
