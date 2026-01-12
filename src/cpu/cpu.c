#include "cpu.h"
#include "memory.h"
#include <stdio.h>
#include <stdlib.h>
#include <string.h>

static CPU_State cpu;

void cpu_init(void) {
  memset(&cpu, 0, sizeof(CPU_State));
  printf("CPU Initialized\n");
}

void cpu_reset(void) {
  // 6502 Power-up state
  cpu.a = 0;
  cpu.x = 0;
  cpu.y = 0;
  cpu.s = 0xFD;
  cpu.p = 0x24; // I=1, U=1

  // Load Reset Vector ($FFFC)
  uint8_t lo = cpu_read(0xFFFC);
  uint8_t hi = cpu_read(0xFFFD);
  cpu.pc = (hi << 8) | lo;

  cpu.total_cycles = 7; // Reset takes 7 cycles
  cpu.cycles_wait = 0;

  printf("CPU Reset. PC: %04X\n", cpu.pc);
  printf("Code at Reset (%04X):\n", cpu.pc);
  for (int i = 0; i < 0x50; i++) {
    printf("%02X ", cpu_read(cpu.pc + i));
    if ((i + 1) % 16 == 0)
      printf("\n");
  }
  printf("\n");
}

void cpu_nmi(void) {
  printf("CPU NMI Triggered!\n");
  cpu.nmi_pending = true;
  // We can't easily peek vector here without reading it, but we know it's at
  // FFFA
  uint8_t lo = cpu_read(0xFFFA);
  uint8_t hi = cpu_read(0xFFFB);
  uint16_t vec = (hi << 8) | lo;
  printf("Code at NMI (%04X): ", vec);
  for (int i = 0; i < 32; i++)
    printf("%02X ", cpu_read(vec + i));
  printf("\n");
}

const CPU_State *cpu_get_state(void) { return &cpu; }

// --- Addressing Modes ---

// Immediate: Operand is the next byte
static uint16_t addr_imm(void) { return cpu.pc++; }

// Zero Page: Operand is 8-bit address ($00LL)
static uint16_t addr_zp(void) { return cpu_read(cpu.pc++); }

// Zero Page, X: Operand + X ($00LL + X) (Wrap around zero page)
static uint16_t addr_zpx(void) {
  uint8_t addr = cpu_read(cpu.pc++);
  return (addr + cpu.x) & 0xFF;
}

// Zero Page, Y: Operand + Y ($00LL + Y) (Wrap around zero page)
static uint16_t addr_zpy(void) {
  uint8_t addr = cpu_read(cpu.pc++);
  return (addr + cpu.y) & 0xFF;
}

// Absolute: Operand is 16-bit address ($HHLL)
static uint16_t addr_abs(void) {
  uint8_t lo = cpu_read(cpu.pc++);
  uint8_t hi = cpu_read(cpu.pc++);
  return (hi << 8) | lo;
}

// Absolute, X: $HHLL + X
// Note: Page crossing often adds a cycle for reads (handled in opcode logic
// usually)
static uint16_t addr_absx(void) {
  uint8_t lo = cpu_read(cpu.pc++);
  uint8_t hi = cpu_read(cpu.pc++);
  uint16_t addr = (hi << 8) | lo;
  return (addr + cpu.x) & 0xFFFF;
}

// Absolute, Y: $HHLL + Y
static uint16_t addr_absy(void) {
  uint8_t lo = cpu_read(cpu.pc++);
  uint8_t hi = cpu_read(cpu.pc++);
  uint16_t addr = (hi << 8) | lo;
  return (addr + cpu.y) & 0xFFFF;
}

// Indirect: ($HHLL) - Only used by JMP. Has page boundary bug!
// If address is $xxFF, next byte is fetched from $xx00, not $xx00+1
static uint16_t addr_ind(void) {
  uint8_t ptr_lo = cpu_read(cpu.pc++);
  uint8_t ptr_hi = cpu_read(cpu.pc++);
  uint16_t ptr = (ptr_hi << 8) | ptr_lo;

  uint8_t lo = cpu_read(ptr);
  // Simulate Page Boundary Bug
  uint16_t next_ptr = (ptr & 0xFF00) | ((ptr + 1) & 0x00FF);
  uint8_t hi = cpu_read(next_ptr);

  return (hi << 8) | lo;
}

// Indirect, X (Indexed Indirect): ($LL + X) -> Pointer to address
static uint16_t addr_indx(void) {
  uint8_t ptr = cpu_read(cpu.pc++);
  uint8_t lo = cpu_read((ptr + cpu.x) & 0xFF);
  uint8_t hi = cpu_read((ptr + cpu.x + 1) & 0xFF);
  return (hi << 8) | lo;
}

