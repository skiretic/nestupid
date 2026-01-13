#include "memory.h"
#include "apu.h"
#include "cpu.h"
#include "input.h"
#include "mapper.h"
#include "ppu.h"
#include <stdio.h>
#include <string.h>

// Global memory state
static uint8_t internal_ram[2048]; // 2KB Internal RAM ($0000-$07FF)
static ROM *current_rom = NULL;

void memory_init(ROM *rom) {
  current_rom = rom;
  memset(internal_ram, 0, sizeof(internal_ram));
  mapper_init(rom);
  printf("Memory System Initialized\n");
}

uint8_t bus_read(uint16_t addr) {
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

  // $4020 - $FFFF: Cartridge Space (PRG ROM/RAM + Mapper Registers)
  return mapper_cpu_read(addr);
}

void bus_write(uint16_t addr, uint8_t val) {
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
      buffer[i] = bus_read(src_base + i);
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

  // Cartridge Space - Writes to ROM/RAM/Registers handled by Mapper
  if (addr == 0x6000) {
    // Debug output for Blargg's tests
    if ((val >= 0x20 && val <= 0x7E) || val == '\n' || val == '\r' ||
        val == '\t') {
      printf("%c", val);      // Print ASCII char directly
    } else if (val != 0x80) { // Skip "Running" heartbeat to avoid clutter
      printf("\n[$6000 Status: %02X]\n", val);
    }
    fflush(stdout);
  }

  // $6004+: Text output from Blargg's tests
  if (addr >= 0x6004 && addr < 0x7000) {
    if ((val >= 0x20 && val <= 0x7E) || val == '\n' || val == '\r' ||
        val == '\t') {
      printf("%c", val);
      fflush(stdout);
    } else if (val == 0) {
      // Null terminator - end of text
      printf("\n");
      fflush(stdout);
    }
  }

  if (addr >= 0x4020) {
    mapper_cpu_write(addr, val);
  }
}
