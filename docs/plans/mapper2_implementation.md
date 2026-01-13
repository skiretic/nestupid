# Implementation Plan - Mapper 2 (UxROM)

## Goal
Implement Mapper 2 (UxROM) support to enable games like *Contra*, *Castlevania*, and *Mega Man*.

## User Review Required
None. Standard mapper implementation.

## Proposed Changes

### `src/rom/mapper.c`
#### [MODIFY] [mapper.c](file:///Users/anthony/projects/code/nestupid2/src/rom/mapper.c)
- Add `UxROM_State` struct (optional, or just reuse a static int since it only has one register).
- Implement `uxrom_cpu_read` and `uxrom_cpu_write`.
- Implement `uxrom_ppu_read` and `uxrom_ppu_write` (Pass-through to CHR-RAM).
- Integrate into `mapper_init`, `mapper_cpu_read`, etc.

**Logic:**
- **Registers:** Write to `$8000-$FFFF` sets the PRG bank.
- **PRG Memory:**
  - `$8000-$BFFF`: Switchable 16KB Bank (selected by register).
  - `$C000-$FFFF`: Fixed to Last 16KB Bank.
- **CHR Memory:**
  - Uses CHR-RAM (8KB). No banking.

## Verification Plan
### Automated Tests
- Since I don't have a known Mapper 2 test ROM in the repo, I will verify that the code compiles and looks logically correct.
- If the user provides *Contra* later, it should work.

### Manual Verification
- Review code against NESDev Wiki specifications for UxROM.
