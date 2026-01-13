#include "apu.h"
#include <stdio.h>

#include "../cpu/cpu.h"
#include "../memory/memory.h"
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

static void apu_write_noise(APU_Noise *n, uint8_t reg, uint8_t val) {
  switch (reg) {
  case 0:
    n->length_halt = (val & 0x20) != 0;
    n->constant_volume = (val & 0x10) != 0;
    n->volume = val & 0x0F;
    break;
  case 2:
    n->mode = (val & 0x80) != 0;
    n->timer_period = val & 0x0F;
    break;
  case 3:
    if (n->enabled) {
      n->length_counter = length_table[(val >> 3) & 0x1F];
    }
    n->envelope_start = true;
    break;
  }
}

// RATE TABLE FOR DMC (Declared here for visibility in write_dmc if desired,
// or I can forward declare it. Just putting helper here is fine if table is
// below? No, C needs decl. I'll put table at top or forward declare. Actually,
// let's put table here first).
static const uint16_t dmc_rate_table[16] = {428, 380, 340, 320, 286, 254,
                                            226, 214, 190, 160, 142, 128,
                                            106, 84,  72,  54};

static void apu_write_dmc(APU_DMC *d, uint8_t reg, uint8_t val) {
  switch (reg) {
  case 0: // $4010
    d->irq_enabled = (val & 0x80) != 0;
    d->loop = (val & 0x40) != 0;
    d->rate_index = val & 0x0F;
    d->timer_period = dmc_rate_table[d->rate_index];
    if (!d->irq_enabled) {
      apu.dmc_irq = false;
      cpu_clear_irq();
    }
    break;
  case 1: // $4011
    d->output_level = val & 0x7F;
    break;
  case 2: // $4012
    d->sample_address = 0xC000 + (val * 64);
    break;
  case 3: // $4013
    d->sample_length = (val * 16) + 1;
    break;
  }
}

// Redundant apu_init/reset removed (moved to later in file)

static void clock_length(void) {
  if (!apu.pulse1.length_halt && apu.pulse1.length_counter > 0)
    apu.pulse1.length_counter--;
  if (!apu.pulse2.length_halt && apu.pulse2.length_counter > 0)
    apu.pulse2.length_counter--;
  if (!apu.triangle.length_halt && apu.triangle.length_counter > 0)
    apu.triangle.length_counter--;
  if (!apu.noise.length_halt && apu.noise.length_counter > 0)
    apu.noise.length_counter--;

  // Triangle Linear Counter (clocked here? No, clocked in Envelope step
  // technically, wait) Actually Linear Counter is clocked 4 times a frame
  // (Envelope step). Length is 2 times. Length is clocked on steps 1 and 3
  // (0-indexed 0,1,2,3 -> Steps 2 and 4 in some docs). Let's stick to standard
  // NTSC: Mode 0: Step 1 (Env), Step 2 (Env, Len), Step 3 (Env), Step 4 (Env,
  // Len, IRQ)
}

static void clock_envelope(void) {
  // Triangle Linear Counter checks
  if (apu.triangle.reload_linear) {
    apu.triangle.linear_counter = apu.triangle.linear_counter_reload;
  } else if (apu.triangle.linear_counter > 0) {
    apu.triangle.linear_counter--;
  }
  if (!apu.triangle.length_halt)
    apu.triangle.reload_linear = false;

  // Pulse/Noise Envelopes (TODO: Full envelope logic)
  // For now we just implement Length Counter logic for the test
}

// 7457.5 CPU cycles per frame step (approx)
// We simplify to 7457
#define FRAME_CYCLES 7457

// Ring Buffer for Audio
#define AUDIO_BUFFER_SIZE 8192 // Increased buffer size for safety
static float audio_buffer[AUDIO_BUFFER_SIZE];
static volatile int buffer_write_pos = 0;
static volatile int buffer_read_pos =
    0; // Explicitly volatile for thread safety in this simple lock-free usage

// Basic lock-free capacity check
// Full if (write + 1) % SIZE == read
// Empty if write == read