// Indirect, Y (Indirect Indexed): ($LL) + Y -> Pointer + Y
static uint16_t addr_indy(void) {
  uint8_t ptr = cpu_read(cpu.pc++);
  uint8_t lo = cpu_read(ptr);
  uint8_t hi = cpu_read((ptr + 1) & 0xFF);
  uint16_t base = (hi << 8) | lo;
  return (base + cpu.y) & 0xFFFF;
}

// Relative: Branch offset
static uint16_t addr_rel(void) {
  uint8_t offset = cpu_read(cpu.pc++);
  return (uint16_t)((int16_t)cpu.pc + (int8_t)offset);
}
// --- Helpers ---

static void set_zn(uint8_t val) {
  if (val == 0)
    cpu.p |= FLAG_Z;
  else
    cpu.p &= ~FLAG_Z;

  if (val & 0x80)
    cpu.p |= FLAG_N;
  else
    cpu.p &= ~FLAG_N;
}

static void push(uint8_t val) {
  cpu_write(0x0100 | cpu.s, val);
  cpu.s--;
}

static uint8_t pop(void) {
  cpu.s++;
  return cpu_read(0x0100 | cpu.s);
}

static void push16(uint16_t val) {
  push((val >> 8) & 0xFF);
  push(val & 0xFF);
}

static uint16_t pop16(void) {
  uint8_t lo = pop();
  uint8_t hi = pop();
  return (hi << 8) | lo;
}

// --- Instructions ---

// LDA: Load Accumulator
static void op_lda(uint16_t addr) {
  cpu.a = cpu_read(addr);
  set_zn(cpu.a);
}

// LDX: Load X
static void op_ldx(uint16_t addr) {
  cpu.x = cpu_read(addr);
  set_zn(cpu.x);
}

// LDY: Load Y
static void op_ldy(uint16_t addr) {
  cpu.y = cpu_read(addr);
  set_zn(cpu.y);
}

// STA: Store Accumulator
static void op_sta(uint16_t addr) { cpu_write(addr, cpu.a); }

// STX: Store X
static void op_stx(uint16_t addr) { cpu_write(addr, cpu.x); }

// STY: Store Y
static void op_sty(uint16_t addr) { cpu_write(addr, cpu.y); }

// Transfer Instructions
static void op_tax(void) {
  cpu.x = cpu.a;
  set_zn(cpu.x);
}
static void op_tay(void) {
  cpu.y = cpu.a;
  set_zn(cpu.y);
}
static void op_txa(void) {
  cpu.a = cpu.x;
  set_zn(cpu.a);
}
static void op_tya(void) {
  cpu.a = cpu.y;
  set_zn(cpu.a);
}
static void op_tsx(void) {
  cpu.x = cpu.s;
  set_zn(cpu.x);
} // TSX sets Z/N
static void op_txs(void) { cpu.s = cpu.x; } // TXS does NOT set flags

// Stack Instructions
static void op_pha(void) { push(cpu.a); }
static void op_pla(void) {
  cpu.a = pop();
  set_zn(cpu.a);
}
static void op_php(void) {
  push(cpu.p | FLAG_B | FLAG_U);
} // B flag set on stack (PHP)
static void op_plp(void) {
  cpu.p = pop();
  cpu.p |= FLAG_U;
  cpu.p &= ~FLAG_B;
} // Ignore B flag pull

// Increment/Decrement Register
static void op_inx(void) {
  cpu.x++;
  set_zn(cpu.x);
}
static void op_iny(void) {
  cpu.y++;
  set_zn(cpu.y);
}
static void op_dex(void) {
  cpu.x--;
  set_zn(cpu.x);
}
static void op_dey(void) {
  cpu.y--;
  set_zn(cpu.y);
}

// --- Arithmetic / Logical ---

static void op_adc(uint16_t addr) {
  uint8_t val = cpu_read(addr);
  uint16_t sum = cpu.a + val + (cpu.p & FLAG_C);

  // Carry Flag: Set if overflow > 255
  if (sum > 0xFF)
    cpu.p |= FLAG_C;
  else
    cpu.p &= ~FLAG_C;

  // Overflow Flag: Set if sign of both inputs is same, but result sign is
  // different
  // ~(A ^ val) & (A ^ sum) & 0x80
  if (~(cpu.a ^ val) & (cpu.a ^ sum) & 0x80)
    cpu.p |= FLAG_V;
  else
    cpu.p &= ~FLAG_V;

  cpu.a = (uint8_t)sum;
  set_zn(cpu.a);
}

static void op_sbc(uint16_t addr) {
  uint8_t val = cpu_read(addr);
  // SBC is ADC with inverted data: A + ~M + C
  val = ~val;

  uint16_t sum = cpu.a + val + (cpu.p & FLAG_C);

  if (sum > 0xFF)
    cpu.p |= FLAG_C;
  else
    cpu.p &= ~FLAG_C;

  if (~(cpu.a ^ val) & (cpu.a ^ sum) & 0x80)
    cpu.p |= FLAG_V;
  else
    cpu.p &= ~FLAG_V;

  cpu.a = (uint8_t)sum;
  set_zn(cpu.a);
}

