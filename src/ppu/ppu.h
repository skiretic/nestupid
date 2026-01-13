#ifndef PPU_H
#define PPU_H

#include "rom.h"
#include <stdbool.h>
#include <stdint.h>

// PPU Registers
#define PPU_CTRL_NT_ADDR 0x03  // Nametable select (0-3)
#define PPU_CTRL_VRAM_INC 0x04 // 0: +1, 1: +32
#define PPU_CTRL_SPR_PT 0x08   // Sprite Pattern Table (0: $0000, 1: $1000)
#define PPU_CTRL_BG_PT 0x10    // Background Pattern Table (0: $0000, 1: $1000)
#define PPU_CTRL_SPR_SIZE 0x20 // 0: 8x8, 1: 8x16
#define PPU_CTRL_NMI 0x80      // NMI Enable

#define PPU_MASK_GRAYSCALE 0x01
#define PPU_MASK_SHOW_BG_LEFT 0x02
#define PPU_MASK_SHOW_SPR_LEFT 0x04
#define PPU_MASK_SHOW_BG 0x08
#define PPU_MASK_SHOW_SPR 0x10
#define PPU_MASK_EMPH_RED 0x20
#define PPU_MASK_EMPH_GREEN 0x40
#define PPU_MASK_EMPH_BLUE 0x80

#define PPU_STATUS_VBLANK 0x80
#define PPU_STATUS_SPR0_HIT 0x40
#define PPU_STATUS_SPR_OVF 0x20

typedef struct {
  // VRAM (16KB total address space mapped here mostly via cartridge or internal
  // NT) Physical VRAM usually 2KB or 4KB for Nametables. CHR ROM/RAM is
  // normally mapped from Cartridge. We will use pointers to map different
  // regions.
  uint8_t display_buffer[256 * 240]; // Output pixel buffer (indices or RGB?)
                                     // Let's use indices.

  // OAM
  uint8_t oam[256];
  uint8_t oam_addr;

  // Palette RAM ($3F00 - $3F1F)
  uint8_t palette[32];

  // Registers and Latches
  uint8_t ctrl;   // $2000
  uint8_t mask;   // $2001
  uint8_t status; // $2002

  // Loopy Registers (Scroll/Addr)
  // v: Current VRAM address (15 bit)
  // t: Temporary VRAM address (15 bit)
  // x: Fine X scroll (3 bit)
  // w: First/Second write toggle (1 bit)
  uint16_t v;
  uint16_t t;
  uint8_t fine_x;
  uint8_t w;

  // Background Rendering State
  uint8_t bg_next_tile_id;
  uint8_t bg_next_tile_attrib;
  uint8_t bg_next_tile_lsb;
  uint8_t bg_next_tile_msb;

  uint16_t bg_shifter_pattern_lo;
  uint16_t bg_shifter_pattern_hi;
  uint16_t bg_shifter_attrib_lo;
  uint16_t bg_shifter_attrib_hi;

  // Sprite Rendering State
  uint8_t secondary_oam[32]; // 8 sprites * 4 bytes
  uint8_t sprite_count;      // Number of sprites found on next scanline
  uint8_t
      render_sprite_count; // Number of sprites to render on current scanline

  // Shifters for 8 sprites (Shift Registers)
  // Using 8-bit shifts as sprites are 8-wide logic
  uint8_t sprite_shifter_pattern_lo[8];
  uint8_t sprite_shifter_pattern_hi[8];

  // Counters and Attributes for 8 sprites
  uint8_t sprite_attrib[8];    // Latched attributes for current line
  uint8_t sprite_x_counter[8]; // X position counters

  // Sprite 0 Detection
  bool sprite_zero_hit_possible;    // True if Sprite 0 is in Secondary OAM
  bool render_sprite_zero_possible; // Latched for rendering
  bool sprite_zero_being_rendered;  // True if Sprite 0 is being rendered

  uint8_t data_buffer; // PPUDATA read buffer

  // Rendering counters
  int scanline; // 0-261
  int dot;      // 0-340
  bool frame_complete;

  // Internal Mirrors
  uint8_t nametables[2048]; // 2KB internal (Vertical/Horizontal mirroring)
  ROM *rom;                 // Access to CHR ROM
} PPU_State;

void ppu_init(ROM *rom);
void ppu_reset(void);
void ppu_step(void);

// Register Access
uint8_t ppu_read_reg(uint16_t addr);
void ppu_write_reg(uint16_t addr, uint8_t val);
void ppu_dma(uint8_t *page_data);

// Debug/Display
// Debug/Display
const uint8_t *ppu_get_framebuffer(void);
const uint8_t *ppu_get_palette(void);
bool ppu_is_frame_complete(void);
void ppu_clear_frame_complete(void);
// Debug Access
int ppu_get_scanline(void);
const PPU_State *ppu_get_state(void);

#endif // PPU_H
