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
