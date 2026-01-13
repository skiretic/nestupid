import lldb

def __lldb_init_module(debugger, internal_dict):
    debugger.HandleCommand('command script add -f test_rom_loading.test_rom_loading test_rom_loading')

def test_rom_loading(debugger, command, result, internal_dict):
    target = debugger.GetSelectedTarget()
    process = target.GetProcess()
    thread = process.GetSelectedThread()
    frame = thread.GetSelectedFrame()

    # Verify ROM pointer (current_rom is static global in main.c)
    # Since we are likely in a different frame (cpu_reset), we look at target globals
    rom_ptr = target.FindFirstGlobalVariable("current_rom")
    
    if not rom_ptr.IsValid():
        # Fallback: Try looking in frame/parent just in case setup changes
        rom_ptr = frame.FindVariable("current_rom")
        if not rom_ptr.IsValid():
             parent = thread.GetFrameAtIndex(1)
             if parent.IsValid():
                 rom_ptr = parent.FindVariable("current_rom")

    if not rom_ptr.IsValid():
        print("WARNING: 'current_rom' variable not found via target or frames.")
        # We might be strictly in a scope where debug info for main.c isn't linked? 
        # But usually globals works.
    elif rom_ptr.GetValueAsUnsigned() == 0:
        print("FAILURE: 'current_rom' is NULL")
        return
    else:    
        print(f"SUCCESS: ROM pointer valid: {rom_ptr.GetValue()}")

    # Verify CPU Read (Reset Vector)
    # Mapping check: $FFFC should map to PRG ROM
    val = frame.EvaluateExpression("(uint8_t)cpu_read(0xFFFC)")
    error = val.GetError()
    
    if error.Fail():
        print(f"FAILURE: cpu_read evaluation failed: {error}")
        return

    byte_val = val.GetValueAsUnsigned()
    print(f"Read $FFFC: {hex(byte_val)}")
    
    if byte_val != 0: # Assuming non-zero for valid ROM
        print("SUCCESS: cpu_read returned data")
    else:
        print("WARNING: cpu_read returned 0x00 (Possible but check if correct for this ROM)")
