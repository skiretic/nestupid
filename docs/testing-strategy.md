# Testing Strategy

## Philosophy
We prefer automated, headless verification using known-good test ROMs over manual "play testing". This catches regressions early and verifies strict architectural behavior.

## Test Harness: LLDB
We use LLDB's Python scripting bridge to automate testing.
The script:
1. Launches NEStupid with a specific ROM.
2. Sets breakpoints at critical functions (e.g., `cpu_step`, or specific addresses).
3. Steps the emulator.
4. Inspects memory/registers to verify state.

## Headless Mode verifiable
To facilitate automated testing without requiring a display or audio, NEStupid supports a `--headless` flag. This bypasses SDL2 initialization and GUI loops, allowing tests to run in CI/environment without windowing support.
```bash
./NEStupid ../test.nes --headless
```

## Test ROMs & Mapping

All ROMs must be loaded from `tests/roms/nes-test-roms/`.

| Component | Test Suite | Specific ROM | Verification Condition |
|---|---|---|---|
| **CPU** | `instr_test-v5` | `official_only.nes` | Result code at `$6000` == 0. |
| **Interrupts** | `cpu_interrupts_v2` | `cpu_interrupts.nes` | Checks NMI/IRQ timing. |
| **Logic** | `nestest.nes` | `nestest.nes` | Compare execution log vs golden log. |
| **PPU** | `blargg_ppu_tests` | `palette_ram.nes` | Memory at range checks out. |

## LLDB Script Naming
Scripts in `tests/lldb/` should overlap with phase milestones:
- `test_rom_loading.py`: Loads a valid ROM, checks header parsing.
- `test_cpu_official.py`: Runs `instr_test-v5`, waits for completion, checks `$6000`.
- `test_ppu_vbl.py`: Checks VBlank flag setting at Scanline 241.

## Continuous Integration
A simple shell script `scripts/run_all_tests.sh` will iterate through all python test scripts and report pass/fail.
