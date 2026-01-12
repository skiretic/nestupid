#include "input.h"
#include <stdio.h>

typedef struct {
  uint8_t state;   // Current button state (updated by SDL)
  uint8_t shifter; // Shift register for serial reading
} Controller;

static Controller controllers[2];
static bool strobe_active = false;

void input_init(void) {
  controllers[0].state = 0;
  controllers[0].shifter = 0;
  controllers[1].state = 0;
  controllers[1].shifter = 0;
  strobe_active = false;
  printf("Input System Initialized\n");
}

void input_update(uint8_t controller, uint8_t buttons) {
  if (controller > 1)
    return;

  controllers[controller].state = buttons;

  // If strobe is still active, the shifter continuously mirrors the state
  // (Standard behavior: while strobe is high, you get the 'A' button status
  // repeatedly)
  if (strobe_active) {
    controllers[controller].shifter = controllers[controller].state;
  }
}

uint8_t input_read(uint8_t controller) {
  if (controller > 1)
    return 0;

  uint8_t val = 0;

  if (strobe_active) {
    // While strobe is high, return the status of button A (Bit 0)
    // and keep reloading the shifter logic, effectively.
    // Usually on real hardware, it just returns the first bit repeatedly.
    val = controllers[controller].state & 0x01;
  } else {
    // Read the LSB from shifter
    val = controllers[controller].shifter & 0x01;
    // Shift right to prepare next button
    // Fill top with 1s (standard controller returns 1s after all 8 are read)
    controllers[controller].shifter >>= 1;
    controllers[controller].shifter |= 0x80;
  }

  // Standard controllers on NES return data on D0. D1-D4 are usually 0 (or open
  // bus). Some docs say bits 5-7 are open bus. We return just 0 or 1 for D0,
  // masked by the caller or bus usually? memory.c should handle the bus value.
  // Here we return 0 or 1.
  return val;
}

void input_write_strobe(uint8_t val) {
  // Only bit 0 matters
  bool new_strobe = (val & 0x01) != 0;

  if (strobe_active && !new_strobe) {
    // High-to-Low transition: Latch the current button states into the shifters
    controllers[0].shifter = controllers[0].state;
    controllers[1].shifter = controllers[1].state;
  }

  strobe_active = new_strobe;

  // If strobe is being held high, shifter track state immediately?
  // Yes, see input_update logic
  if (strobe_active) {
    controllers[0].shifter = controllers[0].state;
    controllers[1].shifter = controllers[1].state;
  }
}
