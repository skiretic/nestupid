#ifndef MAPPER_H
#define MAPPER_H

#include "rom.h"
#include <stdint.h>

// Initialize the mapper system with the loaded ROM
void mapper_init(ROM *rom);

// CPU Read/Write (PRG-ROM, PRG-RAM, Mapper Registers)
uint8_t mapper_cpu_read(uint16_t addr);
void mapper_cpu_write(uint16_t addr, uint8_t val);

// PPU Read/Write (CHR-ROM, CHR-RAM)
uint8_t mapper_ppu_read(uint16_t addr);
void mapper_ppu_write(uint16_t addr, uint8_t val);

// Get current mirroring mode
// Returns: MIRRORING_HORIZONTAL, MIRRORING_VERTICAL, or others
uint8_t mapper_get_mirroring(void);

#endif // MAPPER_H
