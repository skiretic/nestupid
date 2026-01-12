import lldb

def __lldb_init_module(debugger, internal_dict):
    debugger.HandleCommand('command script add -f test_ppu_loop.test_ppu_loop test_ppu_loop')

def test_ppu_loop(debugger, command, result, internal_dict):
    target = debugger.GetSelectedTarget()
    process = target.GetProcess()
    thread = process.GetSelectedThread()
    frame = thread.GetSelectedFrame()
    
    print("Testing PPU Loop...")
    
    # 1. Reset PPU (should be done by init)
    # Check scanline/dot = 0
    sl = frame.EvaluateExpression("ppu.scanline").GetValueAsUnsigned()
    dot = frame.EvaluateExpression("ppu.dot").GetValueAsUnsigned()
    print(f"Start: Scanline={sl}, Dot={dot}")
    
    # 2. Run for enough cycles to complete a scanline (341 dots)
    # CPU does ~114 cycles per scanline.
    # We can't step instruction by instruction easily here without helper.
    # Let's run a few CPU steps.
    
    # Run 200 CPU steps (should be ~600-1000 PPU dots, multiple scanlines)
    for i in range(200):
        frame.EvaluateExpression("(void)cpu_step()")
        # We also need to manually step PPU if we are calling cpu_step() strictly from python?
        # WAIT. logic in main.c is: cpu_step() returns cycles. LOOP ppu_step().
        # If I call cpu_step() from LLDB, main loop code DOES NOT RUN. 
        # So PPU won't step unless I call it!
        
        # NOTE: My test_cpu_exec.py called cpu_step(). It did NOT call ppu_step().
        # So PPU state wouldn't advance in that test.
        # Here I must simulate the loop logic or call a function that does both.
        # Since I don't have a 'system_step()' function exposed, I'll simulate it.
        
        cycles = frame.EvaluateExpression("(uint8_t)cpu_step()").GetValueAsUnsigned()
        for p in range(cycles * 3):
            frame.EvaluateExpression("(void)ppu_step()")
            
    # 3. Check Scanline advanced
    sl_end = frame.EvaluateExpression("ppu.scanline").GetValueAsUnsigned()
    print(f"End: Scanline={sl_end}")
    
    if sl_end > 0:
        print("SUCCESS: Scanline advanced.")
    else:
        print("FAILURE: Scanline did not advance.")

    # 4. Check VBlank flag set (Need to run ~30000 PPU dots -> ~10000 CPU cycles / ~3000 insts)
    # Too slow for LLDB python loop.
    # Just checking scanline advance is enough for now.
