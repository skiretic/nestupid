#include "rom.h"
#include <stdio.h>
#include <stdlib.h>
#include <string.h>

#define PRG_UNIT_SIZE 16384
#define CHR_UNIT_SIZE 8192
#define HEADER_SIZE 16
#define TRAINER_SIZE 512

ROM *rom_load(const char *path) {
  FILE *f = fopen(path, "rb");
  if (!f) {
    fprintf(stderr, "Failed to open ROM file: %s\n", path);
    return NULL;
  }

  NES_Header header;
  if (fread(&header, 1, HEADER_SIZE, f) != HEADER_SIZE) {
    fprintf(stderr, "Failed to read ROM header\n");
    fclose(f);
    return NULL;
  }

  // Validate Magic
  if (header.magic[0] != 'N' || header.magic[1] != 'E' ||
      header.magic[2] != 'S' || header.magic[3] != 0x1A) {
    fprintf(stderr, "Invalid NES ROM signature\n");
    fclose(f);
    return NULL;
  }

  // Parse Mapper and Flags
  uint8_t mapper_lower = (header.flags6 & 0xF0) >> 4;
  uint8_t mapper_upper = (header.flags7 & 0xF0);
  uint8_t mapper_id = mapper_upper | mapper_lower;

  uint8_t mirroring =
      (header.flags6 & 0x01) ? MIRRORING_VERTICAL : MIRRORING_HORIZONTAL;
  if (header.flags6 & 0x08) {
    mirroring = MIRRORING_FOUR_SCREEN; // 4-screen VRAM
  }

  // Check for Trainer
  int has_trainer = (header.flags6 & 0x04);
  if (has_trainer) {
    fseek(f, TRAINER_SIZE, SEEK_CUR);
  }

  // Determine Sizes (Simple iNES support for now)
  size_t prg_size = header.prg_rom_size * PRG_UNIT_SIZE;
  size_t chr_size = header.chr_rom_size * CHR_UNIT_SIZE;

  // Allocate ROM struct
  ROM *rom = (ROM *)malloc(sizeof(ROM));
  if (!rom) {
    fprintf(stderr, "Failed to allocate ROM struct\n");
    fclose(f);
    return NULL;
  }

  rom->mapper_id = mapper_id;
  rom->mirroring = mirroring;
  rom->prg_size = prg_size;
  rom->chr_size = chr_size;
  rom->is_chr_ram = (header.chr_rom_size == 0);
  rom->prg_data = NULL;
  rom->chr_data = NULL;

  // Read PRG ROM
  if (prg_size > 0) {
    rom->prg_data = (uint8_t *)malloc(prg_size);
    if (!rom->prg_data) {
      fprintf(stderr, "Failed to allocate PRG ROM buffer\n");
      rom_free(rom);
      fclose(f);
      return NULL;
    }
    if (fread(rom->prg_data, 1, prg_size, f) != prg_size) {
      fprintf(stderr, "Failed to read PRG ROM data\n");
      rom_free(rom);
      fclose(f);
      return NULL;
    }
  }

  // Read CHR ROM
  if (chr_size > 0) {
    rom->chr_data = (uint8_t *)malloc(chr_size);
    if (!rom->chr_data) {
      fprintf(stderr, "Failed to allocate CHR ROM buffer\n");
      rom_free(rom);
      fclose(f);
      return NULL;
    }
    if (fread(rom->chr_data, 1, chr_size, f) != chr_size) {
      fprintf(stderr, "Failed to read CHR ROM data\n");
      rom_free(rom);
      fclose(f);
      return NULL;
    }
  } else {
    // CHR RAM - Allocate 8KB by default for NROM if CHR size is 0 and no CHR
    // ROM present Note: Actual logic might vary, but for NROM usually implies
    // 8KB RAM if not ROM However, iNES header size 0 implies using CHR-RAM. We
    // will allocate 8KB for CHR-RAM if size is 0. Actually, let's stick to the
    // spec: "0 implies CHR RAM". We will allocate it here or in Memory? Best to
    // allocate it here so ROM struct owns the CHR data pointer regardless of
    // source.
    rom->chr_size = 8192;
    rom->chr_data = (uint8_t *)malloc(rom->chr_size); // 8KB CHR RAM
    if (!rom->chr_data) {
      fprintf(stderr, "Failed to allocate CHR RAM\n");
      rom_free(rom);
      fclose(f);
      return NULL;
    }
    // Initialize with zeros or pattern? Zeros is safer.
    memset(rom->chr_data, 0, rom->chr_size);
  }

  fclose(f);
  printf("ROM Loaded: %s\n", path);
  printf("  Mapper: %d\n", mapper_id);
  printf("  PRG Size: %zu\n", prg_size);
  printf("  CHR Size: %zu\n", chr_size);
  if (rom->chr_data) {
    printf("  CHR Data Head: ");
    for (int k = 0; k < 16; k++)
      printf("%02X ", rom->chr_data[k]);
    printf("\n");
  }
  printf("  Mirroring: %s\n",
         mirroring == MIRRORING_VERTICAL ? "Vertical" : "Horizontal");

  return rom;
}

void rom_free(ROM *rom) {
  if (rom) {
    if (rom->prg_data)
      free(rom->prg_data);
    if (rom->chr_data)
      free(rom->chr_data);
    free(rom);
  }
}
