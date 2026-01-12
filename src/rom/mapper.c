#include "mapper.h"
#include <stdio.h>
#include <string.h>

static ROM *ctx_rom = NULL;
static uint8_t prg_ram[8192]; // 8KB PRG RAM (Battery Backed ideally)

// --- MMC1 State ---
typedef struct {
  uint8_t shift_reg;
  uint8_t shift_count;
  uint8_t control;
  uint8_t chr_bank0;
  uint8_t chr_bank1;
  uint8_t prg_bank;
} MMC1_State;

static MMC1_State mmc1;

// --- Mapper 0 (NROM) Logic ---

static uint8_t nrom_cpu_read(uint16_t addr) {
  // $6000-$7FFF: Family Basic / PRG RAM (Optional in NROM)
  if (addr >= 0x6000 && addr < 0x8000) {
    return prg_ram[addr - 0x6000];
  }

  if (addr >= 0x8000) {
    uint32_t offset = addr - 0x8000;
    if (ctx_rom->prg_size == 16384) {
      offset &= 0x3FFF;
    }
    if (offset < ctx_rom->prg_size) {
      return ctx_rom->prg_data[offset];
    }
  }
  return 0;
}

static void nrom_cpu_write(uint16_t addr, uint8_t val) {
  if (addr >= 0x6000 && addr < 0x8000) {
    prg_ram[addr - 0x6000] = val;
  }
}

static uint8_t nrom_ppu_read(uint16_t addr) {
  if (addr < 0x2000 && ctx_rom->chr_data) {
    return ctx_rom->chr_data[addr % ctx_rom->chr_size];
  }
  return 0;
}

static void nrom_ppu_write(uint16_t addr, uint8_t val) {
  if (addr < 0x2000 && ctx_rom->is_chr_ram && ctx_rom->chr_data) {
    ctx_rom->chr_data[addr % ctx_rom->chr_size] = val;
  }
}

// --- Mapper 1 (MMC1) Logic ---

static void mmc1_reset(void) {
  mmc1.shift_reg = 0x10;
  mmc1.shift_count = 0;
  mmc1.control = 0x0C; // Mode 3, 8KB CHR
  mmc1.chr_bank0 = 0;
  mmc1.chr_bank1 = 0;
  mmc1.prg_bank = 0;
  memset(prg_ram, 0, sizeof(prg_ram));
}

static void mmc1_update_regs(uint16_t addr, uint8_t val) {
  // Reset bit
  if (val & 0x80) {
    mmc1.shift_reg = 0x10;
    mmc1.shift_count = 0;
    mmc1.control |= 0x0C;
    return;
  }

  // Shift in LSB
  bool last_write = (mmc1.shift_count == 4);
  mmc1.shift_reg = (mmc1.shift_reg >> 1) | ((val & 1) << 4);
  mmc1.shift_count++;

  if (last_write) {
    uint8_t data = mmc1.shift_reg;
    uint16_t reg = addr & 0x6000;

    if (reg == 0x0000) { // Control $8000-$9FFF
      mmc1.control = data;
    } else if (reg == 0x2000) { // CHR0 $A000-$BFFF
      mmc1.chr_bank0 = data;
    } else if (reg == 0x4000) { // CHR1 $C000-$DFFF
      mmc1.chr_bank1 = data;
    } else if (reg == 0x6000) { // PRG $E000-$FFFF
      mmc1.prg_bank = data;
    }

    mmc1.shift_reg = 0x10;
    mmc1.shift_count = 0;
  }
}

static uint32_t mmc1_get_prg_addr(uint16_t addr) {
  uint8_t mode = (mmc1.control >> 2) & 3;
  uint32_t bank = 0;
  uint32_t offset = addr & 0x3FFF;

  if (mode == 0 || mode == 1) { // 32KB Mode
    bank = (mmc1.prg_bank & 0xFE);
    // 32KB bank logic:
    // We are accessing 16KB windows at $8000 and $C000
    // If 32KB mode, $8000 calls this, $C000 calls this.
    // If addr $8000 ($0000 off), we want first 16KB of 32KB chunk.
    // If addr $C000 ($4000 off), we want second 16KB of 32KB chunk.
    // My previous logic: "If addr >= 0xC000 bank |= 1" handles it.
    // bank is 16KB index.
    if (addr >= 0xC000)
      bank |= 1;
    return (bank * 16384) + offset;
  } else if (mode == 2) { // Fix First
    if (addr < 0xC000) {
      return offset; // Bank 0
    } else {
      bank = mmc1.prg_bank & 0x0F;
      return (bank * 16384) + offset;
    }
  } else { // Fix Last (Mode 3)
    if (addr < 0xC000) {
      bank = mmc1.prg_bank & 0x0F;
      return (bank * 16384) + offset;
    } else {
      uint32_t last_bank = (ctx_rom->prg_size / 16384) - 1;
      return (last_bank * 16384) + offset;
    }
  }
}

