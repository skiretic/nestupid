#include "apu.h"
#include <stdio.h>

void apu_init(void) { printf("APU Init\n"); }

void apu_reset(void) { printf("APU Reset\n"); }

void apu_step(void) {
  // TODO: Implement APU frame counter and channels
}

uint8_t apu_read_reg(uint16_t addr) {
  switch (addr) {
  case 0x4015:
    // Status register
    return 0;
  default:
    // printf("APU Read Unmapped: %04X\n", addr);
    return 0;
  }
}

void apu_write_reg(uint16_t addr, uint8_t val) {
  switch (addr) {
  case 0x4000: // Pulse 1
  case 0x4001:
  case 0x4002:
  case 0x4003:
  case 0x4004: // Pulse 2
  case 0x4005:
  case 0x4006:
  case 0x4007:
  case 0x4008: // Triangle
  case 0x400A:
  case 0x400B:
  case 0x400C: // Noise
  case 0x400E:
  case 0x400F:
  case 0x4010: // DMC
  case 0x4011:
  case 0x4012:
  case 0x4013:
  case 0x4015: // Status
  case 0x4017: // Frame Counter
    // printf("APU Write: %04X = %02X\n", addr, val);
    break;
  default:
    // printf("APU Write Unmapped: %04X = %02X\n", addr, val);
    break;
  }
}
