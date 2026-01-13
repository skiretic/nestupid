# Implementation Roadmap

## Phase -1: Planning and Research (Current)
- [x] Create core design documentation (`docs/`).
- [x] Establish project structure.

## Phase 0: Skeleton & Build
- [x] Set up CMake build system.
- [x] Create simple `main.c` with SDL2 window creation.
- [x] Implement `Info.plist` and icon bundling.
- [x] **Goal**: App launches, shows window, logs "Hello NES" to `logs/`, quits on Request.

## Phase 1: ROM Loader & Basics
- [x] Implement `rom.c`: Load `.nes` files, parse header.
- [x] Implement `memory.c`: Basic CPU/PPU memory maps (read/write stubs).
- [x] Implement `gui.c`: File loader dialog or CLI arg.
- [x] **Goal**: Pass `test_rom_loading.py`.

## Phase 2: CPU Core
- [x] Implement `cpu.c`: Registers, Fetch-Decode-Execute loop.
- [x] Implement Address Modes and Base Opcodes.
- [x] Run `nestest.nes` in headless mode, compare logs.
- [x] **Goal**: Pass `nestest.nes` (automated log comparison).

## Phase 3: PPU & Rendering
- [x] Implement `ppu.c`: Registers `$2000-$2007`.
- [x] Implement Scanline Logic & VBlank NMI.
- [x] Implement Background Rendering (Nametables).
- [x] **Goal**: Display `Donkey Kong` or `Super Mario Bros` title screen (backgrounds).

## Phase 4: Input, Sprites & Polish
- [x] Implement `input.c`: SDL event handling -> Keypad registers `$4016`.
- [x] Implement Sprite Rendering (OAM).
- [x] Implement Sprite 0 Hit.
- [x] **Goal**: Playable standard games (Mapper 0).
- [x] **GUI**: Native Key Binding & ROM Loading (added per request).

## Phase 5: Mappers & Compatibility
- [x] Implement Mapper Interface (virtual functions).
- [x] Implement Mapper 1 (MMC1) - *Zelda, Metroid*.
- [x] Implement Mapper 2 (UxROM) - *Contra*.
- [ ] Implement Mapper 3 (CNROM).
- [x] Implement Mapper 4 (MMC3) - *SMB3, Kirby*.

## Phase 6: Audio (APU)
- [x] Pulse Channels 1 & 2.
- [x] Triangle Channel.
- [x] Noise Channel.
- [x] DMC Channel.
- [x] Review Mixer & SDL Audio Callback.
