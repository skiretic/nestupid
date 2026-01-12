# APU Design (Ricoh 2A03 Audio)

## Overview
The NES APU generates sound via 5 channels: Pulse 1, Pulse 2, Triangle, Noise, and DMC.

## Channel Types & Register Map

### Pulse 1 ($4000-$4003) & Pulse 2 ($4004-$4007)
| Reg (P1) | Reg (P2) | Bits | Description |
|---|---|---|---|
| `$4000` | `$4004` | `DDLC VVVV` | D: Duty, L: Loop Env/Halt Length, C: Const Vol, V: Vol/Period |
| `$4001` | `$4005` | `EPPP NSSS` | E: Enabled, P: Period, N: Negate, S: Shift |
| `$4002` | `$4006` | `TTTT TTTT` | Timer Low 8 bits |
| `$4003` | `$4007` | `LLLL LTTT` | L: Length Counter Load (Index), T: Timer High 3 bits |

### Triangle ($4008-$400B)
| Reg | Bits | Description |
|---|---|---|
| `$4008` | `CRRR RRRR` | C: Control (Length Halt/Linear Load), R: Linear Counter Reload |
| `$400A` | `TTTT TTTT` | Timer Low 8 bits |
| `$400B` | `LLLL LTTT` | L: Length Counter Load, T: Timer High 3 bits |

### Noise ($400C-$400F)
| Reg | Bits | Description |
|---|---|---|
| `$400C` | `--LC VVVV` | L: Loop Env/Halt Length, C: Const Vol, V: Vol/Env Period |
| `$400E` | `M--- PPPP` | M: Mode (Loop noise), P: Period Index |
| `$400F` | `LLLL L---` | L: Length Counter Load |

### DMC ($4010-$4013)
| Reg | Bits | Description |
|---|---|---|
| `$4010` | `IL-- FFFF` | I: IRQ Enable, L: Loop, F: Frequency Index |
| `$4011` | `-DDD DDDD` | D: Direct Load (7-bit) |
| `$4012` | `AAAA AAAA` | Sample Address (`%11AAAAAA.AA000000`) |
| `$4013` | `LLLL LLLL` | Sample Length (`%LLLL.LLLL0001`) |

### Status ($4015)
- **Write**: `---D NT21` (Enable channels: DMC, Noise, Tri, Pulse 2, Pulse 1)
- **Read**: `IF-D NT21` (I: DMC IRQ, F: Frame IRQ, D: DMC Active, L: Length Counters > 0)

### Frame Counter ($4017)
- `MI-- ----`
  - **M**: Mode (0 = 4-step, 1 = 5-step)
  - **I**: IRQ Inhibit

## Frame Sequencer Timing
The Frame Counter clocks the envelopes, sweeps, and length counters.
Driven by APU Cycle (CPU/2).

**Mode 0 (4-Step)**:
- Step 1: Envelope, Linear (Triangle)
- Step 2: Envelope, Linear, Length, Sweep
- Step 3: Envelope, Linear
- Step 4: Envelope, Linear, Length, Sweep, IRQ (if not inhibited)

**Mode 1 (5-Step)**:
- Step 1: Envelope, Linear
- Step 2: Envelope, Linear, Length, Sweep
- Step 3: Envelope, Linear
- Step 4: -
- Step 5: Envelope, Linear, Length, Sweep
