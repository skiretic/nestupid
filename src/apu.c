#include "apu.h"
#include <stdio.h>

#include <stdbool.h>
#include <string.h>

typedef struct {
  uint8_t enabled;
  uint8_t duty;
  uint8_t volume;
  bool constant_volume;
  bool length_halt; // Also envelope loop
  uint16_t timer;
  uint16_t timer_period;
  uint8_t length_counter;
  uint8_t envelope_counter;
  uint8_t envelope_period;
  bool envelope_start;
  // Sweep
  bool sweep_enabled;
  uint8_t sweep_period;
  bool sweep_negate;
  uint8_t sweep_shift;
  uint8_t sweep_counter;
  bool sweep_reload;
  uint8_t duty_pos;
} APU_Pulse;

typedef struct {
  uint8_t enabled;
  uint8_t linear_counter_reload;
  uint8_t linear_counter;
  bool length_halt; // Also control flag
  bool reload_linear;
  uint16_t timer;
  uint16_t timer_period;
  uint8_t length_counter;
  uint8_t seq_index;
} APU_Triangle;

typedef struct {
  uint8_t enabled;
  bool length_halt; // Also envelope loop
  bool constant_volume;
  uint8_t volume;
  uint8_t envelope_counter;
  uint8_t envelope_period;
  bool envelope_start;
  uint16_t timer;
  uint16_t timer_period;
  uint8_t length_counter;
  uint16_t lfsr;
  bool mode;
} APU_Noise;

typedef struct {
  uint8_t enabled;
  bool irq_enabled;
  bool loop;
  uint16_t rate_index;
  uint16_t timer;
  uint16_t timer_period;
  uint16_t sample_address;
  uint16_t sample_length;
  uint16_t current_address;
  uint16_t bytes_remaining;
  uint8_t output_level;
  uint8_t shift_register;
  uint8_t bits_remaining;
  bool buffer_empty;
  uint8_t sample_buffer;
  bool silence;
} APU_DMC;

typedef struct {
  APU_Pulse pulse1;
  APU_Pulse pulse2;
  APU_Triangle triangle;
  APU_Noise noise;
  APU_DMC dmc;

  uint64_t clock_count;
  uint8_t frame_counter_mode;
  bool irq_inhibit;
  bool frame_irq;
  bool dmc_irq;

  // 5-step mode or 4-step mode
  uint16_t frame_step;
} APU_State;

static APU_State apu;

void apu_init(void) {
  printf("APU Init\n");
  apu_reset();
}

void apu_reset(void) {
  printf("APU Reset\n");
  memset(&apu, 0, sizeof(APU_State));
  apu.noise.lfsr = 1;
}

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
