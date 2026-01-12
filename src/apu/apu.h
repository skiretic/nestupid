#ifndef APU_H
#define APU_H

#include <stdint.h>

void apu_init(void);
void apu_reset(void);
void apu_step(void);

uint8_t apu_read_reg(uint16_t addr);
void apu_write_reg(uint16_t addr, uint8_t val);

#endif
