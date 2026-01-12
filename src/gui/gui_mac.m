#include "gui.h"
#include "input_config.h"
#include "platform_mac.h"
#import <Cocoa/Cocoa.h>

// Forward declaration
@interface KeyConfigWindowController : NSWindowController <NSWindowDelegate>
@end

static KeyConfigWindowController *configController = nil;

@interface KeyConfigWindowController ()
@property(strong) NSWindow *windowRef;
@property(strong) NSMutableArray<NSButton *> *buttons;
@property(assign) NSInteger bindingIndex;
@end

@implementation KeyConfigWindowController

- (instancetype)init {
  self = [super init];
  if (self) {
    self.bindingIndex = -1;
    self.buttons = [NSMutableArray array];
    [self createWindow];
  }
  return self;
}

- (void)createWindow {
  NSRect frame = NSMakeRect(0, 0, 300, 400);
  NSWindow *window = [[NSWindow alloc]
      initWithContentRect:frame
                styleMask:NSWindowStyleMaskTitled | NSWindowStyleMaskClosable
                  backing:NSBackingStoreBuffered
                    defer:NO];
  [window setTitle:@"Key Bindings"];
  [window center];
  [window setDelegate:self];
  self.window = window;
  self.windowRef = window;

  // Create UI
  NSArray *labels = @[
    @"Button A", @"Button B", @"Select", @"Start", @"Up", @"Down", @"Left",
    @"Right"
  ];
  int y = 350;

  for (int i = 0; i < labels.count; i++) {
    // Label
    NSTextField *label =
        [[NSTextField alloc] initWithFrame:NSMakeRect(20, y, 100, 24)];
    [label setStringValue:labels[i]];
    // [label setError:NO]; // Does not exist on standard NSTextField
    [label setBezeled:NO];
    [label setDrawsBackground:NO];
    [label setEditable:NO];
    [[window contentView] addSubview:label];

    // Button
    NSButton *btn =
        [[NSButton alloc] initWithFrame:NSMakeRect(130, y, 150, 24)];
    [btn setButtonType:NSButtonTypeMomentaryPushIn];
    [btn setBezelStyle:NSBezelStyleRounded];
    [btn setTarget:self];
    [btn setAction:@selector(btnClicked:)];
    [btn setTag:i];
    [[window contentView] addSubview:btn];
    [self.buttons addObject:btn];

    y -= 40;
  }

  [self updateButtonStates];

  // Instructions
  NSTextField *info =
      [[NSTextField alloc] initWithFrame:NSMakeRect(20, 20, 260, 40)];
  [info setStringValue:@"Click a button then press a key to rebind.\nChanges "
                       @"apply immediately."];
  [info setEditable:NO];
  [info setBezeled:NO];
  [info setDrawsBackground:NO];
  [info setFont:[NSFont systemFontOfSize:11]];
  [[window contentView] addSubview:info];
}

- (SDL_Scancode)getScancodeForIndex:(NSInteger)index {
  switch (index) {
  case 0:
    return current_keymap.key_a;
  case 1:
    return current_keymap.key_b;
  case 2:
    return current_keymap.key_select;
  case 3:
    return current_keymap.key_start;
  case 4:
    return current_keymap.key_up;
  case 5:
    return current_keymap.key_down;
  case 6:
    return current_keymap.key_left;
  case 7:
    return current_keymap.key_right;
  default:
    return SDL_SCANCODE_UNKNOWN;
  }
}

- (void)setScancode:(SDL_Scancode)code forIndex:(NSInteger)index {
  switch (index) {
  case 0:
    current_keymap.key_a = code;
    break;
  case 1:
    current_keymap.key_b = code;
    break;
  case 2:
    current_keymap.key_select = code;
    break;
  case 3:
    current_keymap.key_start = code;
    break;
  case 4:
    current_keymap.key_up = code;
    break;
  case 5:
    current_keymap.key_down = code;
    break;
  case 6:
    current_keymap.key_left = code;
    break;
  case 7:
    current_keymap.key_right = code;
    break;
  }
}

- (void)updateButtonStates {
  for (int i = 0; i < self.buttons.count; i++) {
    NSButton *btn = self.buttons[i];
    if (self.bindingIndex == i) {
      [btn setTitle:@"Press Key..."];
    } else {
      SDL_Scancode code = [self getScancodeForIndex:i];
      const char *name = SDL_GetScancodeName(code);
      [btn setTitle:[NSString stringWithUTF8String:name]];
    }
  }
}

- (void)btnClicked:(NSButton *)sender {
  self.bindingIndex = [sender tag];
  [self updateButtonStates];
  [self.window makeFirstResponder:nil]; // Clear focus so window gets KeyDown
}

