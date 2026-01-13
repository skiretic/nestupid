#ifndef SYSTEM_H
#define SYSTEM_H

#include <stdint.h>

// Step the entire system (PPU, APU) by one CPU cycle (master clock / 12)
void system_step(void);

#endif
