#include "ppu.h"
#include "cpu.h"
#include "mapper.h"
#include <stdio.h>
#include <string.h>

static PPU_State ppu;

void ppu_init(ROM *rom) {
  memset(&ppu, 0, sizeof(PPU_State));
  ppu.rom = rom;
  printf("PPU Initialized\n");
}

void ppu_reset(void) {
  ppu.ctrl = 0;
  ppu.mask = 0;
  ppu.status = 0;
  ppu.oam_addr = 0;
  ppu.scanline = 0;
  ppu.dot = 0;
  ppu.v = 0;
  ppu.t = 0;
  ppu.w = 0;
  ppu.fine_x = 0;
  ppu.frame_complete = false;
  memset(ppu.oam, 0, sizeof(ppu.oam));
  memset(ppu.secondary_oam, 0xFF, sizeof(ppu.secondary_oam));
  ppu.sprite_count = 0;
  ppu.sprite_zero_hit_possible = false;
  memset(ppu.palette, 0, sizeof(ppu.palette));
  printf("PPU Reset\n");
}

const PPU_State *ppu_get_state(void) { return &ppu; }

// Helpers for VRAM increments
static void ppu_increment_vaddr(void) {
  if (ppu.ctrl & PPU_CTRL_VRAM_INC) {
    ppu.v += 32;
  } else {
    ppu.v += 1;
  }
}

// Read/Write access to VRAM

// Helper for Nametable Mirroring
static uint16_t ppu_mirror_nametable_addr(uint16_t addr) {
  addr &= 0x0FFF;
  uint8_t mirroring = mapper_get_mirroring();

  if (mirroring == MIRRORING_VERTICAL) {
    if (addr & 0x0400)
      return 1024 + (addr & 0x03FF);
    else
      return (addr & 0x03FF);
  } else if (mirroring == MIRRORING_HORIZONTAL) {
    if (addr & 0x0800)
      return 1024 + (addr & 0x03FF);
    else
      return (addr & 0x03FF);
  } else if (mirroring == MIRRORING_ONE_SCREEN_LO) {
    return addr & 0x03FF;
  } else if (mirroring == MIRRORING_ONE_SCREEN_HI) {
    return 1024 + (addr & 0x03FF);
  }
  return addr & 0x03FF; // Default fail-safe
}

static uint8_t ppu_vram_read(uint16_t addr) {
  addr &= 0x3FFF;

  mapper_ppu_tick(addr); // Snooping

  if (addr < 0x2000) {
    return mapper_ppu_read(addr);
  }

  if (addr < 0x3F00) {
    return ppu.nametables[ppu_mirror_nametable_addr(addr)];
  }

  if (addr >= 0x3F00) {
    addr &= 0x001F;
    if (addr == 0x10)
      addr = 0x00;
    if (addr == 0x14)
      addr = 0x04;
    if (addr == 0x18)
      addr = 0x08;
    if (addr == 0x1C)
      addr = 0x0C;
    return ppu.palette[addr];
  }
  return 0;
}

static void ppu_vram_write(uint16_t addr, uint8_t val) {
  addr &= 0x3FFF;

  mapper_ppu_tick(addr); // Snooping

  if (addr < 0x2000) {
    mapper_ppu_write(addr, val);
  } else if (addr < 0x3F00) {
    ppu.nametables[ppu_mirror_nametable_addr(addr)] = val;
  } else if (addr >= 0x3F00) {
    addr &= 0x001F;
    if (addr == 0x10)
      addr = 0x00;
    if (addr == 0x14)
      addr = 0x04;
    if (addr == 0x18)
      addr = 0x08;
    if (addr == 0x1C)
      addr = 0x0C;
    ppu.palette[addr] = val;
  }
}

uint8_t ppu_read_reg(uint16_t addr) {
  switch (addr & 0x0007) {
  case 2: // PPUSTATUS
  {
    uint8_t status = ppu.status;
    ppu.status &= ~PPU_STATUS_VBLANK;
    ppu.w = 0;
    return status;
  }
  case 4: // OAMDATA
    return ppu.oam[ppu.oam_addr];
  case 7: // PPUDATA
  {
    uint8_t val = ppu.data_buffer;
    uint16_t addr = ppu.v & 0x3FFF;
    ppu.data_buffer = ppu_vram_read(addr);
    if (addr >= 0x3F00) {
      val = ppu.data_buffer;
      // When reading palettes, the buffer is loaded with the mirrored VRAM data
      ppu.data_buffer = ppu.nametables[ppu_mirror_nametable_addr(addr)];
    }
    ppu_increment_vaddr();
    return val;
  }
  }
  return 0;
}

