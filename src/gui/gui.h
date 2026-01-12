#ifndef GUI_H
#define GUI_H

#include <stdbool.h>
#include <stdint.h>

#include <SDL2/SDL.h> // for SDL_Scancode

bool gui_init(void);
void gui_cleanup(void);
SDL_Scancode gui_poll_events(void);
bool gui_is_running(void);
void gui_update_framebuffer(const uint8_t *nes_framebuffer);
void gui_update_framebuffer(const uint8_t *nes_framebuffer);

// Forces a render present (useful for overlay updates)
void gui_render_present(void);

// Config Window Management
// Config Window Management (Deprecated/Removed in favor of Native Mac GUI)
// void gui_open_config_window(void);
// ...

// Emulator Control
void emulator_load_rom(const char *path);

#endif
