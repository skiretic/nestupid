#ifndef CPU_H
#define CPU_H

#include "rom.h"
#include <stdbool.h>
#include <stdint.h>

// Status Flags
#define FLAG_C 0x01 // Carry
#define FLAG_Z 0x02 // Zero
#define FLAG_I 0x04 // Interrupt Disable
#define FLAG_D 0x08 // Decimal (Ignored)
#define FLAG_B 0x10 // Break
#define FLAG_U 0x20 // Unused (Always 1)
#define FLAG_V 0x40 // Overflow
#define FLAG_N 0x80 // Negative

typedef struct {
  uint8_t a;   // Accumulator
  uint8_t x;   // Index X
  uint8_t y;   // Index Y
  uint8_t s;   // Stack Pointer
  uint8_t p;   // Status Register (Flags)
  uint16_t pc; // Program Counter

  // Internal emulator state
  uint64_t total_cycles;
  uint8_t cycles_wait; // Cycles for current instruction
  bool nmi_pending;
  bool irq_pending;
  uint16_t last_pcs[32]; // Trace buffer
  int trace_idx;
} CPU_State;

// Initialize CPU
void cpu_init(void);

// Reset CPU (Power-on or Reset button)
void cpu_reset(void);

// Execute one CPU step (one instruction) or handle interrupts
// Returns number of cycles consumed
uint8_t cpu_step(void);

// Get current CPU state (read-only)
// Get current CPU state (read-only)
const CPU_State *cpu_get_state(void);
void cpu_stall(int cycles);

// Signal NMI
void cpu_nmi(void);

#endif // CPU_H