void ppu_write_reg(uint16_t addr, uint8_t val) {
  switch (addr & 0x0007) {
  case 0: // PPUCTRL
    // printf("PPUCTRL Write: %02X\n", val);
    ppu.ctrl = val;
    ppu.t = (ppu.t & 0xF3FF) | ((val & 0x03) << 10);
    break;
  case 1: // PPUMASK
    // printf("PPUMASK Write: %02X (BG:%d SPR:%d)\n", val,
    //        (val & PPU_MASK_SHOW_BG) ? 1 : 0, (val & PPU_MASK_SHOW_SPR) ? 1 :
    //        0);
    ppu.mask = val;
    break;
  case 3: // OAMADDR
    ppu.oam_addr = val;
    break;
  case 4: // OAMDATA
    ppu.oam[ppu.oam_addr++] = val;
    break;
  case 5: // PPUSCROLL
    if (ppu.w == 0) {
      ppu.fine_x = val & 0x07;
      ppu.t = (ppu.t & 0xFFE0) | (val >> 3);
      ppu.w = 1;
    } else {
      ppu.t = (ppu.t & 0x8FFF) | ((val & 0x07) << 12);
      ppu.t = (ppu.t & 0xFC1F) | ((val & 0xF8) << 2);
      ppu.w = 0;
    }
    // printf("PPUSCROLL Write: %02X\n", val);
    break;
  case 6: // PPUADDR
    // printf("PPUADDR Write: %02X (w=%d)\n", val, ppu.w);
    if (ppu.w == 0) {
      ppu.t = (ppu.t & 0x80FF) | ((val & 0x3F) << 8);
      ppu.t &= 0x3FFF;
      ppu.w = 1;
    } else {
      ppu.t = (ppu.t & 0xFF00) | val;
      ppu.v = ppu.t;
      ppu.w = 0;
    }
    break;
  case 7: // PPUDATA
    ppu_vram_write(ppu.v, val);
    ppu_increment_vaddr();
    break;
  }
}

void ppu_dma(uint8_t *page_data) {
  printf("DMA Start OAM Addr: %02X\n", ppu.oam_addr);
  for (int i = 0; i < 256; i++) {
    ppu.oam[ppu.oam_addr++] = page_data[i];
  }
  // CPU stalls for 513 or 514 cycles usually. Not implemented yet.
}

// --- Rendering Helpers ---

static void ppu_increment_scroll_x(void) {
  if ((ppu.v & 0x001F) == 31) { // Coarse X = 31
    ppu.v &= ~0x001F;           // Coarse X = 0
    ppu.v ^= 0x0400;            // Switch Horizontal Nametable
  } else {
    ppu.v += 1;
  }
}

static void ppu_increment_scroll_y(void) {
  if ((ppu.v & 0x7000) != 0x7000) { // If Fine Y < 7
    ppu.v += 0x1000;                // Increment Fine Y
  } else {
    ppu.v &= ~0x7000;              // Fine Y = 0
    int y = (ppu.v & 0x03E0) >> 5; // Coarse Y
    if (y == 29) {
      y = 0;
      ppu.v ^= 0x0800; // Switch Vertical Nametable
    } else if (y == 31) {
      y = 0; // In case of weird Y=31
    } else {
      y += 1;
    }
    ppu.v = (ppu.v & ~0x03E0) | (y << 5);
  }
}

static void ppu_transfer_address_x(void) {
  // v: .....F.. ...EDCBA = t: .....F.. ...EDCBA
  ppu.v = (ppu.v & 0xFBE0) | (ppu.t & 0x041F);
}

static void ppu_transfer_address_y(void) {
  // v: IHGF.EDC BA...... = t: IHGF.EDC BA......
  ppu.v = (ppu.v & 0x841F) | (ppu.t & 0x7BE0);
}

