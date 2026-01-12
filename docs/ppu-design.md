# PPU Design (Ricoh 2C02)

## Memory Layout (VRAM Bus)
The PPU addresses 16KB ($0000-$3FFF):
- `$0000-$0FFF`: Pattern Table 0 (CHR-ROM/RAM)
- `$1000-$1FFF`: Pattern Table 1 (CHR-ROM/RAM)
- `$2000-$23FF`: Nametable 0
- `$2400-$27FF`: Nametable 1
- `$2800-$2BFF`: Mirror of Nametable 0 (usually)
- `$2C00-$2FFF`: Mirror of Nametable 1 (usually)
- `$3F00-$3F1F`: Palette RAM (Internal to PPU)

## Detailed Register Map

### PPUCTRL ($2000) - Write Only
| Bit | Description |
|---|---|
| 0-1 | **NT**: Base nametable address (0=$2000, 1=$2400, 2=$2800, 3=$2C00) |
| 2 | **I**: VRAM Increment (0=add 1, 1=add 32) |
| 3 | **S**: Sprite Pattern Table Address (0=$0000, 1=$1000) |
| 4 | **B**: Background Pattern Table Address (0=$0000, 1=$1000) |
| 5 | **H**: Sprite Size (0=8x8, 1=8x16) |
| 6 | **P**: PPU Master/Slave (Unused) |
| 7 | **V**: NMI Enable (Generate NMI at VBlank start) |

### PPUMASK ($2001) - Write Only
| Bit | Description |
|---|---|
| 0 | **Gr**: Grayscale (0=Normal, 1=Mono) |
| 1 | **m**: Show background in leftmost 8 pixels |
| 2 | **M**: Show sprites in leftmost 8 pixels |
| 3 | **b**: Show background |
| 4 | **s**: Show sprites |
| 5 | **R**: Emphasize Red |
| 6 | **G**: Emphasize Green |
| 7 | **B**: Emphasize Blue |

### PPUSTATUS ($2002) - Read Only
Reading this register resets the Write Latch (w).
| Bit | Description |
|---|---|
| 0-4 | Unused (Open Bus) |
| 5 | **O**: Sprite Overflow (Set if >8 sprites on scanline) |
| 6 | **S**: Sprite 0 Hit (Set when opaque sprite 0 pixel hits opaque BG pixel) |
| 7 | **V**: VBlank (Set at scanline 241, cleared on Read or Pre-render) |

### OAMADDR ($2003) & OAMDATA ($2004)
- Address register pointing to one of 256 OAM bytes.
- Data register to write data to that address.

### PPUSCROLL ($2005) - Write x2
- First write: X Scroll
- Second write: Y Scroll

### PPUADDR ($2006) - Write x2
- First write: High byte of VRAM address.
- Second write: Low byte of VRAM address.

### PPUDATA ($2007) - Read/Write
- Reads/Writes data at current VRAM address, then increments address by 1 or 32.
- Reads are buffered (dummy read first) except for Palette RAM.

## Rendering Cycle
1. **Pre-Render (-1)**: Clear status flags. Fetch data.
2. **Visible (0-239)**: 
   - Every 8 cycles: Fetch NT, AT, Pattern Low, Pattern High.
   - Shift registers output pixels based on fine X scroll.
3. **Post-Render (240)**: Idle.
4. **VBlank (241-260)**: Set VBlank Flag at dot 1 of line 241.

## OAM Layout (4 Bytes per Sprite)
1. **Y**: Y Position - 1.
2. **T**: Tile Index.
3. **A**: Attributes (Palette, Priority, Flip X/Y).
4. **X**: X Position.
