#include "memory.h"
#include "apu.h"
#include "cpu.h"
#include "input.h"
#include "ppu.h"
#include <stdio.h>
#include <string.h>

// Global memory state
static uint8_t internal_ram[2048]; // 2KB Internal RAM ($0000-$07FF)
static ROM *current_rom = NULL;

void memory_init(ROM *rom) {
  current_rom = rom;
  memset(internal_ram, 0, sizeof(internal_ram));
  printf("Memory System Initialized\n");
}

uint8_t cpu_read(uint16_t addr) {
  // $0000 - $1FFF: 2KB Internal RAM (mirrored 4 times)
  if (addr < 0x2000) {
    return internal_ram[addr & 0x07FF];
  }

  // $2000 - $3FFF: PPU Registers (mirrored every 8 bytes)
  if (addr < 0x4000) {
    return ppu_read_reg(addr & 0x2007);
  }

  // $4000 - $4017: APU and I/O Registers
  if (addr < 0x4018) {
    if (addr == 0x4016) {
      return input_read(0);
    }
    if (addr == 0x4017) {
      return input_read(1);
    }

    if (addr == 0x4015) {
      return apu_read_reg(addr);
    }
    return 0;
  }

  // $4018 - $401F: APU Test mode (usually disabled)
  if (addr < 0x4020) {
    return 0;
  }

  // $4020 - $FFFF: Cartridge Space (PRG ROM/RAM)
  if (current_rom) {
    // Simple NROM Mapper Implementation
    // $8000 - $FFFF
    if (addr >= 0x8000) {
      // Calculate offset into PRG ROM
      uint32_t prg_offset = addr - 0x8000;

      // Handle 16KB PRG mirroring (NROM-128)
      if (current_rom->prg_size == 16384) {
        prg_offset &= 0x3FFF; // Mirror upper 16KB to lower 16KB
      }

      // Check bounds just in case
      if (prg_offset < current_rom->prg_size) {
        return current_rom->prg_data[prg_offset];
      }
    }
  }

  return 0; // Open buse
}

void cpu_write(uint16_t addr, uint8_t val) {
  // $0000 - $1FFF: 2KB Internal RAM (mirrored)
  if (addr < 0x2000) {
    internal_ram[addr & 0x07FF] = val;
    return;
  }

  // $2000 - $3FFF: PPU Registers
  if (addr < 0x4000) {
    ppu_write_reg(addr & 0x2007, val);
    return;
  }

  // OAM DMA
  if (addr == 0x4014) {
    // DMA transfer from CPU memory to OAM
    // Source address: val * 0x100
    uint16_t src_base = val << 8;
    uint8_t buffer[256];
    for (int i = 0; i < 256; i++) {
      buffer[i] = cpu_read(src_base + i);
    }
    ppu_dma(buffer);
    printf("OAM DMA Triggered! Page: %02X\n", val);
    printf("OAM Source Dump [00-0F]: ");
    for (int k = 0; k < 16; k++)
      printf("%02X ", buffer[k]);
    printf("\n");
    cpu_stall(513); // Emulate DMA steal cycles
    return;
  }

  // $4000 - $4017: APU and I/O Registers
  if (addr < 0x4018) {
    if (addr == 0x4016) {
      input_write_strobe(val);
      return;
    }

    if ((addr >= 0x4000 && addr <= 0x4013) || addr == 0x4015 ||
        addr == 0x4017) {
      apu_write_reg(addr, val);
      return;
    }
    return;
  }

  // Cartridge Space - Writes to ROM usually ignored in NROM
  // or handled by Mapper (e.g. MMC1/3)
}