static void op_and(uint16_t addr) {
  cpu.a &= cpu_read(addr);
  set_zn(cpu.a);
}

static void op_ora(uint16_t addr) {
  cpu.a |= cpu_read(addr);
  set_zn(cpu.a);
}

static void op_eor(uint16_t addr) {
  cpu.a ^= cpu_read(addr);
  set_zn(cpu.a);
}

static void op_bit(uint16_t addr) {
  uint8_t val = cpu_read(addr);
  // Z flag set if (A & M) == 0
  if ((cpu.a & val) == 0)
    cpu.p |= FLAG_Z;
  else
    cpu.p &= ~FLAG_Z;

  // N and V flags match bits 7 and 6 of memory value
  cpu.p = (cpu.p & 0x3F) | (val & 0xC0);
}

static void op_cmp(uint16_t addr) {
  uint8_t val = cpu_read(addr);
  uint8_t diff = cpu.a - val;
  set_zn(diff);
  // Carry set if A >= M
  if (cpu.a >= val)
    cpu.p |= FLAG_C;
  else
    cpu.p &= ~FLAG_C;
}

static void op_cpx(uint16_t addr) {
  uint8_t val = cpu_read(addr);
  uint8_t diff = cpu.x - val;
  set_zn(diff);
  if (cpu.x >= val)
    cpu.p |= FLAG_C;
  else
    cpu.p &= ~FLAG_C;
}

static void op_cpy(uint16_t addr) {
  uint8_t val = cpu_read(addr);
  uint8_t diff = cpu.y - val;
  set_zn(diff);
  if (cpu.y >= val)
    cpu.p |= FLAG_C;
  else
    cpu.p &= ~FLAG_C;
}

// --- Shifts / Rotates ---

// ASL: Arithmetic Shift Left
static void op_asl_a(void) {
  if (cpu.a & 0x80)
    cpu.p |= FLAG_C;
  else
    cpu.p &= ~FLAG_C;
  cpu.a <<= 1;
  set_zn(cpu.a);
}

static void op_asl_m(uint16_t addr) {
  uint8_t val = cpu_read(addr);
  if (val & 0x80)
    cpu.p |= FLAG_C;
  else
    cpu.p &= ~FLAG_C;
  val <<= 1;
  cpu_write(addr, val);
  set_zn(val);
}

// LSR: Logical Shift Right
static void op_lsr_a(void) {
  if (cpu.a & 0x01)
    cpu.p |= FLAG_C;
  else
    cpu.p &= ~FLAG_C;
  cpu.a >>= 1;
  set_zn(cpu.a);
}

static void op_lsr_m(uint16_t addr) {
  uint8_t val = cpu_read(addr);
  if (val & 0x01)
    cpu.p |= FLAG_C;
  else
    cpu.p &= ~FLAG_C;
  val >>= 1;
  cpu_write(addr, val);
  set_zn(val);
}

// ROL: Rotate Left
static void op_rol_a(void) {
  uint8_t old_c = (cpu.p & FLAG_C) ? 1 : 0;
  if (cpu.a & 0x80)
    cpu.p |= FLAG_C;
  else
    cpu.p &= ~FLAG_C;
  cpu.a = (cpu.a << 1) | old_c;
  set_zn(cpu.a);
}

static void op_rol_m(uint16_t addr) {
  uint8_t val = cpu_read(addr);
  uint8_t old_c = (cpu.p & FLAG_C) ? 1 : 0;
  if (val & 0x80)
    cpu.p |= FLAG_C;
  else
    cpu.p &= ~FLAG_C;
  val = (val << 1) | old_c;
  cpu_write(addr, val);
  set_zn(val);
}

// ROR: Rotate Right
static void op_ror_a(void) {
  uint8_t old_c = (cpu.p & FLAG_C) ? 0x80 : 0;
  if (cpu.a & 0x01)
    cpu.p |= FLAG_C;
  else
    cpu.p &= ~FLAG_C;
  cpu.a = (cpu.a >> 1) | old_c;
  set_zn(cpu.a);
}

static void op_ror_m(uint16_t addr) {
  uint8_t val = cpu_read(addr);
  uint8_t old_c = (cpu.p & FLAG_C) ? 0x80 : 0;
  if (val & 0x01)
    cpu.p |= FLAG_C;
  else
    cpu.p &= ~FLAG_C;
  val = (val >> 1) | old_c;
  cpu_write(addr, val);
  set_zn(val);
}

// --- Jumps / Branches ---