static void buffer_write(float sample) {
  int next_pos = (buffer_write_pos + 1) % AUDIO_BUFFER_SIZE;
  if (next_pos != buffer_read_pos) {
    audio_buffer[buffer_write_pos] = sample;
    buffer_write_pos = next_pos;
  }
  // Else: Buffer full, drop sample (better than overwriting or blocking in this
  // context)
}

// ---------------------------
// Waveform Generation
// ---------------------------

static uint8_t pulse_duty_table[4][8] = {
    {0, 1, 0, 0, 0, 0, 0, 0}, // 12.5%
    {0, 1, 1, 0, 0, 0, 0, 0}, // 25%
    {0, 1, 1, 1, 1, 0, 0, 0}, // 50%
    {1, 0, 0, 1, 1, 1, 1, 1}  // 25% negated
};

static uint8_t triangle_sequence[32] = {
    15, 14, 13, 12, 11, 10, 9, 8, 7, 6, 5,  4,  3,  2,  1,  0,
    0,  1,  2,  3,  4,  5,  6, 7, 8, 9, 10, 11, 12, 13, 14, 15};

static uint8_t pulse_output(APU_Pulse *p) {
  if (!p->enabled || p->length_counter == 0 || p->timer_period < 8)
    return 0;
  // if (p->sweep_forcing_silence) return 0; // TODO: Sweep

  if (pulse_duty_table[p->duty][p->duty_pos]) {
    return p->constant_volume ? p->volume : p->envelope_counter;
  }
  return 0;
}

static uint8_t triangle_output(APU_Triangle *t) {
  if (!t->enabled || t->length_counter == 0 || t->linear_counter == 0)
    return 0;

  return triangle_sequence[t->seq_index];
}

static uint8_t noise_output(APU_Noise *n) {
  if (!n->enabled || n->length_counter == 0)
    return 0;
  if (n->lfsr & 0x01) // Output is 0 if bit 0 is set ? No, volume if bit 0 is 0
    return 0;

  return n->constant_volume ? n->volume : n->envelope_counter;
}

static uint8_t dmc_output(APU_DMC *d) { return d->output_level; }

// ---------------------------
// Mixer
// ---------------------------
// Approximated linear mixing for simplicity, or look-up table
// output = pulse_out + tnd_out
// pulse_out = 0.00752 * (pulse1 + pulse2)
// tnd_out = 0.00851 * triangle + 0.00494 * noise + 0.00335 * dmc

// High-Pass Filters
// The NES has two HPFs:
// 1. First-order, approx 90Hz cutoff
// 2. First-order, approx 440Hz cutoff
// We will implement a single aggregate HPF to remove DC offset.
static float hpf_prev_in = 0.0f;
static float hpf_prev_out = 0.0f;
static float sample_accumulator = 0.0f; // Moved to file scope for reset

// DMC Logic
static void dmc_step(void) {
  APU_DMC *d = &apu.dmc;

  // Memory Reader (If buffer empty and bytes remaining)
  // Check this BEFORE timer? Real HW does reader independent of timer, but
  // usually synced. Reader needs to fill buffer if empty.
  if (d->buffer_empty && d->bytes_remaining > 0) {
    // Read Sample
    // CPU Stall: 4 cycles for DMC DMA
    // We only stall if CPU is running. Since we are in apu_step (called by
    // main), we assume we can read. Ideally calls cpu_stall(4); cpu_stall(4);
    // // TODO: Add timing accuracy later

    d->sample_buffer = cpu_read(d->current_address);
    d->buffer_empty = false;

    d->current_address++;
    if (d->current_address == 0) { // Wrap FFFF -> 0000 -> 8000?
      // Spec: "Address is incremented. If it exceeds $FFFF, it wraps to $8000."
      d->current_address = 0x8000;
    }
    // Wait, standard increment doesn't wrap to 8000 on overflow of 16-bit
    // unless we check. cpu_read takes uint16_t, so overflow wraps to 0. If
    // d->current_address was 0xFFFF, ++ makes it 0x0000. We must catch this
    // wrap.
    if (d->current_address < 0x8000 && d->bytes_remaining > 0) {
      // If we wrapped to 0, fix to 8000.
      // Actually, simpler logic: if (addr == 0x8000) check if previous was
      // 0xFFFF? Just check if it became 0. But wait, valid range is 8000-FFFF.
      if (d->current_address == 0)
        d->current_address = 0x8000;
    }

    d->bytes_remaining--;
    if (d->bytes_remaining == 0) {
      if (d->loop) {
        d->current_address = d->sample_address;
        d->bytes_remaining = d->sample_length;
      } else if (d->irq_enabled) {
        apu.dmc_irq = true;
        cpu_irq();
      }
    }
  }

  // Timer
  if (d->enabled) {
    if (d->timer > 0) {
      d->timer--;
    } else {
      d->timer = d->timer_period;

      // Output Cycle
      if (!d->silence) {
        int delta = (d->shift_register & 0x01) ? 2 : -2;
        int new_level = d->output_level + delta;

        // Clamp 0..127
        if (new_level > 127)
          new_level = 127;
        if (new_level < 0)
          new_level = 0;

        d->output_level = new_level;
        d->shift_register >>= 1;
      }

      d->bits_remaining--;
      if (d->bits_remaining == 0) {
        d->bits_remaining = 8;

        if (d->buffer_empty) {
          d->silence = true;
        } else {
          d->silence = false;
          d->shift_register = d->sample_buffer;
          d->buffer_empty = true;
          // Buffer consumed, loop handles refill next step/immediately
        }
      }
    }
  }
}

