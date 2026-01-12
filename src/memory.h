#ifndef MEMORY_H
#define MEMORY_H

#include "rom.h"
#include <stdint.h>

// Initialize memory system with loaded ROM
void memory_init(ROM *rom);

// CPU Memory Bus Access
uint8_t cpu_read(uint16_t addr);
void cpu_write(uint16_t addr, uint8_t val);

#endif // MEMORY_H
