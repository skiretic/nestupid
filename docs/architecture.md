# NEStupid Architecture

## High-Level System Design

NEStupid implements a standard NES architecture consisting of the following core components connected via simulated buses:

1.  **CPU (Central Processing Unit)**: A modified MOS 6502 processor running the game logic.
2.  **PPU (Picture Processing Unit)**: The Ricoh 2C02 graphics processor responsible for rendering the display.
3.  **Memory (RAM/BUS)**: Manages the 64KB CPU address space and the 14-bit PPU address space, verifying mirrors and handling I/O registers.
4.  **Cartridge (ROM/Mapper)**: Handles ROM data loading and memory mapping logic (Mappers).
5.  **Input**: Handling controller states.
6.  **GUI/Platform Layer**: 
    -   **SDL2**: Windowing, input polling, and framebuffer display.
    -   **Native (macOS)**: Cocoa-based menus, file dialogs, and configuration windows for enhanced usability.

## Execution Loop

The emulator runs in a tight synchronization loop. Since the PPU runs at 3x the frequency of the CPU (NTSC), the main loop is driven by CPU cycles, with the PPU catching up.

```c
while (app.running) {
    // Execute 1 CPU instruction (returns cycles taken)
    uint8_t cycles = cpu_step();
    
    // PPU runs 3x as fast
    for (int i = 0; i < cycles * 3; i++) {
        ppu_step();
    }
    
    // APU step would go here (usually per frame or per cycle)
    // apu_step(cycles);
    
    // Handle specific timing events (NMI, IRQ)
}
```

## Data Flow

- **CPU <-> Memory**: Read/Write operations to specific addresses. 
  - `$0000-$1FFF`: RAM
  - `$2000-$3FFF`: PPU Registers
  - `$4000-$4017`: Input/APU
  - `$8000-$FFFF`: Cartridge PRG-ROM
- **CPU <-> PPU**: Communication via memory mapped registers `$2000-$2007`.
  - CPU writes to `$2000` (PPUCTRL) to set flags.
  - CPU writes to `$2007` (PPUDATA) to send data to VRAM.
  - PPU triggers **NMI** (Non-Maskable Interrupt) on the CPU at start of VBlank.
- **PPU -> Display**: The PPU generates a 256x240 pixel buffer. Once a frame is complete, this buffer is blitted to the SDL texture.

## Subsystem Boundaries

- **`cpu.c`**: Pure instruction execution. Knows nothing about PPU/Input, only calls `bus_read()` and `bus_write()`.
- **`ppu.c`**: Renders pixels to an internal buffer. exposes `ppu_read/write` for CPU register access.
- **`memory.c`**: The "Bus". Dispatches reads/writes to correct components (RAM, PPU, Mapper).
- **`mapper.c`**: Handles Cartridge memory mapping logic. Implements NROM, MMC1, etc., and controls PRG/CHR banking and mirroring.
- **`rom.c`**: Responsible for loading the ROM file and parsing the iNES header.

## Mappers

Mappers are implemented using a unified interface (`mapper.h`) that intercepts CPU and PPU reads/writes to cartridge space:

- **CPU Read/Write**: Addresses `$4020-$FFFF` (Cartridge Space) are routed to `mapper_cpu_read` / `mapper_cpu_write`.
- **PPU Read/Write**: Addresses `$0000-$1FFF` (Pattern Tables) are routed to `mapper_ppu_read` / `mapper_ppu_write`.
- **Mirroring**: PPU Nametable access ($2000-$3EFF) queries `mapper_get_mirroring()` to determine physical address.

Supported Mappers:
- **Mapper 0 (NROM)**: Standard 16KB/32KB PRG, 8KB CHR.
- **Mapper 1 (MMC1)**: Shift-register based banking, dynamic mirroring. (Used by *Zelda*, *Metroid*)
