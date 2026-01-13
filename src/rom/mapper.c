#include "mapper.h"
#include "../ppu/ppu.h"
#include "cpu.h"
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

// --- MMC3 (Mapper 4) State ---
typedef struct {
  uint8_t bank_select;     // $8000 (Command)
  uint8_t prg_banks[2];    // $8001 (R6, R7)
  uint8_t chr_banks[6];    // $8001 (R0-R5)
  uint8_t mirroring;       // $A000
  uint8_t prg_ram_protect; // $A001

  // IRQ
  uint8_t irq_latch;
  uint8_t irq_counter;
  bool irq_enabled;
  bool irq_reload;

  // A12 Filter
  int a12_low_count;
} MMC3_State;

static MMC3_State mmc3;

// --- Mapper 2 (UxROM) State ---
static uint8_t uxrom_prg_bank = 0;

// --- Mapper 3 (CNROM) State ---
static uint8_t cnrom_chr_bank = 0;

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

// extern PPU_State ppu; // Removed

static bool mmc1_is_wram_disabled(void) {
  // 1. Standard PRG Bank Bit 4 Disable
  if (mmc1.prg_bank & 0x10)
    return true;

  // 2. SNROM CHR A16 Disable (Wiring: CHR A16 -> WRAM /CE)
  const PPU_State *ppu = ppu_get_state();

  bool rendering = (ppu->mask & 0x18);
  bool a12 = false;

  if (rendering) {
    if (ppu->dot >= 257 && ppu->dot <= 320) {
      // Sprite fetch time
      if (ppu->ctrl & 0x08)
        a12 = true; // Sprite Table $1000
    } else if (ppu->dot >= 1 && ppu->dot <= 256) {
      // BG fetch time
      if (ppu->ctrl & 0x10)
        a12 = true; // BG Table $1000
    }
  } else {
    // Idle
    if ((ppu->v & 0x1000))
      a12 = true;
  }

  // MMC1 4KB Mode: Bank determined by A12
  uint8_t chr_mode = (mmc1.control >> 4) & 1;
  uint8_t selected_bank = mmc1.chr_bank0;

  if (chr_mode == 1) { // 4KB
    if (a12)
      selected_bank = mmc1.chr_bank1;
    else
      selected_bank = mmc1.chr_bank0;
  }
  // In 8KB mode, bank0 is used (and A12 is low-order address bit inside bank),
  // but CHR A16 is effectively Bit 4 of Bank0?
  // Yes, 8KB mode: Banks are 8KB aligned (bit 0 ignored). Bank index bits 1-4.
  // So selected_bank = bank0.

  // Check Bit 4 (CHR A16)
  if (selected_bank & 0x10)
    return true;

  return false;
}

