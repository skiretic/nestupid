#ifndef PLATFORM_MAC_H
#define PLATFORM_MAC_H

#include <SDL2/SDL.h>

// Initialize Mac Menu
void mac_init_menu(void);

// Convert Mac KeyCode to SDL Scancode
SDL_Scancode mac_keycode_to_sdl_scancode(unsigned short key_code);

#endif
