# Mapper Implementation Design

## Overview
The emulator currently supports the following iNES mappers:
- **Mapper 0**: NROM
- **Mapper 1**: MMC1
- **Mapper 2**: UxROM
- **Mapper 4**: MMC3

## Mapper 0 (NROM)
- **PRG**: 16KB/32KB fixed.
- **CHR**: 8KB fixed (or RAM).
- **Mirroring**: Fixed H/V from header.

## Mapper 2 (UxROM)
- **PRG Banking**:
  - $8000-$BFFF: 16KB switchable bank.
  - $C000-$FFFF: 16KB bank fixed to the last 16KB of PRG ROM.
- **CHR**: 8KB fixed (typically uses CHR-RAM).
- **Mirroring**: Fixed H/V from header.
- **Registers**: A write to any address in the range $8000-$FFFF sets the 16KB switchable bank at $8000 (Bank Register = Bits 0-3 roughly, depending on total PRG size).

## Mapper 1 (MMC1)
- **PRG Banking**: 16KB/32KB switchable.
- **CHR Banking**: 4KB/8KB switchable.
- **Mirroring**: Dynamic (One-Screen, H, V).
- **Implementation**: Shift register based (5 writes to set).

## Mapper 4 (MMC3)
- **PRG Banking**:
  - Two 8KB banks at $8000 and $A000.
  - Two fixed/swappable 8KB banks at $C000 (fixed to second-to-last) and $E000 (fixed to last).
- **CHR Banking**:
  - Two 2KB banks at $0000-$0FFF.
  - Four 1KB banks at $1000-$1FFF.
  - *Modes*: Can invert the address ranges (0x0000<->0x1000).
- **Mirroring**: Dynamic H/V controlled by $A000 writes.
  - **Note**: MMC3 mirroring value (0=V, 1=H) is inverted relative to iNES header bit.
- **IRQ**:
  - Scanline-based IRQ using PPU A12 line monitoring.
  - **A12 Filter**: The PPU address line A12 is monitored in `mapper_ppu_tick`. A transition from 0->1 triggers the counter, provided A12 was low for at least **6** PPU ticks (filtering spurious reads).
  - **IRQ Firing**: When counter reaches 0 and IRQs are enabled, the CPU IRQ line is asserted.
