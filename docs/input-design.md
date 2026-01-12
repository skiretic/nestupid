# Input Design (Standard Controller)

## Overview
The NES uses a shift-register based protocol to read controller inputs. The standard controller (JOYPAD1 and JOYPAD2) is accessed via memory-mapped registers `$4016` and `$4017`.

## Hardware Interface

### Registers
- **`$4016` (Write)**: Strobe.
  - Writing `1` resets/reloads the shift registers in the controllers with current button states.
  - Writing `0` allows the shift registers to be clocked out.
- **`$4016` (Read)**: Controller 1 Data.
  - Returns **bit 0** of the shift register (current button).
  - Clocks the shift register to the next bit.
- **`$4017` (Read)**: Controller 2 Data.
  - Same as `$4016` but for Player 2.

### Protocol
1. **Pulse Strobe**:
   - CPU writes `1` to `$4016`.
   - CPU writes `0` to `$4016`.
   - This "latches" the current state of buttons (A, B, Select, Start, Up, Down, Left, Right) into the controller's internal shift register.
2. **Read Data**:
   - CPU reads `$4016` 8 times.
   - 1st Read: returns status of **A** (Bit 0).
   - 2nd Read: returns status of **B** (Bit 0).
   - ...and so on.
   - After 8 reads, subsequent reads return `1` (usually, depends on open bus behavior).

### 8-bit Shift Register Order
1. **A**
2. **B**
3. **Select**
4. **Start**
5. **Up**
6. **Down**
7. **Left**
8. **Right**

## Implementation Strategy

### Component: `input.c` / `input.h`
- **State**:
  - `uint8_t controller1_state`: Bitmask of currently held buttons (from SDL).
  - `uint8_t controller1_shifter`: The actual shift register modified by reads.
  - `bool strobe_active`: Tracks the strobe line state.

### Integration with Memory Bus
- Bus WRITE to `$4016`:
  - If `val & 1`: Set `strobe_active = true`. Reload `shifter = state`.
  - If `!(val & 1)`: Set `strobe_active = false`.
- Bus READ from `$4016`:
  - Return `shifter & 1`.
  - If `!strobe_active`, `shifter >>= 1`.
  - If `strobe_active`, `shifter` continuously reloads `state` (so repeated reads return button A).

### API
- `void input_update_state(uint8_t buttons)`: Called by main loop/GUI to update internal state from SDL events.
- `uint8_t input_read_4016(void)`: Called by CPU Bus read.
- `void input_write_4016(uint8_t data)`: Called by CPU Bus write.

## Handling DPCM Conflicts
The protocol mentions DPCM DMA can corrupt reads. Our initial implementation requires cycle-exact DMA stealing to emulate this bug (which some games rely on, but most work around). For Phase 4, we will implement ideal behavior first.