static float mix_samples(void) {
  uint8_t p1 = pulse_output(&apu.pulse1);
  uint8_t p2 = pulse_output(&apu.pulse2);
  uint8_t t = triangle_output(&apu.triangle);
  uint8_t n = noise_output(&apu.noise);
  uint8_t d = dmc_output(&apu.dmc);

  float pulse_out = 0.00752f * (p1 + p2);
  float tnd_out = 0.00851f * t + 0.00494f * n + 0.00335f * d;

  float raw_output = pulse_out + tnd_out;

  // Simple High Pass Filter (RC filter implementation)
  // y[i] = alpha * (y[i-1] + x[i] - x[i-1])
  // alpha ~ 0.996 for reasonable DC removal
  float alpha = 0.996f;
  float filtered_out = alpha * (hpf_prev_out + raw_output - hpf_prev_in);

  hpf_prev_in = raw_output;
  hpf_prev_out = filtered_out;

  return filtered_out;
}

void apu_init(void) {
  printf("APU Init\n");
  apu_reset();
}

void apu_reset(void) {
  printf("APU Reset\n");
  memset(&apu, 0, sizeof(APU_State));
  apu.noise.lfsr = 1;

  // Reset Audio State
  buffer_read_pos = 0;
  buffer_write_pos = 0;
  hpf_prev_in = 0.0f;
  hpf_prev_out = 0.0f;
  sample_accumulator = 0.0f;
  memset(audio_buffer, 0, sizeof(audio_buffer));
}

