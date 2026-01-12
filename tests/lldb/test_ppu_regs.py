import lldb

def __lldb_init_module(debugger, internal_dict):
    debugger.HandleCommand('command script add -f test_ppu_regs.test_ppu_regs test_ppu_regs')

def test_ppu_regs(debugger, command, result, internal_dict):
    target = debugger.GetSelectedTarget()
    process = target.GetProcess()
    thread = process.GetSelectedThread()
    frame = thread.GetSelectedFrame()
    
    print("Testing PPU Registers via CPU Bus...")
    
    # 1. PPUCTRL Write ($2000)
    # Write 0xFF to $2000 via cpu_write
    frame.EvaluateExpression("(void)cpu_write(0x2000, 0xFF)")
    
    # Verify internal ppu.ctrl
    ctrl = frame.EvaluateExpression("ppu.ctrl").GetValueAsUnsigned()
    print(f"PPUCTRL: {hex(ctrl)}")
    if ctrl == 0xFF:
        print("SUCCESS: PPUCTRL updated.")
    else:
        print(f"FAILURE: PPUCTRL expected 0xFF, got {hex(ctrl)}")

    # 2. VRAM Increment
    # Set increment to +32 (Bit 2 of CTRL = 0x04)
    # Write 0x04 to $2000
    frame.EvaluateExpression("(void)cpu_write(0x2000, 0x04)")
    
    # Set PPUADDR ($2006) to $0000
    frame.EvaluateExpression("(void)cpu_write(0x2006, 0x00)")
    frame.EvaluateExpression("(void)cpu_write(0x2006, 0x00)")
    
    # Verify v = 0
    v = frame.EvaluateExpression("ppu.v").GetValueAsUnsigned()
    print(f"PPU V (Addr): {hex(v)}")
    
    # Write to PPUDATA ($2007)
    frame.EvaluateExpression("(void)cpu_write(0x2007, 0x42)")
    
    # Verify v incremented by 32
    v_new = frame.EvaluateExpression("ppu.v").GetValueAsUnsigned()
    print(f"PPU V (After Write): {hex(v_new)}")
    
    if v_new == 32: # 0x20
        print("SUCCESS: VRAM Address incremented by 32.")
    else:
        print(f"FAILURE: Expected V=0x20, got {hex(v_new)}")

    # 3. Reading Status ($2002)
    # Manually set VBlank flag in Status
    frame.EvaluateExpression("ppu.status |= 0x80")
    
    # Read $2002
    status_val = frame.EvaluateExpression("(uint8_t)cpu_read(0x2002)").GetValueAsUnsigned()
    print(f"Read $2002: {hex(status_val)}")
    
    if status_val & 0x80:
        print("SUCCESS: Read VBlank flag.")
    else:
        print("FAILURE: VBlank flag not read.")
        
    # Verify VBlank flag cleared after read
    status_after = frame.EvaluateExpression("ppu.status").GetValueAsUnsigned()
    if not (status_after & 0x80):
        print("SUCCESS: VBlank flag cleared after read.")
    else:
        print("FAILURE: VBlank flag NOT cleared.")
