# Handoff Document - NEStupid

## Project Status: **Playable Alpha**
The emulator successfully runs *Super Mario Bros.* (Mapper 0) with full graphics (backgrounds, sprites) and input support. The game is playable from start to finish.

## Completed Features
- **CPU**: Cycle-stepped 6502 core with official opcodes.
- **PPU**: Full rendering pipeline (Fetch/Render), Scroll logic, Sprites (8x8/8x16), and minimal Sprite 0 Hit detection.
- **Input**: SDL2 keyboard mapping to Controller 1.
- **System**: Basic NROM memory mapping.
- **Mappers**: MMC1 (Mapper 1) implemented (Support for *Zelda*, *Metroid*).

## Known Issues
- **Timing**: The "Cycle Step" is a hardcoded 1 CPU : 3 PPU loop. It is accurate enough for NTSC but lacks sub-cycle precision or PAL support.
- **APU**: Audio is completely missing.
- **Mappers**: Mapper 0 (NROM) works. Mapper 1 (MMC1) implemented and *The Legend of Zelda* is playable. Other mappers (like MMC3) are not yet implemented.

## Next Steps (Recommended)

### 1. Phase 5: Mappers (Continued)
- **Goal**: Run *Super Mario Bros. 3* (MMC3).
- **Files**: `src/mapper.c`.
- **Task**: Implement Mapper 3 (CNROM) or Mapper 4 (MMC3). MMC3 requires Scanline IRQ counters.

### 2. Phase 6: Audio (APU) (Priority: Medium)
Implement the 2A03 APU.
- **Goal**: Sound output in *Super Mario Bros*.
- **Files**: `src/apu.c`, `src/apu.h`.
- **Task**: Implement Pulse 1, Pulse 2, Triangle, Noise, and DMC channels. Hook into SDL Audio callback in `gui.c` or main loop.

### 3. Refactoring (Priority: Low)
- **GUI**: Add a file picker dialog (using native file dialogs or Dear ImGui) instead of CLI arguments.
- **Tests**: Integrate `blargg_nes_cpu_test` ROMs for rigorous verification.

## Useful Commands
- **Build**: `cd build && cmake .. && make`
- **Run**: `./build/NEStupid.app/Contents/MacOS/NEStupid ../smb.nes`
