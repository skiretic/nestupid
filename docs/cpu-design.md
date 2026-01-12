# CPU Design (MOS 6502)

## Overview
The NES uses a Ricoh 2A03, which contains a MOS 6502 core lacking decimal mode support. It runs at approx 1.79 MHz (NTSC).

## State Structure

```c
typedef struct {
    uint8_t a;  // Accumulator
    uint8_t x;  // Index X
    uint8_t y;  // Index Y
    uint8_t s;  // Stack Pointer
    uint8_t p;  // Status Register (Flags)
    uint16_t pc; // Program Counter
    
    // Internal emulator state
    uint64_t total_cycles;
    uint8_t cycles_wait; // Cycles for current instruction
    bool nmi_pending;
    bool irq_pending;
} CPU_State;
```

## Flags (Status Register P)
| Bit | Name | Description |
|---|---|---|
| 0 | **C** | Carry Flag |
| 1 | **Z** | Zero Flag |
| 2 | **I** | Interrupt Disable |
| 3 | **D** | Decimal Mode (Ignored on NES, but flag exists) |
| 4 | **B** | Break Command (Only exists on stack) |
| 5 | **-** | Unused (Always 1 on stack) |
| 6 | **V** | Overflow Flag |
| 7 | **N** | Negative Flag |

## Addressing Modes
| Mode | Syntax | Bytes |
|---|---|---|
| Implicit | `INX` | 1 |
| Accumulator | `ASL A` | 1 |
| Immediate | `LDA #$10` | 2 |
| Zero Page | `LDA $10` | 2 |
| Zero Page,X | `LDA $10,X` | 2 |
| Zero Page,Y | `LDX $10,Y` | 2 |
| Relative | `BEQ +5` | 2 |
| Absolute | `LDA $1234` | 3 |
| Absolute,X | `LDA $1234,X` | 3 |
| Absolute,Y | `LDA $1234,Y` | 3 |
| Indirect | `JMP ($1234)` | 3 |
| Indirect,X | `LDA ($10,X)` | 2 |
| Indirect,Y | `LDA ($10),Y` | 2 |

## Official Opcode Table (151 Opcodes)

Cycles Key: `+1` if page crossed, `+1` if branch taken, `+2` if branch taken across page.

