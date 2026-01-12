#ifndef INPUT_H
#define INPUT_H

#include <stdbool.h>
#include <stdint.h>

// Standard NES Controller Buttons
// These bitmasks align with the shift register order:
// Bit 0: A
// Bit 1: B
// Bit 2: Select
// Bit 3: Start
// Bit 4: Up
// Bit 5: Down
// Bit 6: Left
// Bit 7: Right

#define BUTTON_A 0x01
#define BUTTON_B 0x02
#define BUTTON_SELECT 0x04
#define BUTTON_START 0x08
#define BUTTON_UP 0x10
#define BUTTON_DOWN 0x20
#define BUTTON_LEFT 0x40
#define BUTTON_RIGHT 0x80

// Initialize input system
void input_init(void);

// Called by main loop to update the internal state based on SDL events
// `controller` is 0 for Joypad 1, 1 for Joypad 2
void input_update(uint8_t controller, uint8_t buttons);

// Read from $4016 / $4017
// `controller` is 0 or 1
uint8_t input_read(uint8_t controller);

// Write to $4016 (Strobe)
// `val` only the lowest bit matters
void input_write_strobe(uint8_t val);

#endif // INPUT_H
