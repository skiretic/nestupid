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

  // Write to Pulse 1 Control (Duty, Envelope) - 0x4000
  // Duty: 10 (50%), Loop: 1, Const: 1, Vol: 1111 (15) -> 0xBF
  apu_write_reg(0x4000, 0xBF);

  // Run a single step
  apu_step();

  printf("Basic APU test passed (no crash)\n");
  return 0;
}