| Hex | Mnemonic | Mode | Cycles | Hex | Mnemonic | Mode | Cycles | Hex | Mnemonic | Mode | Cycles | Hex | Mnemonic | Mode | Cycles |
|---|---|---|---|---|---|---|---|---|---|---|---|---|---|---|---|
| 00 | BRK | Impl | 7 | 01 | ORA | Ind,X | 6 | 05 | ORA | ZP | 3 | 06 | ASL | ZP | 5 |
| 08 | PHP | Impl | 3 | 09 | ORA | Imm | 2 | 0A | ASL | Acc | 2 | 0D | ORA | Abs | 4 |
| 0E | ASL | Abs | 6 | 10 | BPL | Rel | 2* | 11 | ORA | Ind,Y | 5* | 15 | ORA | ZP,X | 4 |
| 16 | ASL | ZP,X | 6 | 18 | CLC | Impl | 2 | 19 | ORA | Abs,Y | 4* | 1D | ORA | Abs,X | 4* |
| 1E | ASL | Abs,X | 7 | 20 | JSR | Abs | 6 | 21 | AND | Ind,X | 6 | 24 | BIT | ZP | 3 |
| 25 | AND | ZP | 3 | 26 |ROL | ZP | 5 | 28 | PLP | Impl | 4 | 29 | AND | Imm | 2 |
| 2A | ROL | Acc | 2 | 2C | BIT | Abs | 4 | 2D | AND | Abs | 4 | 2E | ROL | Abs | 6 |
| 30 | BMI | Rel | 2* | 31 | AND | Ind,Y | 5* | 35 | AND | ZP,X | 4 | 36 | ROL | ZP,X | 6 |
| 38 | SEC | Impl | 2 | 39 | AND | Abs,Y | 4* | 3D | AND | Abs,X | 4* | 3E | ROL | Abs,X | 7 |
| 40 | RTI | Impl | 6 | 41 | EOR | Ind,X | 6 | 45 | EOR | ZP | 3 | 46 | LSR | ZP | 5 |
| 48 | PHA | Impl | 3 | 49 | EOR | Imm | 2 | 4A | LSR | Acc | 2 | 4C | JMP | Abs | 3 |
| 4D | EOR | Abs | 4 | 4E | LSR | Abs | 6 | 50 | BVC | Rel | 2* | 51 | EOR | Ind,Y | 5* |
| 55 | EOR | ZP,X | 4 | 56 | LSR | ZP,X | 6 | 58 | CLI | Impl | 2 | 59 | EOR | Abs,Y | 4* |
| 5D | EOR | Abs,X | 4* | 5E | LSR | Abs,X | 7 | 60 | RTS | Impl | 6 | 61 | ADC | Ind,X | 6 |
| 65 | ADC | ZP | 3 | 66 | ROR | ZP | 5 | 68 | PLA | Impl | 4 | 69 | ADC | Imm | 2 |
| 6A | ROR | Acc | 2 | 6C | JMP | Ind | 5 | 6D | ADC | Abs | 4 | 6E | ROR | Abs | 6 |
| 70 | BVS | Rel | 2* | 71 | ADC | Ind,Y | 5* | 75 | ADC | ZP,X | 4 | 76 | ROR | ZP,X | 6 |
| 78 | SEI | Impl | 2 | 79 | ADC | Abs,Y | 4* | 7D | ADC | Abs,X | 4* | 7E | ROR | Abs,X | 7 |
| 81 | STA | Ind,X | 6 | 84 | STY | ZP | 3 | 85 | STA | ZP | 3 | 86 | STX | ZP | 3 |
| 88 | DEY | Impl | 2 | 8A | TXA | Impl | 2 | 8C | STY | Abs | 4 | 8D | STA | Abs | 4 |
| 8E | STX | Abs | 4 | 90 | BCC | Rel | 2* | 91 | STA | Ind,Y | 6 | 94 | STY | ZP,X | 4 |
| 95 | STA | ZP,X | 4 | 96 | STX | ZP,Y | 4 | 98 | TYA | Impl | 2 | 99 | STA | Abs,Y | 5 |
| 9A | TXS | Impl | 2 | 9D | STA | Abs,X | 5 | A0 | LDY | Imm | 2 | A1 | LDA | Ind,X | 6 |
| A2 | LDX | Imm | 2 | A4 | LDY | ZP | 3 | A5 | LDA | ZP | 3 | A6 | LDX | ZP | 3 |
| A8 | TAY | Impl | 2 | A9 | LDA | Imm | 2 | AA | TAX | Impl | 2 | AC | LDY | Abs | 4 |
| AD | LDA | Abs | 4 | AE | LDX | Abs | 4 | B0 | BCS | Rel | 2* | B1 | LDA | Ind,Y | 5* |
| B4 | LDY | ZP,X | 4 | B5 | LDA | ZP,X | 4 | B6 | LDX | ZP,Y | 4 | B8 | CLV | Impl | 2 |
| B9 | LDA | Abs,Y | 4* | BA | TSX | Impl | 2 | BC | LDY | Abs,X | 4* | BD | LDA | Abs,X | 4* |
| BE | LDX | Abs,Y | 4* | C0 | CPY | Imm | 2 | C1 | CMP | Ind,X | 6 | C4 | CPY | ZP | 3 |
| C5 | CMP | ZP | 3 | C6 | DEC | ZP | 5 | C8 | INY | Impl | 2 | C9 | CMP | Imm | 2 |
| CA | DEX | Impl | 2 | CC | CPY | Abs | 4 | CD | CMP | Abs | 4 | CE | DEC | Abs | 6 |
| D0 | BNE | Rel | 2* | D1 | CMP | Ind,Y | 5* | D5 | CMP | ZP,X | 4 | D6 | DEC | ZP,X | 6 |
| D8 | CLD | Impl | 2 | D9 | CMP | Abs,Y | 4* | DD | CMP | Abs,X | 4* | DE | DEC | Abs,X | 7 |
| E0 | CPX | Imm | 2 | E1 | SBC | Ind,X | 6 | E4 | CPX | ZP | 3 | E5 | SBC | ZP | 3 |
| E6 | INC | ZP | 5 | E8 | INX | Impl | 2 | E9 | SBC | Imm | 2 | EA | NOP | Impl | 2 |
| EC | CPX | Abs | 4 | ED | SBC | Abs | 4 | EE | INC | Abs | 6 | F0 | BEQ | Rel | 2* |
| F1 | SBC | Ind,Y | 5* | F5 | SBC | ZP,X | 4 | F6 | INC | ZP,X | 6 | F8 | SED | Impl | 2 |
| F9 | SBC | Abs,Y | 4* | FD | SBC | Abs,X | 4* | FE | INC | Abs,X | 7 | | | | |

(Note: All blanks are illegal opcodes).