static void ppu_load_bg_shifters(void) {
  ppu.bg_shifter_pattern_lo =
      (ppu.bg_shifter_pattern_lo & 0xFF00) | ppu.bg_next_tile_lsb;
  ppu.bg_shifter_pattern_hi =
      (ppu.bg_shifter_pattern_hi & 0xFF00) | ppu.bg_next_tile_msb;

  // Attributes are expanded to 8 bits (0 or 0xFF) for easy mixing
  ppu.bg_shifter_attrib_lo = (ppu.bg_shifter_attrib_lo & 0xFF00) |
                             ((ppu.bg_next_tile_attrib & 0x01) ? 0xFF : 0x00);
  ppu.bg_shifter_attrib_hi = (ppu.bg_shifter_attrib_hi & 0xFF00) |
                             ((ppu.bg_next_tile_attrib & 0x02) ? 0xFF : 0x00);
}

static void ppu_update_shifters(void) {
  if (ppu.mask & PPU_MASK_SHOW_BG) {
    ppu.bg_shifter_pattern_lo <<= 1;
    ppu.bg_shifter_pattern_hi <<= 1;
    ppu.bg_shifter_attrib_lo <<= 1;
    ppu.bg_shifter_attrib_hi <<= 1;
  }
}

void ppu_step(void) {
  bool rendering_enabled = (ppu.mask & (PPU_MASK_SHOW_BG | PPU_MASK_SHOW_SPR));

  // Visible Scanlines (0-239)
  if (ppu.scanline <= 239) {

    // --- Sprite Evaluation (Cycles 1-256) ---
    // Standard Behavior: Only happens if rendering is enabled
    // Simplified Logic:
    // 1. Clear Secondary OAM (Cycles 1-64)
    if (ppu.dot == 1) {
      // In hardware this takes 64 cycles. Instant here.
      memset(ppu.secondary_oam, 0xFF, sizeof(ppu.secondary_oam));
      ppu.sprite_count = 0;
      ppu.sprite_zero_hit_possible = false;
    }

    // 2. Sprite Evaluation (Cycles 65-256)
    if (rendering_enabled && ppu.dot == 257) {
      // Scan OAM
      int count = 0;
      uint8_t sprite_size = (ppu.ctrl & PPU_CTRL_SPR_SIZE) ? 16 : 8;

      for (int i = 0; i < 64; i++) {
        uint8_t y = ppu.oam[i * 4];
        // Sprite data is Y+1 logic usually, but OAM storage is Y-1? No, Y
        // is byte 0. Visible on scanlines Y+1 to Y+8/16. So if scanline >=
        // y && scanline < y + size
        int diff = ppu.scanline - y;
        if (diff >= 0 && diff < sprite_size) {
          if (count < 8) {
            // Found a sprite!
            if (i == 0) {
              ppu.sprite_zero_hit_possible = true;
              uint8_t tile = ppu.oam[i * 4 + 1];
              uint8_t x = ppu.oam[i * 4 + 3];
              // printf("Sprite 0 Found: Y=%d, Tile=%02X, X=%d\n", y, tile, x);
            }

            // Copy 4 bytes to Secondary OAM
            memcpy(&ppu.secondary_oam[count * 4], &ppu.oam[i * 4], 4);
            count++;
          } else {
            // Sprite Overflow
            ppu.status |= PPU_STATUS_SPR_OVF;
            break; // In hardware there's a bug, but we can just break for
                   // now behavior
          }
        }
      }
      ppu.sprite_count = count;
    }

    // 3. Sprite Fetching (Cycles 257-320)
    if (rendering_enabled && ppu.dot == 320) {
      // Iterate found sprites
      uint8_t sprite_size = (ppu.ctrl & PPU_CTRL_SPR_SIZE) ? 16 : 8;
      uint16_t sprite_pattern_table =
          (ppu.ctrl & PPU_CTRL_SPR_PT) ? 0x1000 : 0x0000;

      for (int i = 0; i < 8; i++) {
        uint8_t y = ppu.secondary_oam[i * 4 + 0];
        uint8_t tile = ppu.secondary_oam[i * 4 + 1];
        uint8_t attr = ppu.secondary_oam[i * 4 + 2];
        uint8_t x = ppu.secondary_oam[i * 4 + 3];

        // Only process valid sprites for shifters, but ALWAYS fetch
        bool valid_sprite = (i < ppu.sprite_count);

        if (valid_sprite) {
          ppu.sprite_x_counter[i] = x;
          ppu.sprite_attrib[i] = attr;
        }

        // Calculate Pattern Address
        uint16_t addr_lo = 0, addr_hi = 0;

        // Y-flip logic
        uint8_t row = ppu.scanline - y;
        if (attr & 0x80) { // Flip Y
          row = sprite_size - 1 - row;
        }

        if (sprite_size == 8) {
          // 8x8 Mode
          uint16_t pt_base = sprite_pattern_table;
          addr_lo = pt_base + (tile << 4) + row;
          addr_hi = addr_lo + 8;
        } else {
          // 8x16 Mode: Tile LSB determines pattern table
          uint16_t pt_base = (tile & 0x01) ? 0x1000 : 0x0000;
          tile &= 0xFE;  // Top tile index
          if (row < 8) { // Top half
            addr_lo = pt_base + (tile << 4) + row;
          } else { // Bottom half
            addr_lo = pt_base + ((tile + 1) << 4) + (row - 8);
          }
          addr_hi = addr_lo + 8;
        }

        // Read Pattern Data (This drives MMC3 IRQ!)
        uint8_t pat_lo = ppu_vram_read(addr_lo);
        uint8_t pat_hi = ppu_vram_read(addr_hi);

        // Horizontal Flip Logic (Flip X)
        if (attr & 0x40 && valid_sprite) {
          // Reverse bits
          uint8_t r_lo = 0, r_hi = 0;
          for (int b = 0; b < 8; b++) {
            if (pat_lo & (1 << b))
              r_lo |= (0x80 >> b);
            if (pat_hi & (1 << b))
              r_hi |= (0x80 >> b);
          }
          pat_lo = r_lo;
          pat_hi = r_hi;
        }

        if (valid_sprite) {
          ppu.sprite_shifter_pattern_lo[i] = pat_lo;
          ppu.sprite_shifter_pattern_hi[i] = pat_hi;
        }
      }

      // Latch sprite count for next line rendering
      ppu.render_sprite_count = ppu.sprite_count;
      ppu.render_sprite_zero_possible = ppu.sprite_zero_hit_possible;
    }
  }

  // Visible Scanlines (0-239) or Pre-render (261)
  if (ppu.scanline <= 239 || ppu.scanline == 261) {

    // Background Cycle Loop (Fetches every 8 cycles)
    if (ppu.scanline == 261 && ppu.dot == 1) {
      // Clear VBlank
      ppu.status &=
          ~(PPU_STATUS_VBLANK | PPU_STATUS_SPR0_HIT | PPU_STATUS_SPR_OVF);
    }

    // Cycle 1-256 (Visible) + 321-336 (Prefetch)
    if (rendering_enabled && ((ppu.dot >= 1 && ppu.dot <= 256) ||
                              (ppu.dot >= 321 && ppu.dot <= 336))) {

      ppu_update_shifters();

      // 8-step cadence
      switch ((ppu.dot - 1) % 8) {
      case 0: // Load shifters, Fetch NT
        ppu_load_bg_shifters();
        // Fetch NT Byte
        ppu.bg_next_tile_id = ppu_vram_read(0x2000 | (ppu.v & 0x0FFF));
        break;
      case 2: // Fetch AT Byte
      {
        // Attribute Logic is complex: address 23C0 + (v.NN 1111 YYY XXX)
        // v: ...NN.. ...YYYXXX
        // AT: 0x23C0 | (v & 0x0C00) | ((v >> 4) & 0x38) | ((v >> 2) & 0x07)
        uint16_t at_addr = 0x23C0 | (ppu.v & 0x0C00) | ((ppu.v >> 4) & 0x38) |
                           ((ppu.v >> 2) & 0x07);
        ppu.bg_next_tile_attrib = ppu_vram_read(at_addr);

        // Parse appropriate quadrant
        if (ppu.v & 0x40)
          ppu.bg_next_tile_attrib >>= 4; // Bottom
        if (ppu.v & 0x02)
          ppu.bg_next_tile_attrib >>= 2; // Right
        ppu.bg_next_tile_attrib &= 0x03;
      } break;
      case 4: // Fetch Low BG Byte
        // Pattern Table Addr = (Ctrl.4 << 12) + (TileID * 16) + FineY
        {
          uint16_t pt_addr = ((ppu.ctrl & PPU_CTRL_BG_PT) ? 0x1000 : 0x0000) +
                             ((uint16_t)ppu.bg_next_tile_id << 4) +
                             ((ppu.v >> 12) & 0x07);
          ppu.bg_next_tile_lsb = ppu_vram_read(pt_addr);
        }
        break;
      case 6: // Fetch High BG Byte
      {
        uint16_t pt_addr = ((ppu.ctrl & PPU_CTRL_BG_PT) ? 0x1000 : 0x0000) +
                           ((uint16_t)ppu.bg_next_tile_id << 4) +
                           ((ppu.v >> 12) & 0x07) + 8;
        ppu.bg_next_tile_msb = ppu_vram_read(pt_addr);
      } break;
      case 7: // Increment Scroll X
        ppu_increment_scroll_x();
        break;
      }
    }

    // Start of Scanline Log
    if (ppu.dot == 0 && ppu.scanline < 240) {
      static int frame_log_count = 0;
      if (frame_log_count < 2) { // Log first 2 frames only
        // printf("SL:%d v:%04X t:%04X x:%d\n", ppu.scanline, ppu.v, ppu.t,
        //        ppu.fine_x);
      }
    }

    // Cycle 256: Increment Y
    if (rendering_enabled && ppu.dot == 256) {
      ppu_increment_scroll_y();
    }

    // Cycle 257: Reset X (Load from t)
    if (rendering_enabled && ppu.dot == 257) {
      ppu_load_bg_shifters(); // Just in case? Usually done at dot 1? No 257
                              // just resets X
      ppu_transfer_address_x();
    }

    // Cycle 338 or 340? Dummy fetches (skipped for simple implementation)

    // Pre-render Cycle 280-304: Reset Y (Load from t)
    if (rendering_enabled && ppu.scanline == 261 && ppu.dot >= 280 &&
        ppu.dot <= 304) {
      ppu_transfer_address_y();
    }

    // --- Pixel Output (To Framebuffer) ---
    // Only if rendering enabled and visible period
    if (ppu.scanline < 240 && ppu.dot >= 1 && ppu.dot <= 256) {
      // Check if rendering is actually enabled
      if (!(ppu.mask & (PPU_MASK_SHOW_BG | PPU_MASK_SHOW_SPR))) {
        // Rendering disabled - output backdrop color
        int x = ppu.dot - 1;
        int y = ppu.scanline;
        ppu.display_buffer[y * 256 + x] = ppu_vram_read(0x3F00) & 0x3F;
      } else {
        // Rendering enabled - process pixels normally
        uint8_t pixel = 0;
        uint8_t palette = 0;

        if (ppu.mask & PPU_MASK_SHOW_BG) {
          // De-mux shifter bits using fine_x
          // Bit 15 is leftmost. 15 - fine_x is the bit we want?
          // Actually shifters shift LEFT. So MSB is current pixel.
          // We need to pick bit (15 - fine_x)
          uint16_t bit_mux = 0x8000 >> ppu.fine_x;

          uint8_t p0 = (ppu.bg_shifter_pattern_lo & bit_mux) ? 1 : 0;
          uint8_t p1 = (ppu.bg_shifter_pattern_hi & bit_mux) ? 1 : 0;
          pixel = (p1 << 1) | p0; // 0-3

          uint8_t pal0 = (ppu.bg_shifter_attrib_lo & bit_mux) ? 1 : 0;
          uint8_t pal1 = (ppu.bg_shifter_attrib_hi & bit_mux) ? 1 : 0;
          palette = (pal1 << 1) | pal0; // 0-3

          // Left Clipping (BG)
          if ((ppu.mask & PPU_MASK_SHOW_BG_LEFT) == 0) {
            if (ppu.dot <= 8) {
              pixel = 0;
              palette = 0;
            }
          }

          // --- Sprite Pixel Logic ---
          uint8_t sprite_pixel = 0;
          uint8_t sprite_palette = 0;
          bool sprite_priority = false; // 0=Front, 1=Back

          if (ppu.mask & PPU_MASK_SHOW_SPR) {
            // Check all active sprites
            for (int i = 0; i < ppu.render_sprite_count; i++) {
              if (ppu.sprite_x_counter[i] == 0) {
                // Sprite is active at this X
                // Check bit 7 (MSB) of shifter? Sprites shifters don't shift
                // endlessly? Actually sprite pattern is loaded, and as X
                // counter hits 0, we output pixels? Wait, X counter
                // determines START position. Once started, we output 8
                // pixels. My logic: X counter decrements until 0. Then we use
                // shifter? Standard: Counter decrements. If 0, shift out
                // pixel.

                uint8_t pixel_lo =
                    (ppu.sprite_shifter_pattern_lo[i] & 0x80) ? 1 : 0;
                uint8_t pixel_hi =
                    (ppu.sprite_shifter_pattern_hi[i] & 0x80) ? 1 : 0;
                uint8_t sp_pix = (pixel_hi << 1) | pixel_lo;

                if (sp_pix != 0) {
                  // Left Clipping (Sprite)
                  if ((ppu.mask & PPU_MASK_SHOW_SPR_LEFT) == 0) {
                    if (ppu.dot <= 8) {
                      sp_pix = 0;
                    }
                  }

                  if (sp_pix != 0) {
                    if (sprite_pixel ==
                        0) { // First non-transparent sprite wins
                      sprite_pixel = sp_pix;
                      sprite_palette = (ppu.sprite_attrib[i] & 0x03) + 4;
                      sprite_priority =
                          (ppu.sprite_attrib[i] & 0x20) ? true : false;

                      // Sprite 0 Hit
                      if (i == 0 && ppu.render_sprite_zero_possible) {
                        if (pixel != 0 && sp_pix != 0) {
                          // Require BG pixel to be opaque too
                          // Hit not possible if clipped? Already handled by
                          // setting 0.
                          if (ppu.dot != 255) {
                            ppu.status |= PPU_STATUS_SPR0_HIT; // Set flag
                            ppu.sprite_zero_being_rendered =
                                true; // Mark potential hit
                          }
                        }
                      }
                    }
                  }
                }

                // Shift
                ppu.sprite_shifter_pattern_lo[i] <<= 1;
                ppu.sprite_shifter_pattern_hi[i] <<= 1;
              } else {
                ppu.sprite_x_counter[i]--;
              }
            }
          }

          // --- Multiplexing ---
          uint8_t final_pixel = 0;
          uint8_t final_palette = 0;

          if (pixel != 0 && sprite_pixel != 0) {
            // Collision!
            if (ppu.sprite_zero_being_rendered) {
              // Confirm BG pixel is valid for hit
              if (pixel != 0 && ppu.scanline != 255) { // Hit!
                ppu.status |= PPU_STATUS_SPR0_HIT;
              }
            }

            if (sprite_priority) { // Behind BG
              final_pixel = pixel;
              final_palette = palette;
            } else { // In Front of BG
              final_pixel = sprite_pixel;
              final_palette = sprite_palette;
            }
          } else if (sprite_pixel != 0) {
            final_pixel = sprite_pixel;
            final_palette = sprite_palette;
          } else {
            final_pixel = pixel;
            final_palette = palette;
          }

          // Compose final palette index
          // Palette RAM: $3F00 + (Palette * 4) + Pixel
          uint16_t pal_addr = 0x3F00 | (final_palette << 2) | final_pixel;
          if (final_pixel == 0)
            pal_addr = 0x3F00;

          uint8_t color_index = ppu_vram_read(pal_addr) & 0x3F;

          // Write to framebuffer
          // scanline 0-239
          // dot 1-256 -> x 0-255
          int x = ppu.dot - 1;
          int y = ppu.scanline;
          // Ensure bounds (should be safe by checks)
          ppu.display_buffer[y * 256 + x] = color_index;
        }
      }
    }
  }

  // VBlank Set (Moved outside because scanline 241 is not <= 239)
  if (ppu.scanline == 241 && ppu.dot == 1) {
    ppu.status |= PPU_STATUS_VBLANK;
    if (ppu.ctrl & PPU_CTRL_NMI) {
      cpu_nmi();
    }
  }

  // Scanline/Dot counters (ALWAYS Increment)
  ppu.dot++;
  if (ppu.dot > 340) {
    ppu.dot = 0;
    ppu.scanline++;

    if (ppu.scanline > 261) {
      ppu.scanline = 0;
      ppu.frame_complete = true;
    }
  }
}

const uint8_t *ppu_get_framebuffer(void) { return ppu.display_buffer; }

const uint8_t *ppu_get_palette(void) { return ppu.palette; }

bool ppu_is_frame_complete(void) { return ppu.frame_complete; }

void ppu_clear_frame_complete(void) { ppu.frame_complete = false; }

int ppu_get_scanline(void) { return ppu.scanline; }