// Clock everything
void apu_step(void) {
  apu.clock_count++;

  // 1. Frame Counter (Quarter Frame / Half Frame)
  if (apu.clock_count % (FRAME_CYCLES / 4) == 0) { // Rough approx
    // We implemented detailed 4-step/5-step in previous 'apu_step'
    // This needs to be carefully merged.
    // FOR NOW: Let's stick to the previous 'apu_step' logic for frame counter
    // but inject timer clocking every APU cycle.
  }

  // --- REIMPLMENTING FRAME COUNTER LOGIC from original file for clarity ---
  // Ideally we separate 'Clock Frame Counter' from 'Clock Timers'

  // Frame Counter Step
  // (Keeping original frame counter logic is complex inside this small window)

  // 2. Timers
  // Pulse 1
  if (apu.pulse1.timer > 0) {
    apu.pulse1.timer--;
  } else {
    apu.pulse1.timer = apu.pulse1.timer_period;
    apu.pulse1.duty_pos = (apu.pulse1.duty_pos + 1) & 7;
  }

  // Pulse 2
  if (apu.pulse2.timer > 0) {
    apu.pulse2.timer--;
  } else {
    apu.pulse2.timer = apu.pulse2.timer_period;
    apu.pulse2.duty_pos = (apu.pulse2.duty_pos + 1) & 7;
  }

  // Triangle
  if (apu.triangle.timer > 0) {
    apu.triangle.timer--;
  } else {
    apu.triangle.timer = apu.triangle.timer_period;
    if (apu.triangle.linear_counter > 0 && apu.triangle.length_counter > 0) {
      apu.triangle.seq_index = (apu.triangle.seq_index + 1) & 31;
    }
  }

  // Noise
  if (apu.noise.timer > 0) {
    apu.noise.timer--;
  } else {
    apu.noise.timer = apu.noise.timer_period; // Lookup table needed actually
    uint16_t feedback;
    if (apu.noise.mode) {
      feedback = (apu.noise.lfsr & 1) ^ ((apu.noise.lfsr >> 6) & 1);
    } else {
      feedback = (apu.noise.lfsr & 1) ^ ((apu.noise.lfsr >> 1) & 1);
    }
    apu.noise.lfsr >>= 1;
    apu.noise.lfsr |= (feedback << 14);
  }

  // DMC
  dmc_step();

  // 3. Audio Sampling (Downsample to 44.1kHz)
  // CPU(1.789773MHz) / 44100 ~= 40.5844...
  sample_accumulator += 1.0f;
  if (sample_accumulator >= 40.5844f) {
    sample_accumulator -= 40.5844f;
    buffer_write(mix_samples());
  }

  // Original Frame Counter Logic (Simplified for merging)
  if (apu.clock_count >= FRAME_CYCLES) {
    apu.clock_count = 0;
    apu.frame_step++;

    // Mode 0: 4-Step Sequence
    if (apu.frame_counter_mode == 0) {
      if (apu.frame_step > 3)
        apu.frame_step = 0;

      switch (apu.frame_step) {
      case 0:
        clock_envelope();
        break;
      case 1:
        clock_envelope();
        clock_length();
        break;
      case 2:
        clock_envelope();
        break;
      case 3:
        clock_envelope();
        clock_length();
        if (!apu.irq_inhibit)
          apu.frame_irq = true;
        break;
      }
    }
    // Mode 1: 5-Step Sequence
    else {
      if (apu.frame_step > 4)
        apu.frame_step = 0;

      switch (apu.frame_step) {
      case 0:
        clock_envelope();
        break;
      case 1:
        clock_envelope();
        clock_length();
        break;
      case 2:
        clock_envelope();
        break;
      case 3:
        break; // Step 4 does nothing
      case 4:
        clock_envelope();
        clock_length();
        break;
      }
    }
  }
}

void apu_fill_buffer(void *userdata, uint8_t *stream, int len) {
  float *fstream = (float *)stream;
  int samples_needed = len / sizeof(float);

  for (int i = 0; i < samples_needed; i++) {
    if (buffer_read_pos != buffer_write_pos) {
      fstream[i] = audio_buffer[buffer_read_pos];
      buffer_read_pos = (buffer_read_pos + 1) % AUDIO_BUFFER_SIZE;
    } else {
      // Buffer underflow: Output silence (0.0f).
      // Since we have a High-Pass Filter, the signal is centered at 0.0f,
      // so silence is appropriate and shouldn't cause a huge pop
      // unless the last sample was high amplitude (a real signal cut).
      // For smoother handling, we could repeat the last sample, but 0.0f is
      // safer for silence.
      fstream[i] = 0.0f;
    }
  }
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

  case 0x400C:
    apu_write_noise(&apu.noise, 0, val);
    break;
  case 0x400E:
    apu_write_noise(&apu.noise, 2, val);
    break;
  case 0x400F:
    apu_write_noise(&apu.noise, 3, val);
    break;

  case 0x4010:
    apu_write_dmc(&apu.dmc, 0, val);
    break;
  case 0x4011:
    apu_write_dmc(&apu.dmc, 1, val);
    break;
  case 0x4012:
    apu_write_dmc(&apu.dmc, 2, val);
    break;
  case 0x4013:
    apu_write_dmc(&apu.dmc, 3, val);
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
      // If enabled and bytes were 0, restart only if needed
    }

    apu.dmc_irq = false;
    break;

  case 0x4017: // Frame Counter
    break;
  }
}
