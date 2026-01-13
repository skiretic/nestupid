# Changelog

All notable changes to **NEStupid** will be documented in this file.

## [Unreleased] - 2026-01-12

### Added
- **Native macOS GUI**: Replaced custom overlay with native Cocoa UI elements.
- **Menu Integration**: Application now uses the system menu bar.
- **Open ROM Dialog**: Added **File -> Open ROM...** (Cmd+O) to load games via a native file picker.
- **Key Bindings**: Added **NEStupid -> Key Bindings...** (Cmd+K) to configure controller inputs.
    - Supports rebinding interaction for all standard keys and modifiers (Shift, Ctrl, Alt, Cmd).
- **Mapper 1 (MMC1)**: Implemented support for MMC1 mapper (used in *The Legend of Zelda*, *Metroid*).
- **APU Core**: Implemented underlying logic for Audio Processing Unit.
    - **Pulse Channels**: Square wave generation with sweep and envelope control.
    - **Triangle Channel**: Linear counter and timer logic.
    - **Noise Channel**: LFSR-based noise generation (random and periodic).
    - **Frame Counter**: Standard 4-step and 5-step sequencer for clocking envelopes and counters.

### Changed
- **Project Structure**: Completely reorganized `src/` directory into modular subdirectories (`apu`, `cpu`, `ppu`, `rom`, `input`, `gui`, `memory`).
- **Build System**: Updated `CMakeLists.txt` to support the new directory structure.
- **Main Loop**: Refactored `main.c` to support dynamic ROM loading/unloading without restarting the app.
- **Documentation**: Updated README, Tasklist, Roadmap, and Architecture docs to reflect new features.

### Fixed
- **Sprite 0 Hit**: Fixed critical PPU timing bug that caused *Super Mario Bros.* to freeze at the title screen.
- **Shift Key Binding**: Fixed an issue where modifier keys (like Left Shift) could not be bound to controller inputs.
- **Visual Glitches**: Fixed background transparency and palette rendering issues.

### Added (MMC3 Update)
- **Mapper 4 (MMC3)**: Full implementation including PRG/CHR banking and IRQ support. Verified with *Super Mario Bros. 3*.
- **PPU Clipping**: Implemented Left 8-Pixel column masking (controlled by `PPUMASK`), essential for correct display of scrolling games.

### Fixed (MMC3 Update)
- **MMC3 Mirroring**: Corrected initialization logic to handle inverted header bits properly.
- **MMC3 IRQ Timing**: Tuned A12 filter threshold to `6` to prevent spurious IRQ firings and fix status bar jitters.
- **Map Screen Glitches**: Resolved "Vertical Bar" artifacts on SMB3 map screen via proper clipping implementation.
### Added (Audio Update)
- **DMC Channel**: Full implementation of the Delta Modulation Channel support, enabling sample playback in games like *Super Mario Bros. 3*.
- **High-Pass Filter**: Added a first-order HPF to remove DC offset and improve audio quality.
- **Improved Audio Sync**: Implemented sub-sample timing accumulation for accurate pitch and synchronization.

### Fixed (Audio Update)
- **Audio Popping**: Resolved loud popping artifacts by implementing the HPF and properly centering the audio signal.
- **Race Conditions**: Replaced the audio ring buffer with a thread-safe, lock-free implementation to prevent buffer corruption between emulation and audio threads.

### Added (Mapper 2 & Testing Update)
- **Mapper 2 (UxROM)**: Implemented support for UxROM mapper, verifying compatibility with *Contra*.
- **Headless Mode**: Added `--headless` argument to run the emulator without GUI/Audio initialization, optimizing for automated test environments.
- **Debug Console Support**: Added interception for writes to the `$6000` test register in `memory.c`, enabling terminal-based string output from test ROMs.

### Fixed (Mapper 2 & Testing Update)
- **CPU Opcode 0x3E**: Implemented missing `ROL abs,X` instruction, resolving crashes in comprehensive instruction tests.
- **Automated Test Pipeline**: Updated `run_all_tests.sh` to use the `cpu_reset` breakpoint instead of `gui_init`, ensuring ROM data is fully loaded before verification.
- **Test Variable Scope**: Fixed `test_rom_loading.py` to correctly locate the `current_rom` global via the LLDB target scope.
