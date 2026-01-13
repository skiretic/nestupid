# Mapper 2 (UxROM) Implementation

## Overview
Implemented generic support for Mapper 2 (UxROM), used by games such as *Contra*, *Castlevania*, and *Mega Man*.

## Technical Details
- **PRG ROM Banking**:
  - CPU `$8000-$BFFF`: Switchable 16KB bank.
  - CPU `$C000-$FFFF`: Fixed to the last 16KB bank.
- **Bank Selection**:
  - Writing to any address in `$8000-$FFFF` selects the lower 16KB bank.
- **Mirroring**:
  - Hardwired (Vertical or Horizontal) as per ROM header.
- **CHR Memory**:
  - Uses CHR-RAM (8KB).

- **Launch Contra:** Use `File -> Open` or drag-and-drop safely.
- **Check Logs:** Verify "Mapper 2 Initialized".
- **Visuals:** Confirm title screen renders correctly (blue background vs black?).

## CPU and Code-Level Verification

### CPU Opcode Fix
During comprehensive testing, the automated test suite identified a missing opcode: `0x3E` (ROL abs,X). This was causing crashes in some test ROMs.
- **Fix:** Implemented `0x3E` case in `cpu.c` using `op_rol_m(addr_absx())`.
- **Verification:** Ran `tests/roms/nes-test-roms/instr_test-v5/official_only.nes` successfully.

### Automated Testing Fixes
The automated testing script `scripts/run_all_tests.sh` was updated to correct a timing issue where tests attempted to verify ROM data before loading completed.
- **Change:** Updated LLDB breakpoints from `gui_init` to `cpu_reset` to ensure tests run after ROM initialization.
- **Status:** All automated tests (ROM Loading, CPU Exec, PPU Regs) are now passing.

### APU Debug Output
To assist with APU debugging in headless mode, character output to the `$6000` test register is now captured and printed to the terminal.
- **Benefit:** Allows reading "PASS" or "FAIL" messages from Blargg's test ROMs directly in the terminal output.

## Summary of Session
1.  **Implemented Mapper 2 (UxROM)** for *Contra* support.
2.  **Fixed Automated Test Pipeline** (broken due to timing/variable name issues).
3.  **Corrected CPU Logic** (missing `ROL abs,X`).
4.  **Enhanced Headless Mode** and debug logging for testing efficiency.
5.  **Synchronized All Project Documentation.**
