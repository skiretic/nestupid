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

// Length Counter Table
static const uint8_t length_table[32] = {
    10, 254, 20, 2,  40, 4,  80, 6,  160, 8,  60, 10, 14, 12, 26, 14,
    12, 16,  24, 18, 48, 20, 96, 22, 192, 24, 72, 26, 16, 28, 32, 30};

static void apu_write_pulse(APU_Pulse *p, uint8_t reg, uint8_t val) {
  switch (reg) {
  case 0:
    p->duty = (val >> 6) & 0x03;
    p->length_halt = (val & 0x20) != 0;
    p->constant_volume = (val & 0x10) != 0;
    p->volume = val & 0x0F;
    break;
  case 1:
    p->sweep_enabled = (val & 0x80) != 0;
    p->sweep_period = (val >> 4) & 0x07;
    p->sweep_negate = (val & 0x08) != 0;
    p->sweep_shift = val & 0x07;
    p->sweep_reload = true;
    break;
  case 2:
    p->timer_period = (p->timer_period & 0x0700) | val;
    break;
  case 3:
    p->timer_period = (p->timer_period & 0x00FF) | ((val & 0x07) << 8);
    if (p->enabled) {
      p->length_counter = length_table[(val >> 3) & 0x1F];
    }
    p->envelope_start = true;
    p->duty_pos = 0;
    break;
  }
}

static void apu_write_triangle(APU_Triangle *t, uint8_t reg, uint8_t val) {
  switch (reg) {
  case 0:
    t->length_halt = (val & 0x80) != 0;
    t->linear_counter_reload = val & 0x7F;
    break;
  case 2:
    t->timer_period = (t->timer_period & 0x0700) | val;
    break;
  case 3:
    t->timer_period = (t->timer_period & 0x00FF) | ((val & 0x07) << 8);
    if (t->enabled) {
      t->length_counter = length_table[(val >> 3) & 0x1F];
    }
    t->reload_linear = true;
    break;
  }
}

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
  case 0x4015: {
    // Status register
    uint8_t status = 0;
    if (apu.pulse1.length_counter > 0)
      status |= 0x01;
    if (apu.pulse2.length_counter > 0)
      status |= 0x02;
    if (apu.triangle.length_counter > 0)
      status |= 0x04;
    if (apu.noise.length_counter > 0)
      status |= 0x08;
    if (apu.dmc.bytes_remaining > 0)
      status |= 0x10;
    if (apu.frame_irq)
      status |= 0x40;
    if (apu.dmc_irq)
      status |= 0x80;

    // Reading 4015 clears frame irq
    apu.frame_irq = false;
    return status;
  }
  default:
    return 0;
  }
}

void apu_write_reg(uint16_t addr, uint8_t val) {
  switch (addr) {
  case 0x4000:
    apu_write_pulse(&apu.pulse1, 0, val);
    break;
  case 0x4001:
    apu_write_pulse(&apu.pulse1, 1, val);
    break;
  case 0x4002:
    apu_write_pulse(&apu.pulse1, 2, val);
    break;
  case 0x4003:
    apu_write_pulse(&apu.pulse1, 3, val);
    break;

  case 0x4004:
    apu_write_pulse(&apu.pulse2, 0, val);
    break;
  case 0x4005:
    apu_write_pulse(&apu.pulse2, 1, val);
    break;
  case 0x4006:
    apu_write_pulse(&apu.pulse2, 2, val);
    break;
  case 0x4007:
    apu_write_pulse(&apu.pulse2, 3, val);
    break;

  case 0x4008:
    apu_write_triangle(&apu.triangle, 0, val);
    break;
  case 0x400A:
    apu_write_triangle(&apu.triangle, 2, val);
    break;
  case 0x400B:
    apu_write_triangle(&apu.triangle, 3, val);
    break;

  case 0x400C: // Noise
  case 0x400E:
  case 0x400F:
  case 0x4010: // DMC
  case 0x4011:
  case 0x4012:
  case 0x4013:
    break;

  case 0x4015: // Status
    apu.pulse1.enabled = (val & 0x01) != 0;
    if (!apu.pulse1.enabled)
      apu.pulse1.length_counter = 0;

    apu.pulse2.enabled = (val & 0x02) != 0;
    if (!apu.pulse2.enabled)
      apu.pulse2.length_counter = 0;

    apu.triangle.enabled = (val & 0x04) != 0;
    if (!apu.triangle.enabled)
      apu.triangle.length_counter = 0;

    apu.noise.enabled = (val & 0x08) != 0;
    if (!apu.noise.enabled)
      apu.noise.length_counter = 0;

    apu.dmc.enabled = (val & 0x10) != 0;
    if (!apu.dmc.enabled)
      apu.dmc.bytes_remaining = 0;
    else if (apu.dmc.bytes_remaining == 0) {
      // If enabled and bytes were 0, restart only if needed (complicated,
      // skipping for now)
    }

    apu.dmc_irq = false;
    break;

  case 0x4017: // Frame Counter
    break;
  }
}
