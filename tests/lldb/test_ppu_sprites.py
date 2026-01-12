import lldb

def __lldb_init_module(debugger, internal_dict):
    debugger.HandleCommand('command script add -f test_ppu_sprites.test_ppu_sprites test_ppu_sprites')

def test_ppu_sprites(debugger, command, result, internal_dict):
    target = debugger.GetSelectedTarget()
    process = target.GetProcess()
    thread = process.GetSelectedThread()
    frame = thread.GetSelectedFrame()
    
    print("Testing PPU Sprites (Optimized)...")
    
    # 1. Write OAM Data
    # Sprite 0: Y=10, Tile=1, Attr=0, X=50
    frame.EvaluateExpression("(void)cpu_write(0x2003, 0x00)") 
    frame.EvaluateExpression("(void)cpu_write(0x2004, 10)")   
    frame.EvaluateExpression("(void)cpu_write(0x2004, 1)")    
    frame.EvaluateExpression("(void)cpu_write(0x2004, 0)")    
    frame.EvaluateExpression("(void)cpu_write(0x2004, 50)")   

    # Sprite 1: Y=20, Tile=2, Attr=0, X=60
    frame.EvaluateExpression("(void)cpu_write(0x2004, 20)")   
    frame.EvaluateExpression("(void)cpu_write(0x2004, 2)")    
    frame.EvaluateExpression("(void)cpu_write(0x2004, 0)")    
    frame.EvaluateExpression("(void)cpu_write(0x2004, 60)")   
    
    # 2. Enable Rendering
    frame.EvaluateExpression("(void)cpu_write(0x2001, 0x10)") 
    
    # 3. Set Conditional Breakpoint at ppu_step
    # Condition: ppu.scanline == 10 && ppu.dot == 257 (Start of Sprite Eval/Fetch check)
    print("Setting Breakpoint at Scanline 10, Dot 257...")
    bp = target.BreakpointCreateByName("ppu_step")
    bp.SetCondition("ppu.scanline == 10 && ppu.dot == 257")
    
    # 4. Continue Execution
    # This runs the emulator loop normally, which calls ppu_step 3x faster than python
    print("Continuing execution...")
    process.Continue()
    
    # We should have hit the breakpoint
    frame = thread.GetSelectedFrame()
    sl = frame.EvaluateExpression("ppu.scanline").GetValueAsUnsigned()
    dot = frame.EvaluateExpression("ppu.dot").GetValueAsUnsigned()
    print(f"Stopped at Scanline {sl}, Dot {dot}")
    
    if sl == 10 and dot == 257:
        print("SUCCESS: Reached Scanline 10.")
    else:
        print("FAILURE: Did not stop at expected condition (Timeout or Error?).")
        # Proceed with checks anyway if close?
        
    # Execute one PPU step to run the Eval/Fetch logic at 257
    thread.StepOver() 
    # Or just Evaluate ppu_step
    # Note: ppu_step() function is what we are IN. 
    # We stopped AT entry (line 313/314).
    # Logic for dot 257 is inside. We need to step active frame.
    
    # Let's just Finish the function?
    # Or StepInto until we are past the logic.
    # The logic is: "if (dot==257) { ... }"
    # We are at start of function.
    # We need to execute the function body.
    thread.StepOut() # Finish ppu_step
    
    # Now check effects
    count = frame.EvaluateExpression("ppu.sprite_count").GetValueAsUnsigned()
    print(f"Sprite Count: {count}")
    
    if count >= 1:
        print("SUCCESS: Sprite found (Count >= 1).")
    else:
        print("FAILURE: No sprites found.")
        
    y0 = frame.EvaluateExpression("ppu.secondary_oam[0]").GetValueAsUnsigned()
    print(f"SecOAM[0].Y: {y0}")
    if y0 == 10:
        print("SUCCESS: Sprite 0 Y copied correctly.")
    else:
        print(f"FAILURE: Expected Y=10, got {y0}")
