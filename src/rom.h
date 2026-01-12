#ifndef ROM_H
#define ROM_H

#include <stdbool.h>
#include <stddef.h>
#include <stdint.h>

// Mirroring modes
#define MIRRORING_HORIZONTAL 0
#define MIRRORING_VERTICAL 1
#define MIRRORING_FOUR_SCREEN 2

typedef struct {
  uint8_t magic[4];     // "NES" + 0x1A
  uint8_t prg_rom_size; // In 16KB units
  uint8_t chr_rom_size; // In 8KB units (0 means CHR-RAM)
  uint8_t flags6;       // Mapper lower nibble, mirroring, etc.
  uint8_t flags7;       // Mapper upper nibble, NES 2.0 signature
  uint8_t flags8;       // Prg RAM size (older iNES) or Mapper variant (NES 2.0)
  uint8_t flags9;  // TV system (older iNES) or Upper bits of ROM size (NES 2.0)
  uint8_t flags10; // TV system, PRG-RAM present (older iNES) or PRG-RAM/EEPROM
                   // size (NES 2.0)
  uint8_t padding[5]; // Unused in standard iNES
} NES_Header;

typedef struct {
  uint8_t *prg_data;
  size_t prg_size;
  uint8_t *chr_data;
  size_t chr_size;
  uint8_t mapper_id;
  uint8_t mirroring;
  bool is_chr_ram;
} ROM;

// Loads an NES ROM from a file
ROM *rom_load(const char *path);

// Frees a ROM structure and its buffers
void rom_free(ROM *rom);

#endif // ROM_H
