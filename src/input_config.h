#ifndef INPUT_CONFIG_H
#define INPUT_CONFIG_H

#include <SDL2/SDL.h>

typedef struct {
  SDL_Scancode key_a;
  SDL_Scancode key_b;
  SDL_Scancode key_select;
  SDL_Scancode key_start;
  SDL_Scancode key_up;
  SDL_Scancode key_down;
  SDL_Scancode key_left;
  SDL_Scancode key_right;
  SDL_Scancode key_menu; // To toggle menu
} KeyMap;

extern KeyMap current_keymap;

void input_config_init(void);
void input_config_set_defaults(void);

// Helper to get name of key
const char *input_config_get_key_name(SDL_Scancode key);

#endif // INPUT_CONFIG_H