static uint8_t mmc1_cpu_read(uint16_t addr) {
  if (addr >= 0x6000 && addr < 0x8000) {
    if (!mmc1_is_wram_disabled()) {
      return prg_ram[addr - 0x6000];
    }
    return 0; // Open Bus
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
    if (!mmc1_is_wram_disabled()) {
      prg_ram[addr - 0x6000] = val;
    }
    return;
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

// --- Mapper 4 (MMC3) Logic ---

static void mmc3_reset(void) {
  memset(&mmc3, 0, sizeof(MMC3_State));

  // Initialize mirroring from ROM header
  // iNES: 0=Horizontal, 1=Vertical
  // MMC3: 0=Vertical, 1=Horizontal
  if (ctx_rom->mirroring == MIRRORING_VERTICAL) {
    mmc3.mirroring = 0;
  } else {
    mmc3.mirroring = 1;
  }
}

static uint32_t mmc3_get_prg_addr(uint16_t addr) {
  uint8_t prg_mode = (mmc3.bank_select & 0x40) >> 6;
  uint32_t bank = 0;
  uint32_t offset = addr & 0x1FFF; // 8KB windows

  // 4 x 8KB PRG Banks
  // $8000, $A000, $C000, $E000

  uint32_t last_bank = (ctx_rom->prg_size / 8192) - 1;
  uint32_t second_last_bank =
      last_bank -
      1; // Fixed unless in mode 1? No, 2nd last is always at E000? Wait.

  // Mode 0: $8000=R6, $A000=R7, $C000=Fixed(-2), $E000=Fixed(-1)
  // Mode 1: $8000=Fixed(-2), $A000=R7, $C000=R6, $E000=Fixed(-1)

  if (addr < 0xA000) { // $8000-$9FFF
    if (prg_mode == 0) {
      bank = mmc3.prg_banks[0];
    } else {
      bank = second_last_bank;
    }
  } else if (addr < 0xC000) { // $A000-$BFFF
    bank = mmc3.prg_banks[1];
  } else if (addr < 0xE000) { // $C000-$DFFF
    if (prg_mode == 0) {
      bank = second_last_bank;
    } else {
      bank = mmc3.prg_banks[0];
    }
  } else { // $E000-$FFFF
    bank = last_bank;
  }

  return (bank * 8192) + offset;
}

static uint8_t mmc3_cpu_read(uint16_t addr) {
  if (addr >= 0x6000 && addr < 0x8000) {
    return prg_ram[addr - 0x6000];
  }
  if (addr >= 0x8000) {
    uint32_t phys = mmc3_get_prg_addr(addr);
    if (phys < ctx_rom->prg_size) {
      return ctx_rom->prg_data[phys];
    }
  }
  return 0;
}

static void mmc3_cpu_write(uint16_t addr, uint8_t val) {
  if (addr >= 0x6000 && addr < 0x8000) {
    prg_ram[addr - 0x6000] = val;
    return;
  }

  // Registers $8000-$FFFF
  // Odd/Even based on low bit of address? No, ranges.
  // $8000, $8001 even/odd
  // $A000, $A001 even/odd

  bool even = (addr & 1) == 0;

  if (addr >= 0x8000 && addr <= 0x9FFF) {
    if (even) { // $8000 Bank Select
      mmc3.bank_select = val;
    } else { // $8001 Bank Data
      uint8_t cmd = mmc3.bank_select & 0x07;
      if (cmd <= 5) { // CHR
        mmc3.chr_banks[cmd] = val;
      } else if (cmd == 6) { // PRG R6
        mmc3.prg_banks[0] = val;
      } else if (cmd == 7) { // PRG R7
        mmc3.prg_banks[1] = val;
      }
    }
  } else if (addr >= 0xA000 && addr <= 0xBFFF) {
    if (even) { // $A000 Mirroring
      mmc3.mirroring = val;
      printf("MMC3 Mirroring Write: %02X (Mode: %s)\n", val,
             (val & 1) ? "Horizontal" : "Vertical");
    } else { // $A001 RAM Protect
      mmc3.prg_ram_protect = val;
    }
  } else if (addr >= 0xC000 && addr <= 0xDFFF) {
    if (even) { // $C000 IRQ Latch
      mmc3.irq_latch = val;
    } else { // $C001 IRQ Reload
      mmc3.irq_reload = true;
    }
  } else if (addr >= 0xE000 && addr <= 0xFFFF) {
    if (even) { // $E000 IRQ Disable
      mmc3.irq_enabled = false;
      cpu_clear_irq();
    } else { // $E001 IRQ Enable
      mmc3.irq_enabled = true;
    }
  }
}

static uint32_t mmc3_get_chr_addr(uint16_t addr) {
  // 6 CHR Banks. Two 2KB, Four 1KB.
  // CHR Mode (ctrl bit 7):
  // 0: 2KB @ $0000, 2KB @ $0800, 1KB @ $1000...
  // 1: 1KB @ $0000... 2KB @ $1000, 2KB @ $1800

  uint8_t chr_mode = (mmc3.bank_select & 0x80) >> 7;
  uint32_t bank = 0;
  int kb = 0; // 0 for 1KB chunks

  if (chr_mode == 0) {
    if (addr < 0x0800) {               // $0000-$07FF (2KB) -> R0
      bank = mmc3.chr_banks[0] & 0xFE; // Ignore LSB
      return (bank * 1024) + (addr & 0x07FF);
    } else if (addr < 0x1000) { // $0800-$0FFF (2KB) -> R1
      bank = mmc3.chr_banks[1] & 0xFE;
      return (bank * 1024) + (addr & 0x07FF);
    } else if (addr < 0x1400) { // $1000->R2
      bank = mmc3.chr_banks[2];
      return (bank * 1024) + (addr & 0x03FF);
    } else if (addr < 0x1800) { // R3
      bank = mmc3.chr_banks[3];
      return (bank * 1024) + (addr & 0x03FF);
    } else if (addr < 0x1C00) { // R4
      bank = mmc3.chr_banks[4];
      return (bank * 1024) + (addr & 0x03FF);
    } else { // R5
      bank = mmc3.chr_banks[5];
      return (bank * 1024) + (addr & 0x03FF);
    }
  } else {               // Mode 1: Inverted
    if (addr < 0x0400) { // $0000 -> R2
      bank = mmc3.chr_banks[2];
      return (bank * 1024) + (addr & 0x03FF);
    } else if (addr < 0x0800) { // R3
      bank = mmc3.chr_banks[3];
      return (bank * 1024) + (addr & 0x03FF);
    } else if (addr < 0x0C00) { // R4
      bank = mmc3.chr_banks[4];
      return (bank * 1024) + (addr & 0x03FF);
    } else if (addr < 0x1000) { // R5
      bank = mmc3.chr_banks[5];
      return (bank * 1024) + (addr & 0x03FF);
    } else if (addr < 0x1800) { // $1000 (2KB) -> R0
      bank = mmc3.chr_banks[0] & 0xFE;
      return (bank * 1024) + (addr & 0x07FF);
    } else { // $1800 (2KB) -> R1
      bank = mmc3.chr_banks[1] & 0xFE;
      return (bank * 1024) + (addr & 0x07FF);
    }
  }
}

#include "ppu.h"

static void mmc3_clock_irq(void) {
  if (mmc3.irq_counter == 0 || mmc3.irq_reload) {
    mmc3.irq_counter = mmc3.irq_latch;
    mmc3.irq_reload = false;
  } else {
    mmc3.irq_counter--;
  }

  if (mmc3.irq_counter == 0 && mmc3.irq_enabled) {
    // printf("MMC3 IRQ Fired! Scanline: %d Frame: %d Ctr: %d\n",
    //        ppu_get_scanline(), 0, mmc3.irq_counter); // TODO: Frame
    cpu_irq();
  }
}

void mapper_ppu_tick(uint16_t addr) {
  // printf("Tick: %04X A12: %d Low: %d\n", addr, (addr & 0x1000)>>12,
  // mmc3.a12_low_count); A12 is bit 12 (0x1000). Transition 0 -> 1 causes
  // clock. Filter: A12 must be low for a certain duration (M2 delays) We
  // simulate this by requiring multiple consecutive "Low" observations. Normal
  // pattern: BG ($0xxx, Low) -> Sprites ($1xxx, High)

  // Nametable fetches ($2xxx, A12=0) also count as Low!

  if ((addr & 0x1000) == 0) {
    mmc3.a12_low_count++;
  } else {
    if (mmc3.a12_low_count > 6) { // Threshold > 6 (Safe middle ground)
      mmc3_clock_irq();
    }
    mmc3.a12_low_count = 0;
  }
}

static uint8_t mmc3_ppu_read(uint16_t addr) {
  // mmc3_check_a12(addr); // Moved to global tick

  if (addr < 0x2000 && ctx_rom->chr_data) {
    uint32_t phys = mmc3_get_chr_addr(addr);
    return ctx_rom->chr_data[phys % ctx_rom->chr_size];
  }
  return 0;
}

static void mmc3_ppu_write(uint16_t addr, uint8_t val) {
  // mmc3_check_a12(addr); // Moved to global tick

  if (addr < 0x2000 && ctx_rom->is_chr_ram && ctx_rom->chr_data) {
    uint32_t phys = mmc3_get_chr_addr(addr);
    ctx_rom->chr_data[phys % ctx_rom->chr_size] = val;
  }
}

static uint8_t mmc3_get_mirroring(void) {
  if (mmc3.mirroring & 1)
    return MIRRORING_HORIZONTAL;
  else
    return MIRRORING_VERTICAL;
}

// --- Mapper 2 (UxROM) Logic ---

static void uxrom_reset(void) {
  uxrom_prg_bank = 0;
  printf("UxROM Reset\n");
}

static uint8_t uxrom_cpu_read(uint16_t addr) {
  if (addr >= 0x8000 && addr < 0xC000) {
    // Switchable Bank ($8000-$BFFF)
    unsigned int bank = uxrom_prg_bank;
    unsigned int offset = addr & 0x3FFF;
    unsigned int paddr = (bank * 16384) + offset;

    // Mask against PRG size to be safe (wrap around)
    if (ctx_rom->prg_size > 0)
      paddr %= ctx_rom->prg_size;

    return ctx_rom->prg_data[paddr];
  } else if (addr >= 0xC000) {
    // Fixed Last Bank ($C000-$FFFF)
    unsigned int last_bank_idx = (ctx_rom->prg_size / 16384) - 1;
    unsigned int offset = addr & 0x3FFF;
    return ctx_rom->prg_data[(last_bank_idx * 16384) + offset];
  }
  return 0;
}

static void uxrom_cpu_write(uint16_t addr, uint8_t val) {
  // $8000-$FFFF: Bank Select
  if (addr >= 0x8000) {
    uxrom_prg_bank = val & 0x0F; // Typical 4 bits for 256KB.
    // Uses full byte technically, but usually only lower bits matter based on
    // ROM size. e.g. for Castlevania (128KB), bits 0-2 matter. (0-7). Let's
    // just store val, valid check done in read.
    uxrom_prg_bank = val;
  }
}

static uint8_t uxrom_ppu_read(uint16_t addr) {
  // Uses CHR-RAM, standard mapping
  if (addr < 0x2000 && ctx_rom->chr_data) {
    return ctx_rom->chr_data[addr % ctx_rom->chr_size];
  }
  return 0;
}

static void uxrom_ppu_write(uint16_t addr, uint8_t val) {
  if (addr < 0x2000 && ctx_rom->is_chr_ram && ctx_rom->chr_data) {
    ctx_rom->chr_data[addr % ctx_rom->chr_size] = val;
  }
}

// --- Mapper 3 (CNROM) Logic ---

static void cnrom_reset(void) {
  cnrom_chr_bank = 0;
  printf("CNROM Reset\n");
}

static uint8_t cnrom_cpu_read(uint16_t addr) {
  // CNROM has fixed PRG ROM (16KB or 32KB)
  // Standard NROM-like behavior for PRG
  if (addr >= 0x8000) {
    uint32_t offset = addr - 0x8000;
    // Mirror 16KB if needed?
    if (ctx_rom->prg_size == 16384) {
      offset &= 0x3FFF;
    }
    if (offset < ctx_rom->prg_size) {
      return ctx_rom->prg_data[offset];
    }
  }
  return 0;
}

static void cnrom_cpu_write(uint16_t addr, uint8_t val) {
  // $8000-$FFFF: CHR Bank Select
  if (addr >= 0x8000) {
    cnrom_chr_bank = val & 0x03; // Usually 2 bits for 32KB max CHR
  }
}

static uint8_t cnrom_ppu_read(uint16_t addr) {
  if (addr < 0x2000 && ctx_rom->chr_data) {
    // 8KB Banked CHR
    uint32_t bank = cnrom_chr_bank;
    uint32_t offset = addr & 0x1FFF; // 8KB window
    uint32_t phys = (bank * 8192) + offset;
    return ctx_rom->chr_data[phys % ctx_rom->chr_size];
  }
  return 0;
}

static void cnrom_ppu_write(uint16_t addr, uint8_t val) {
  // Usually CNROM uses CHR-ROM, so not writable.
  // But if CHR-RAM is used (unlikely for standard CNROM?), we map it same way.
  if (addr < 0x2000 && ctx_rom->is_chr_ram && ctx_rom->chr_data) {
    uint32_t bank = cnrom_chr_bank;
    uint32_t offset = addr & 0x1FFF;
    uint32_t phys = (bank * 8192) + offset;
    ctx_rom->chr_data[phys % ctx_rom->chr_size] = val;
  }
}

void mapper_init(ROM *rom) {
  ctx_rom = rom;
  if (rom->mapper_id == 1) {
    mmc1_reset();
    printf("Mapper 1 (MMC1) Initialized\n");
  } else if (rom->mapper_id == 4) {
    mmc3_reset();
    printf("Mapper 4 (MMC3) Initialized\n");
  } else if (rom->mapper_id == 2) {
    uxrom_reset();
    uxrom_reset();
    printf("Mapper 2 (UxROM) Initialized\n");
  } else if (rom->mapper_id == 3) {
    cnrom_reset();
    printf("Mapper 3 (CNROM) Initialized\n");
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
  if (ctx_rom->mapper_id == 4)
    return mmc3_cpu_read(addr);
  if (ctx_rom->mapper_id == 2)
    return uxrom_cpu_read(addr);
  if (ctx_rom->mapper_id == 3)
    return cnrom_cpu_read(addr);
  return 0;
}

void mapper_cpu_write(uint16_t addr, uint8_t val) {
  if (!ctx_rom)
    return;
  if (ctx_rom->mapper_id == 0)
    nrom_cpu_write(addr, val);
  if (ctx_rom->mapper_id == 1)
    mmc1_cpu_write(addr, val);
  if (ctx_rom->mapper_id == 4)
    mmc3_cpu_write(addr, val);
  if (ctx_rom->mapper_id == 2)
    uxrom_cpu_write(addr, val);
  if (ctx_rom->mapper_id == 3)
    cnrom_cpu_write(addr, val);
}

uint8_t mapper_ppu_read(uint16_t addr) {
  if (!ctx_rom)
    return 0;
  if (ctx_rom->mapper_id == 0)
    return nrom_ppu_read(addr);
  if (ctx_rom->mapper_id == 1)
    return mmc1_ppu_read(addr);
  if (ctx_rom->mapper_id == 4)
    return mmc3_ppu_read(addr);
  if (ctx_rom->mapper_id == 2)
    return uxrom_ppu_read(addr);
  if (ctx_rom->mapper_id == 3)
    return cnrom_ppu_read(addr);
  return 0;
}

void mapper_ppu_write(uint16_t addr, uint8_t val) {
  if (!ctx_rom)
    return;
  if (ctx_rom->mapper_id == 0)
    nrom_ppu_write(addr, val);
  if (ctx_rom->mapper_id == 1)
    mmc1_ppu_write(addr, val);
  if (ctx_rom->mapper_id == 4)
    mmc3_ppu_write(addr, val);
  if (ctx_rom->mapper_id == 2)
    uxrom_ppu_write(addr, val);
  if (ctx_rom->mapper_id == 3)
    cnrom_ppu_write(addr, val);
}

uint8_t mapper_get_mirroring(void) {
  if (!ctx_rom)
    return MIRRORING_VERTICAL;
  if (ctx_rom->mapper_id == 0)
    return ctx_rom->mirroring;
  if (ctx_rom->mapper_id == 1)
    return mmc1_get_mirroring();
  if (ctx_rom->mapper_id == 4)
    return mmc3_get_mirroring();
  if (ctx_rom->mapper_id == 2)
    return ctx_rom->mirroring; // Hardwired in ROM header
  if (ctx_rom->mapper_id == 3)
    return ctx_rom->mirroring;
  return ctx_rom->mirroring;
}
