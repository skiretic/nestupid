// tests/test_apu_basic.c
#include "../src/apu.h"
#include <assert.h>
#include <stdio.h>

// Mock or stub necessary dependencies if any (none for now as apu.c is
// self-contained mostly)

int main() {
  printf("Running Basic APU Test...\n");

  apu_init();
  apu_reset();

  // Enable Pulse 1 (Bit 0 of 0x4015)
  apu_write_reg(0x4015, 0x01);

  // Write to Pulse 1 Control (Duty, Envelope) - 0x4000
  apu_write_reg(0x4000, 0xBF);

  // Write Pulse 1 Timer Low
  apu_write_reg(0x4002, 0xFD);

  // Write Pulse 1 Timer High + Length Counter Load (Length index 0 -> count 10)
  // 0x00 -> Length Counter index 0 (approx 10 ticks)
  apu_write_reg(0x4003, 0x00);

  // Run a single step (shouldn't decrement length immediately if configured
  // right, or will decrement by 1)
  apu_step();

  // Check Status Register 0x4015
  // Bit 0 should be 1 (Pulse 1 length counter > 0)
  uint8_t status = apu_read_reg(0x4015);
  printf("Status Reg: %02X\n", status);

  if ((status & 1) == 0) {
    printf("FAIL: Pulse 1 not active in Status register\n");
    return 1;
  }

  // Test Triangle Channel
  // Enable Triangle (Bit 2 of 0x4015, | existing)
  apu_write_reg(0x4015, 0x01 | 0x04);

  // Write Triangle Linear Counter (0xFF)
  apu_write_reg(0x4008, 0xFF);

  // Write Triangle Timer Low
  apu_write_reg(0x400A, 0x00);

  // Write Triangle Timer High + Length (Length index 0 -> 10)
  apu_write_reg(0x400B, 0x00);

  apu_step();

  status = apu_read_reg(0x4015);
  // Bit 2 should be 1
  if ((status & 0x04) == 0) {
    printf("Status Reg: %02X\n", status);
    printf("FAIL: Triangle not active in Status register\n");
    return 1;
  }

  printf("Basic APU test passed\n");
  return 0;
}