static void op_jmp(uint16_t addr) { cpu.pc = addr; }

static void op_jsr(uint16_t addr) {
  push16(cpu.pc - 1);
  printf("JSR to %04X\n", addr);
  cpu.pc = addr;
}

static void op_rts(void) { cpu.pc = pop16() + 1; }

static void op_brk(void) {
  push16(cpu.pc + 1); // BRK skips one byte (signature/padding)
  push(cpu.p | FLAG_B | FLAG_U);
  cpu.p |= FLAG_I;
  uint8_t lo = cpu_read(0xFFFE);
  uint8_t hi = cpu_read(0xFFFF);
  cpu.pc = (hi << 8) | lo;
}

static void op_rti(void) {
  cpu.p = pop();
  cpu.p |= FLAG_U;  // Ensure Unused bit is always set
  cpu.p &= ~FLAG_B; // Break flag does not persist in register
  cpu.pc = pop16();
  printf("RTI Executed. Return to PC: %04X\n", cpu.pc);
}

static void branch_if(bool condition) {
  // Relative address is fetched before instruction execution logic usually?
  // In my addr_rel(), I read the byte and return the target address.
  // However, addr_rel() already advanced PC by 1.
  // So if I call addr_rel() inside the case switch, cpu.pc is already
  // pointing to next instruction? addr_rel reads the offset byte. Let's look
  // at my addr_rel: uint8_t offset = cpu_read(cpu.pc++); return
  // (uint16_t)((int16_t)cpu.pc + (int8_t)offset);

  // So calling addr_rel() inside the case consumes the byte and returns the
  // target.

  uint16_t target = addr_rel();

  if (condition) {
    // Page crossing check
    if ((cpu.pc & 0xFF00) != (target & 0xFF00)) {
      cpu.cycles_wait += 2; // +1 taken, +1 page crossed
    } else {
      cpu.cycles_wait += 1; // +1 taken
    }
    cpu.pc = target;
  }
}

// --- Status Flag Instructions ---

static void op_clc(void) { cpu.p &= ~FLAG_C; }
static void op_sec(void) { cpu.p |= FLAG_C; }
static void op_cli(void) { cpu.p &= ~FLAG_I; }
static void op_sei(void) { cpu.p |= FLAG_I; }
static void op_clv(void) { cpu.p &= ~FLAG_V; }
static void op_cld(void) { cpu.p &= ~FLAG_D; }
static void op_sed(void) { cpu.p |= FLAG_D; }

// INC: Increment Memory
static void op_inc_m(uint16_t addr) {
  uint8_t val = cpu_read(addr);
  val++;
  cpu_write(addr, val);
  set_zn(val);
}

// DEC: Decrement Memory
static void op_dec_m(uint16_t addr) {
  uint8_t val = cpu_read(addr);
  val--;
  cpu_write(addr, val);
  set_zn(val);
}

// Assuming CPU_State struct definition is elsewhere...
/*
  //  bool nmi_pending; // This seems to be already defined as cpu.nmi_pending
  bool irq_pending;
  uint32_t cycles_wait; // This seems to be already defined as cpu.cycles_wait
  uint32_t total_cycles; // This seems to be already defined as cpu.total_cycles
  uint16_t last_pcs[32]; // Trace buffer
  int trace_idx;
*/

