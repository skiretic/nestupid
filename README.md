# NEStupid

**NEStupid** is a lightweight, cycle-stepped Nintendo Entertainment System (NES) emulator written in C. It relies on SDL2 for windowing, rendering, and input handling.

This project aims to approximate the inner workings of the NES hardware, including the Ricoh 2A03 CPU (6502 variant) and the Ricoh 2C02 PPU (Picture Processing Unit), with a focus on running classic titles like *Super Mario Bros*.

<div align="center">
  <img src="docs/screenshot.png" alt="NEStupid Screenshot" width="600" />
</div>

## Features

*   **CPU**: Complete 6502 instruction set emulation (official opcodes).
*   **PPU**: Cycle-based rendering pipeline with background scrolling, nametable mirroring, and 8x8/8x16 sprite support.
*   **Memory**: Standard memory map implementation including internal RAM, PPU registers, and cartridge mapping (NROM).
*   **Graphics**: SDL2-based framebuffer rendering.
*   **Input**: Keyboard-mapped controller input support.

## Prerequisites

To build NEStupid, you need:

*   **C Compiler** (GCC or Clang)
*   **CMake** (3.10+)
*   **SDL2 Development Libraries**

### macOS (Homebrew)
```bash
brew install cmake sdl2
```

### Ubuntu/Debian
```bash
sudo apt-get install build-essential cmake libsdl2-dev
```

## Building

Clone the repository and build using CMake:

```bash
mkdir build
cd build
cmake ..
make
```

This will generate the `NEStupid` executable (or `NEStupid.app` bundle on macOS).

## Usage

Run the emulator by passing a valid `.nes` ROM file as an argument.

```bash
# From the build directory
./NEStupid.app/Contents/MacOS/NEStupid ../smb.nes
```

*Note: The emulator currently supports **NROM** mapper games (e.g., Super Mario Bros, Donkey Kong). Advanced mappers (MMC1, MMC3) are in development.*

## Controls

The emulator currently maps **Controller 1** to the keyboard:

| NES Button | Keyboard Key      |
|------------|-------------------|
| **A**      | `Z`               |
| **B**      | `X`               |
| **Start**  | `Enter` (Return)  |
| **Select** | `Shift` (L or R)  |
| **D-Pad**  | `Arrow Keys`      |

## Development Status

- [x] **CPU**: Cycle-accurate instruction execution.
- [x] **PPU**: Background fetching, scrolling, and Sprite 0 Hit detection.
- [x] **Sprites**: Secondary OAM evaluation and 8-sprite limit enforcement.
- [ ] **APU**: Audio Processing Unit (Not yet implemented).
- [ ] **Mappers**: Basic NROM is functional. MMC support is planned.

## License

This project is provided for educational purposes. All rights to the authentic NES hardware and games belong to Nintendo.