- (void)keyDown:(NSEvent *)event {
  if (self.bindingIndex != -1) {
    unsigned short keyCode = [event keyCode];
    SDL_Scancode scancode = mac_keycode_to_sdl_scancode(keyCode);

    if (scancode != SDL_SCANCODE_UNKNOWN) {
      [self setScancode:scancode forIndex:self.bindingIndex];
      self.bindingIndex = -1;
      [self updateButtonStates];
    } else {
      // [super keyDown:event]; // NSWindowController doesn't implement keyDown,
      // passed up
    }
  } else {
    // [super keyDown:event]; // NSWindowController doesn't implement keyDown,
    // passed up
  }
}

- (void)flagsChanged:(NSEvent *)event {
  if (self.bindingIndex != -1) {
    unsigned short keyCode = [event keyCode];
    SDL_Scancode scancode = mac_keycode_to_sdl_scancode(keyCode);

    if (scancode != SDL_SCANCODE_UNKNOWN) {
      [self setScancode:scancode forIndex:self.bindingIndex];
      self.bindingIndex = -1;
      [self updateButtonStates];
    }
  }
}

- (void)windowWillClose:(NSNotification *)notification {
  configController = nil;
}

@end

// --- C Bridge ---

static void open_key_config(void) {
  if (!configController) {
    configController = [[KeyConfigWindowController alloc] init];
  }
  [configController showWindow:nil];
  [configController.window makeKeyAndOrderFront:nil];
  [NSApp activateIgnoringOtherApps:YES];
}

/*
void mac_init_menu(void) {
  NSMenu *mainMenu = [NSApp mainMenu];
  if (!mainMenu)
    return; // Should exist if SDL initialized

  // Add "NEStupid" -> "Key Bindings..."
  // SDL usually claims the first menu (App Menu).
  NSMenuItem *appIdx = [mainMenu itemAtIndex:0];
  NSMenu *appMenu = [appIdx submenu];

  [appMenu addItem:[NSMenuItem separatorItem]];

  NSMenuItem *item = [[NSMenuItem alloc] initWithTitle:@"Key Bindings..."
                                                action:@selector(openKeyConfig:)
                                         keyEquivalent:@"k"];

  // We need a target that responds to openKeyConfig.
  // Let's create a global delegate or hook app delegate.
  // Easier: Create a shim object instance held statically.
}
*/

// Minimal target for menu action
@interface MenuTarget : NSObject
- (void)openKeyConfig:(id)sender;
- (void)openROM:(id)sender;
@end

@implementation MenuTarget
- (void)openKeyConfig:(id)sender {
  open_key_config();
}

- (void)openROM:(id)sender {
  NSOpenPanel *panel = [NSOpenPanel openPanel];
  [panel setCanChooseFiles:YES];
  [panel setCanChooseDirectories:NO];
  [panel setAllowsMultipleSelection:NO];
  [panel setAllowedFileTypes:@[ @"nes", @"NES" ]];

  if ([panel runModal] == NSModalResponseOK) {
    NSURL *url = [[panel URLs] objectAtIndex:0];
    const char *path = [[url path] UTF8String];
    emulator_load_rom(path);

    // Bring main window to front?
    // SDL window usually stays in front, but just in case.
  }
}
@end

static MenuTarget *menuTarget = nil;

void mac_setup_menu_shim(void) {
  if (menuTarget)
    return;
  menuTarget = [[MenuTarget alloc] init];

  NSMenu *mainMenu = [NSApp mainMenu];
  if (!mainMenu)
    return;

  // Try to find app menu
  NSMenuItem *appItem = [mainMenu itemAtIndex:0];
  if (appItem) {
    NSMenu *appMenu = [appItem submenu];

    // Add Key Bindings
    [appMenu addItem:[NSMenuItem separatorItem]];
    NSMenuItem *item =
        [[NSMenuItem alloc] initWithTitle:@"Key Bindings..."
                                   action:@selector(openKeyConfig:)
                            keyEquivalent:@"k"];
    [item setTarget:menuTarget];
    [appMenu addItem:item];

    // Add "Open ROM..." to File Menu (or create it if missing)
    // Usually index 1 is File.
    NSMenu *fileMenu = nil;
    if ([mainMenu numberOfItems] > 1) {
      NSMenuItem *fileItem = [mainMenu itemAtIndex:1];
      if ([[fileItem title] isEqualToString:@"File"]) {
        fileMenu = [fileItem submenu];
      }
    }

    if (!fileMenu) {
      // Create minimal File menu if SDL didn't
      NSMenuItem *newItem = [[NSMenuItem alloc] initWithTitle:@"File"
                                                       action:nil
                                                keyEquivalent:@""];
      fileMenu = [[NSMenu alloc] initWithTitle:@"File"];
      [mainMenu insertItem:newItem atIndex:1];
      [mainMenu setSubmenu:fileMenu forItem:newItem];
    }

    NSMenuItem *openItem = [[NSMenuItem alloc] initWithTitle:@"Open ROM..."
                                                      action:@selector(openROM:)
                                               keyEquivalent:@"o"];
    [openItem setTarget:menuTarget];
    [fileMenu addItem:openItem];
  }
}

void mac_init_menu(void) {
  // Must be run on main thread?
  // SDL runs on main thread usually.
  mac_setup_menu_shim();
}
