# Implementation Roadmap

## Phase -1: Planning and Research (Current)
- [x] Create core design documentation (`docs/`).
- [x] Establish project structure.

## Phase 0: Skeleton & Build
- [ ] Set up CMake build system.
- [ ] Create simple `main.c` with SDL2 window creation.
- [ ] Implement `Info.plist` and icon bundling.
- [ ] **Goal**: App launches, shows window, logs "Hello NES" to `logs/`, quits on Request.

## Phase 1: ROM Loader & Basics
- [ ] Implement `rom.c`: Load `.nes` files, parse header.
- [ ] Implement `memory.c`: Basic CPU/PPU memory maps (read/write stubs).
- [ ] Implement `gui.c`: File loader dialog or CLI arg.
- [ ] **Goal**: Pass `test_rom_loading.py`.

## Phase 2: CPU Core
- [ ] Implement `cpu.c`: Registers, Fetch-Decode-Execute loop.
- [ ] Implement Address Modes and Base Opcodes.
- [ ] Run `nestest.nes` in headless mode, compare logs.
- [ ] **Goal**: Pass `nestest.nes` (automated log comparison).

## Phase 3: PPU & Rendering
- [ ] Implement `ppu.c`: Registers `$2000-$2007`.
- [ ] Implement Scanline Logic & VBlank NMI.
- [ ] Implement Background Rendering (Nametables).
- [ ] **Goal**: Display `Donkey Kong` or `Super Mario Bros` title screen (backgrounds).

## Phase 4: Input, Sprites & Polish
- [ ] Implement `input.c`: SDL event handling -> Keypad registers `$4016`.
- [ ] Implement Sprite Rendering (OAM).
- [ ] Implement Sprite 0 Hit.
- [ ] **Goal**: Playable standard games (Mapper 0).