uint8_t cpu_step(void) {
  // Stall if waiting for cycles
  if (cpu.cycles_wait > 0) {
    cpu.cycles_wait--;
    cpu.total_cycles++;
    return 1;
  }

  // Handle NMI
  if (cpu.nmi_pending) {
    printf("CPU Entering NMI Handler!\n");
    cpu.nmi_pending = false;
    push16(cpu.pc);
    push(cpu.p | FLAG_U); // B flag clear
    cpu.p |= FLAG_I;
    uint8_t lo = cpu_read(0xFFFA);
    uint8_t hi = cpu_read(0xFFFB);
    cpu.pc = (hi << 8) | lo;
    printf("NMI Vector: %04X\n", cpu.pc);
    cpu.cycles_wait = 7;
    return 1; // Start executing interrupt (cycles waited in subsequent calls)
  }

  // Trace
  cpu.last_pcs[cpu.trace_idx] = cpu.pc;
  cpu.trace_idx = (cpu.trace_idx + 1) % 32;

  // Fetch Opcode
  uint8_t opcode = cpu_read(cpu.pc++);

  /*
  if ((cpu.pc - 1) == 0x8EE0) {
    printf("DEBUG PC:8EE0 Opcode: %02X\n", opcode);
    printf("Code at 8E90-8EF0:\n");
    for (int i = 0; i < 0x60; i++) {
      printf("%02X ", cpu_read(0x8E90 + i));
      if ((i + 1) % 16 == 0)
        printf("\n");
    }
    printf("\n");
    printf("RAM [0300-0340]:\n");
    for (int i = 0; i < 0x40; i++) {
      printf("%02X ", cpu_read(0x0300 + i));
      if ((i + 1) % 16 == 0)
        printf("\n");
    }
    printf("\n");
    printf("Code at 8182-8200:\n");
    for (int i = 0; i < 0x80; i++) {
      printf("%02X ", cpu_read(0x8182 + i));
      if ((i + 1) % 16 == 0)
        printf("\n");
    }
    printf("\n");
    printf("Code at 8080-8180:\n");
    for (int i = 0; i < 0x100; i++) {
      printf("%02X ", cpu_read(0x8080 + i));
      if ((i + 1) % 16 == 0)
        printf("\n");
    }
    printf("\n");
    printf("Reset Vector Code (8000-8080):\n");
    for (int i = 0; i < 0x80; i++) {
      printf("%02X ", cpu_read(0x8000 + i));
      if ((i + 1) % 16 == 0)
        printf("\n");
    }
    printf("\n");
    printf("Subroutine at 90CC:\n");
    for (int i = 0; i < 0x40; i++) {
      printf("%02X ", cpu_read(0x90CC + i));
      if ((i + 1) % 16 == 0)
        printf("\n");
    }
    printf("\n");
    printf("Code at 8050-8080:\n");
    for (int i = 0; i < 0x30; i++) {
      printf("%02X ", cpu_read(0x8050 + i));
      if ((i + 1) % 16 == 0)
        printf("\n");
    }
    printf("\n");
    printf("Zero Page [00-0F]: ");
    for (int i = 0; i < 16; i++)
      printf("%02X ", cpu_read(i));
    printf("\n");
  }
  */

  // Unconditional logging with flush for debugging (First 5000 ops)
  /*
  static int log_count = 0;
  if (log_count < 10000) {
      log_count++;
      printf("PC:%04X OP:%02X A:%02X X:%02X Y:%02X P:%02X SP:%02X\n", cpu.pc
  - 1, opcode, cpu.a, cpu.x, cpu.y, cpu.p, cpu.s);
      // fflush(stdout); // Force flush if needed, but creates lag
  }
  */

  // Execute Opcode (Switch)
  switch (opcode) {
  case 0xEA: // NOP
    cpu.cycles_wait = 2;
    break;

  // LDA
  case 0xA9:
    op_lda(addr_imm());
    cpu.cycles_wait = 2;
    break;
  case 0xA5:
    op_lda(addr_zp());
    cpu.cycles_wait = 3;
    break;
  case 0xB5:
    op_lda(addr_zpx());
    cpu.cycles_wait = 4;
    break;
  case 0xAD:
    op_lda(addr_abs());
    cpu.cycles_wait = 4;
    break;
  case 0xBD:
    op_lda(addr_absx());
    cpu.cycles_wait = 4;
    break; // +1 if page crossed
  case 0xB9:
    op_lda(addr_absy());
    cpu.cycles_wait = 4;
    break; // +1 if page crossed
  case 0xA1:
    op_lda(addr_indx());
    cpu.cycles_wait = 6;
    break;
  case 0xB1:
    op_lda(addr_indy());
    cpu.cycles_wait = 5;
    break; // +1 if page crossed

  // LDX
  case 0xA2:
    op_ldx(addr_imm());
    cpu.cycles_wait = 2;
    break;
  case 0xA6:
    op_ldx(addr_zp());
    cpu.cycles_wait = 3;
    break;
  case 0xB6:
    op_ldx(addr_zpy());
    cpu.cycles_wait = 4;
    break;
  case 0xAE:
    op_ldx(addr_abs());
    cpu.cycles_wait = 4;
    break;
  case 0xBE:
    op_ldx(addr_absy());
    cpu.cycles_wait = 4;
    break; // +1 if page crossed

  // LDY
  case 0xA0:
    op_ldy(addr_imm());
    cpu.cycles_wait = 2;
    break;
  case 0xA4:
    op_ldy(addr_zp());
    cpu.cycles_wait = 3;
    break;
  case 0xB4:
    op_ldy(addr_zpx());
    cpu.cycles_wait = 4;
    break;
  case 0xAC:
    op_ldy(addr_abs());
    cpu.cycles_wait = 4;
    break;
  case 0xBC:
    op_ldy(addr_absx());
    cpu.cycles_wait = 4;
    break; // +1 if page crossed

  // STA
  case 0x85:
    op_sta(addr_zp());
    cpu.cycles_wait = 3;
    break;
  case 0x95:
    op_sta(addr_zpx());
    cpu.cycles_wait = 4;
    break;
  case 0x8D:
    op_sta(addr_abs());
    cpu.cycles_wait = 4;
    break;
  case 0x9D:
    op_sta(addr_absx());
    cpu.cycles_wait = 5;
    break;
  case 0x99:
    op_sta(addr_absy());
    cpu.cycles_wait = 5;
    break;
  case 0x81:
    op_sta(addr_indx());
    cpu.cycles_wait = 6;
    break;
  case 0x91:
    op_sta(addr_indy());
    cpu.cycles_wait = 6;
    break;

  // STX
  case 0x86:
    op_stx(addr_zp());
    cpu.cycles_wait = 3;
    break;
  case 0x96:
    op_stx(addr_zpy());
    cpu.cycles_wait = 4;
    break;
  case 0x8E:
    op_stx(addr_abs());
    cpu.cycles_wait = 4;
    break;

  // STY
  case 0x84:
    op_sty(addr_zp());
    cpu.cycles_wait = 3;
    break;
  case 0x94:
    op_sty(addr_zpx());
    cpu.cycles_wait = 4;
    break;
  case 0x8C:
    op_sty(addr_abs());
    cpu.cycles_wait = 4;
    break;

  // Transfer
  case 0xAA:
    op_tax();
    cpu.cycles_wait = 2;
    break;
  case 0xA8:
    op_tay();
    cpu.cycles_wait = 2;
    break;
  case 0x8A:
    op_txa();
    cpu.cycles_wait = 2;
    break;
  case 0x98:
    op_tya();
    cpu.cycles_wait = 2;
    break;
  case 0xBA:
    op_tsx();
    cpu.cycles_wait = 2;
    break;
  case 0x9A:
    op_txs();
    cpu.cycles_wait = 2;
    break;

  // Stack
  case 0x48:
    op_pha();
    cpu.cycles_wait = 3;
    break;
  case 0x68:
    op_pla();
    cpu.cycles_wait = 4;
    break;
  case 0x08:
    op_php();
    cpu.cycles_wait = 3;
    break;
  case 0x28:
    op_plp();
    cpu.cycles_wait = 4;
    break;

  // Inc/Dec Register
  case 0xE8:
    op_inx();
    cpu.cycles_wait = 2;
    break;
  case 0xC8:
    op_iny();
    cpu.cycles_wait = 2;
    break;
  case 0xCA:
    op_dex();
    cpu.cycles_wait = 2;
    break;
  case 0x88:
    op_dey();
    cpu.cycles_wait = 2;
    break;

  // ORA
  case 0x09:
    op_ora(addr_imm());
    cpu.cycles_wait = 2;
    break;
  case 0x05:
    op_ora(addr_zp());
    cpu.cycles_wait = 3;
    break;
  case 0x15:
    op_ora(addr_zpx());
    cpu.cycles_wait = 4;
    break;
  case 0x0D:
    op_ora(addr_abs());
    cpu.cycles_wait = 4;
    break;
  case 0x1D:
    op_ora(addr_absx());
    cpu.cycles_wait = 4;
    break;
  case 0x19:
    op_ora(addr_absy());
    cpu.cycles_wait = 4;
    break;
  case 0x01:
    op_ora(addr_indx());
    cpu.cycles_wait = 6;
    break;
  case 0x11:
    op_ora(addr_indy());
    cpu.cycles_wait = 5;
    break;

  // AND
  case 0x29:
    op_and(addr_imm());
    cpu.cycles_wait = 2;
    break;
  case 0x25:
    op_and(addr_zp());
    cpu.cycles_wait = 3;
    break;
  case 0x35:
    op_and(addr_zpx());
    cpu.cycles_wait = 4;
    break;
  case 0x2D:
    op_and(addr_abs());
    cpu.cycles_wait = 4;
    break;
  case 0x3D:
    op_and(addr_absx());
    cpu.cycles_wait = 4;
    break;
  case 0x39:
    op_and(addr_absy());
    cpu.cycles_wait = 4;
    break;
  case 0x21:
    op_and(addr_indx());
    cpu.cycles_wait = 6;
    break;
  case 0x31:
    op_and(addr_indy());
    cpu.cycles_wait = 5;
    break;

  // EOR
  case 0x49:
    op_eor(addr_imm());
    cpu.cycles_wait = 2;
    break;
  case 0x45:
    op_eor(addr_zp());
    cpu.cycles_wait = 3;
    break;
  case 0x55:
    op_eor(addr_zpx());
    cpu.cycles_wait = 4;
    break;
  case 0x4D:
    op_eor(addr_abs());
    cpu.cycles_wait = 4;
    break;
  case 0x5D:
    op_eor(addr_absx());
    cpu.cycles_wait = 4;
    break;
  case 0x59:
    op_eor(addr_absy());
    cpu.cycles_wait = 4;
    break;
  case 0x41:
    op_eor(addr_indx());
    cpu.cycles_wait = 6;
    break;
  case 0x51:
    op_eor(addr_indy());
    cpu.cycles_wait = 5;
    break;

  // ADC
  case 0x69:
    op_adc(addr_imm());
    cpu.cycles_wait = 2;
    break;
  case 0x65:
    op_adc(addr_zp());
    cpu.cycles_wait = 3;
    break;
  case 0x75:
    op_adc(addr_zpx());
    cpu.cycles_wait = 4;
    break;
  case 0x6D:
    op_adc(addr_abs());
    cpu.cycles_wait = 4;
    break;
  case 0x7D:
    op_adc(addr_absx());
    cpu.cycles_wait = 4;
    break;
  case 0x79:
    op_adc(addr_absy());
    cpu.cycles_wait = 4;
    break;
  case 0x61:
    op_adc(addr_indx());
    cpu.cycles_wait = 6;
    break;
  case 0x71:
    op_adc(addr_indy());
    cpu.cycles_wait = 5;
    break;

  // SBC
  case 0xE9:
    op_sbc(addr_imm());
    cpu.cycles_wait = 2;
    break;
  case 0xE5:
    op_sbc(addr_zp());
    cpu.cycles_wait = 3;
    break;
  case 0xF5:
    op_sbc(addr_zpx());
    cpu.cycles_wait = 4;
    break;
  case 0xED:
    op_sbc(addr_abs());
    cpu.cycles_wait = 4;
    break;
  case 0xFD:
    op_sbc(addr_absx());
    cpu.cycles_wait = 4;
    break;
  case 0xF9:
    op_sbc(addr_absy());
    cpu.cycles_wait = 4;
    break;
  case 0xE1:
    op_sbc(addr_indx());
    cpu.cycles_wait = 6;
    break;
  case 0xF1:
    op_sbc(addr_indy());
    cpu.cycles_wait = 5;
    break;

  // CMP
  case 0xC9:
    op_cmp(addr_imm());
    cpu.cycles_wait = 2;
    break;
  case 0xC5:
    op_cmp(addr_zp());
    cpu.cycles_wait = 3;
    break;
  case 0xD5:
    op_cmp(addr_zpx());
    cpu.cycles_wait = 4;
    break;
  case 0xCD:
    op_cmp(addr_abs());
    cpu.cycles_wait = 4;
    break;
  case 0xDD:
    op_cmp(addr_absx());
    cpu.cycles_wait = 4;
    break;
  case 0xD9:
    op_cmp(addr_absy());
    cpu.cycles_wait = 4;
    break;
  case 0xC1:
    op_cmp(addr_indx());
    cpu.cycles_wait = 6;
    break;
  case 0xD1:
    op_cmp(addr_indy());
    cpu.cycles_wait = 5;
    break;

  // CPX
  case 0xE0:
    op_cpx(addr_imm());
    cpu.cycles_wait = 2;
    break;
  case 0xE4:
    op_cpx(addr_zp());
    cpu.cycles_wait = 3;
    break;
  case 0xEC:
    op_cpx(addr_abs());
    cpu.cycles_wait = 4;
    break;

  // CPY
  case 0xC0:
    op_cpy(addr_imm());
    cpu.cycles_wait = 2;
    break;
  case 0xC4:
    op_cpy(addr_zp());
    cpu.cycles_wait = 3;
    break;
  case 0xCC:
    op_cpy(addr_abs());
    cpu.cycles_wait = 4;
    break;

  // ASL
  case 0x0A:
    op_asl_a();
    cpu.cycles_wait = 2;
    break;
  case 0x06:
    op_asl_m(addr_zp());
    cpu.cycles_wait = 5;
    break;
  case 0x16:
    op_asl_m(addr_zpx());
    cpu.cycles_wait = 6;
    break;
  case 0x0E:
    op_asl_m(addr_abs());
    cpu.cycles_wait = 6;
    break;
  case 0x1E:
    op_asl_m(addr_absx());
    cpu.cycles_wait = 7;
    break;

  // LSR
  case 0x4A:
    op_lsr_a();
    cpu.cycles_wait = 2;
    break;
  case 0x46:
    op_lsr_m(addr_zp());
    cpu.cycles_wait = 5;
    break;
  case 0x56:
    op_lsr_m(addr_zpx());
    cpu.cycles_wait = 6;
    break;
  case 0x4E:
    op_lsr_m(addr_abs());
    cpu.cycles_wait = 6;
    break;
  case 0x5E:
    op_lsr_m(addr_absx());
    cpu.cycles_wait = 7;
    break;

  // ROL
  case 0x2A:
    op_rol_a();
    cpu.cycles_wait = 2;
    break;
  case 0x26:
    op_rol_m(addr_zp());
    cpu.cycles_wait = 5;
    break;
  case 0x36:
    op_rol_m(addr_zpx());
    cpu.cycles_wait = 6;
    break;
  case 0x2E:
    op_rol_m(addr_abs());
    cpu.cycles_wait = 6;
    break;
    op_rol_m(addr_absx());
    cpu.cycles_wait = 7;
    break;

  // ROR
  case 0x6A:
    op_ror_a();
    cpu.cycles_wait = 2;
    break;
  case 0x66:
    op_ror_m(addr_zp());
    cpu.cycles_wait = 5;
    break;
  case 0x76:
    op_ror_m(addr_zpx());
    cpu.cycles_wait = 6;
    break;
  case 0x6E:
    op_ror_m(addr_abs());
    cpu.cycles_wait = 6;
    break;
  case 0x7E:
    op_ror_m(addr_absx());
    cpu.cycles_wait = 7;
    break;

  // JMP
  case 0x4C:
    op_jmp(addr_abs());
    cpu.cycles_wait = 3;
    break;
  case 0x6C:
    op_jmp(addr_ind());
    cpu.cycles_wait = 5;
    break;

  // JSR
  case 0x20:
    op_jsr(addr_abs());
    cpu.cycles_wait = 6;
    break;

  // RTS
  case 0x60:
    op_rts();
    cpu.cycles_wait = 6;
    break;

  // BRK
  case 0x00:
    op_brk();
    cpu.cycles_wait = 7;
    break;

  // RTI
  case 0x40:
    op_rti();
    cpu.cycles_wait = 6;
    break;

  // Branches
  case 0x10:
    branch_if(!(cpu.p & FLAG_N));
    cpu.cycles_wait = 2;
    break; // BPL
  case 0x30:
    branch_if(cpu.p & FLAG_N);
    cpu.cycles_wait = 2;
    break; // BMI
  case 0x50:
    branch_if(!(cpu.p & FLAG_V));
    cpu.cycles_wait = 2;
    break; // BVC
  case 0x70:
    branch_if(cpu.p & FLAG_V);
    cpu.cycles_wait = 2;
    break; // BVS
  case 0x90:
    branch_if(!(cpu.p & FLAG_C));
    cpu.cycles_wait = 2;
    break; // BCC
  case 0xB0:
    branch_if(cpu.p & FLAG_C);
    cpu.cycles_wait = 2;
    break; // BCS
  case 0xD0:
    branch_if(!(cpu.p & FLAG_Z));
    cpu.cycles_wait = 2;
    break; // BNE
  case 0xF0:
    branch_if(cpu.p & FLAG_Z);
    cpu.cycles_wait = 2;
    break;

  // Status Flags
  case 0x18:
    op_clc();
    cpu.cycles_wait = 2;
    break;
  case 0x38:
    op_sec();
    cpu.cycles_wait = 2;
    break;
  case 0x58:
    op_cli();
    cpu.cycles_wait = 2;
    break;
  case 0x78:
    op_sei();
    cpu.cycles_wait = 2;
    break;
  case 0xB8:
    op_clv();
    cpu.cycles_wait = 2;
    break;
  case 0xD8:
    op_cld();
    cpu.cycles_wait = 2;
    break;
  case 0xF8:
    op_sed();
    cpu.cycles_wait = 2;
    break;

  // BIT
  case 0x24:
    op_bit(addr_zp());
    cpu.cycles_wait = 3;
    break;
  case 0x2C:
    op_bit(addr_abs());
    cpu.cycles_wait = 4;
    break;

  // INC
  case 0xE6:
    op_inc_m(addr_zp());
    cpu.cycles_wait = 5;
    break;
  case 0xF6:
    op_inc_m(addr_zpx());
    cpu.cycles_wait = 6;
    break;
  case 0xEE:
    op_inc_m(addr_abs());
    cpu.cycles_wait = 6;
    break;
  case 0xFE:
    op_inc_m(addr_absx());
    cpu.cycles_wait = 7;
    break;

  // DEC
  case 0xC6:
    op_dec_m(addr_zp());
    cpu.cycles_wait = 5;
    break;
  case 0xD6:
    op_dec_m(addr_zpx());
    cpu.cycles_wait = 6;
    break;
  case 0xCE:
    op_dec_m(addr_abs());
    cpu.cycles_wait = 6;
    break;
  case 0xDE:
    op_dec_m(addr_absx());
    cpu.cycles_wait = 7;
    break;

  default:
    printf("FATAL: Illegal Opcode %02X at PC:%04X\n", opcode, cpu.pc - 1);
    exit(1);
    break;
  }

  // Correct Timing Logic:
  // 1. Account for the current cycle (Fetch/Execute)
  if (cpu.cycles_wait > 0) {
    cpu.cycles_wait--;
  }
  // 2. Return 1 (One CPU cycle stepped)
  return 1;
}

void cpu_stall(int cycles) { cpu.cycles_wait += cycles; }