static uint8_t mmc1_cpu_read(uint16_t addr) {
  if (addr >= 0x6000 && addr < 0x8000) {
    return prg_ram[addr - 0x6000];
  }

  if (addr >= 0x8000) {
    uint32_t phys = mmc1_get_prg_addr(addr);
    if (phys < ctx_rom->prg_size) {
      return ctx_rom->prg_data[phys];
    }
  }
  return 0;
}

static void mmc1_cpu_write(uint16_t addr, uint8_t val) {
  if (addr >= 0x6000 && addr < 0x8000) {
    prg_ram[addr - 0x6000] = val;
  }
  if (addr >= 0x8000) {
    mmc1_update_regs(addr, val);
  }
}

static uint32_t mmc1_get_chr_addr(uint16_t addr) {
  uint8_t mode = (mmc1.control >> 4) & 1;
  uint32_t bank = 0;
  uint32_t offset = addr & 0x0FFF;

  if (mode == 0) { // 8KB
    bank = mmc1.chr_bank0 & 0x1E;
    if (addr >= 0x1000)
      bank |= 1;
    return (bank * 4096) + offset;
  } else { // 4KB
    if (addr < 0x1000) {
      bank = mmc1.chr_bank0; // Select 4KB bank
    } else {
      bank = mmc1.chr_bank1;
    }
    return (bank * 4096) + offset;
  }
}

static uint8_t mmc1_ppu_read(uint16_t addr) {
  if (addr < 0x2000 && ctx_rom->chr_data) {
    uint32_t phys = mmc1_get_chr_addr(addr);
    return ctx_rom->chr_data[phys % ctx_rom->chr_size];
  }
  return 0;
}

static void mmc1_ppu_write(uint16_t addr, uint8_t val) {
  if (addr < 0x2000 && ctx_rom->is_chr_ram && ctx_rom->chr_data) {
    uint32_t phys = mmc1_get_chr_addr(addr);
    ctx_rom->chr_data[phys % ctx_rom->chr_size] = val;
  }
}

static uint8_t mmc1_get_mirroring(void) {
  uint8_t m = mmc1.control & 3;
  switch (m) {
  case 0:
    return MIRRORING_ONE_SCREEN_LO;
  case 1:
    return MIRRORING_ONE_SCREEN_HI;
  case 2:
    return MIRRORING_VERTICAL;
  case 3:
    return MIRRORING_HORIZONTAL;
  }
  return MIRRORING_VERTICAL;
}

void mapper_init(ROM *rom) {
  ctx_rom = rom;
  if (rom->mapper_id == 1) {
    mmc1_reset();
    printf("Mapper 1 (MMC1) Initialized\n");
  } else {
    printf("Mapper %d Initialized (NROM)\n", rom->mapper_id);
  }
}

uint8_t mapper_cpu_read(uint16_t addr) {
  if (!ctx_rom)
    return 0;
  if (ctx_rom->mapper_id == 0)
    return nrom_cpu_read(addr);
  if (ctx_rom->mapper_id == 1)
    return mmc1_cpu_read(addr);
  return 0;
}

void mapper_cpu_write(uint16_t addr, uint8_t val) {
  if (!ctx_rom)
    return;
  if (ctx_rom->mapper_id == 0)
    nrom_cpu_write(addr, val);
  if (ctx_rom->mapper_id == 1)
    mmc1_cpu_write(addr, val);
}

uint8_t mapper_ppu_read(uint16_t addr) {
  if (!ctx_rom)
    return 0;
  if (ctx_rom->mapper_id == 0)
    return nrom_ppu_read(addr);
  if (ctx_rom->mapper_id == 1)
    return mmc1_ppu_read(addr);
  return 0;
}

void mapper_ppu_write(uint16_t addr, uint8_t val) {
  if (!ctx_rom)
    return;
  if (ctx_rom->mapper_id == 0)
    nrom_ppu_write(addr, val);
  if (ctx_rom->mapper_id == 1)
    mmc1_ppu_write(addr, val);
}

uint8_t mapper_get_mirroring(void) {
  if (!ctx_rom)
    return MIRRORING_VERTICAL;
  if (ctx_rom->mapper_id == 0)
    return ctx_rom->mirroring;
  if (ctx_rom->mapper_id == 1)
    return mmc1_get_mirroring();
  return ctx_rom->mirroring;
}
