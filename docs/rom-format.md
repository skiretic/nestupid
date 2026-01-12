# iNES / NES 2.0 ROM Format

## Header Layout (16 Bytes)

Standard iNES header structure:

| Byte | Description |
|---|---|
| 0-3 | Constant `$4E $45 $53 $1A` ("NES" + EOF) |
| 4 | Size of PRG ROM in 16KB units |
| 5 | Size of CHR ROM in 8KB units (0 implies CHR RAM) |
| 6 | Flags 6: Lower Mapper Nibble, Mirroring, Battery, Trainer |
| 7 | Flags 7: Upper Mapper Nibble, NES 2.0 Magic Bits |
| 8-15| Unused / Reserved (in iNES 1.0) or Extended (in NES 2.0) |

## NES 2.0 Detection
We check specific bits in Byte 7:
- `(Header[7] & 0x0C) == 0x08` implies NES 2.0 format.
- If detected, Bytes 8-15 provide extended info (Submappers, larger ROM sizes, PRG-RAM sizes).
- If not detected, we assume standard iNES 1.0 (mask bytes 8-15 to 0 if necessary for compatibility).

## Supported Mappers

### Mapper 0 (NROM)
- **PRG ROM**: 16KB or 32KB.
- **CHR ROM**: 8KB.
- **Mirroring**: Horizontal or Vertical (fixed in h/w).
- **Registers**: None.
- **Mapping**:
  - CPU `$8000-$BFFF`: First 16KB of PRG ROM.
  - CPU `$C000-$FFFF`: Last 16KB of PRG ROM (if 16KB total, mirror of first).
  - PPU `$0000-$1FFF`: 8KB CHR ROM.

## Loading Rules
1. Read Header.
2. Validate Signature ("NES" + 0x1A).
3. Determine Mapper ID.
4. If Trainer present (Flag 6 bit 2), skip 512 bytes.
5. Read PRG ROM into buffer.
6. Read CHR ROM into buffer (if Size > 0).
7. Allocate CHR RAM if Size == 0.
8. Initialize CPU Memory Map and PPU Memory Map pointers to these buffers.
