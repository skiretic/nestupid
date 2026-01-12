import lldb

def __lldb_init_module(debugger, internal_dict):
    debugger.HandleCommand('command script add -f test_cpu_exec.test_cpu_exec test_cpu_exec')

def test_cpu_exec(debugger, command, result, internal_dict):
    target = debugger.GetSelectedTarget()
    process = target.GetProcess()
    thread = process.GetSelectedThread()
    frame = thread.GetSelectedFrame()
    
    # Locate cpu struct
    cpu_val = frame.FindVariable("cpu")
    if not cpu_val.IsValid():
        # Try finding it global or in parent
        # 'cpu' is static in cpu.c, so it might be harder to find by name within main.
        # But we can access it via cpu_get_state probably if we call it?
        # Or look for formatting symbols.
        # Since it's static in cpu.c, we might need to be in cpu frame or use global lookup.
        target = debugger.GetSelectedTarget()
        module = target.GetModuleAtIndex(0)
        # Try to find symbol by name
        # If static, might be cpu.c:cpu or similar
        pass

    # Better: Use expression to interact with emulator
    # 1. Load nestest.nes (Passed via args in launch)
    # 2. Set PC = 0xC000
    frame.EvaluateExpression("cpu.pc = 0xC000")
    
    # 3. Step CPU for N cycles/instructions
    # We can't easily loop inside LLDB python script efficiently calling cpu_step() 1000 times
    # one by one via expressions (slow).
    # But we can call a loop in C if we had one, or just continue and break.
    
    # Let's execute cpu_step() 100 times and print PC
    print("Running 100 instructions...")
    for i in range(100):
        # Call cpu_step
        val = frame.EvaluateExpression("(uint8_t)cpu_step()")
        if val.GetError().Fail():
             print(f"Error calling cpu_step: {val.GetError()}")
             break
        
        # Check PC
        pc_val = frame.EvaluateExpression("cpu.pc").GetValueAsUnsigned()
        print(f"Inst {i+1}: PC=${hex(pc_val)}")
        
        # nestest starts at C000.
        # First inst is 4C F5 C5 (JMP $C5F5).
        # So after 1 step, PC should be C5F5.
        if i == 0:
            if pc_val == 0xC5F5:
                print("SUCCESS: JMP $C5F5 executed correctly.")
            else:
                print(f"FAILURE: Expected PC=$C5F5, got {hex(pc_val)}")
                # Read opcode at C000 to debug
                dbg = frame.EvaluateExpression("(uint8_t)cpu_read(0xC000)").GetValueAsUnsigned()
                print(f"Opcode at $C000: {hex(dbg)}")

    print("CPU Test finished.")
