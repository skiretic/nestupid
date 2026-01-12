#include "input_config.h"

KeyMap current_keymap;

void input_config_set_defaults(void) {
  current_keymap.key_a = SDL_SCANCODE_Z;
  current_keymap.key_b = SDL_SCANCODE_X;
  current_keymap.key_select = SDL_SCANCODE_RSHIFT;
  current_keymap.key_start = SDL_SCANCODE_RETURN;
  current_keymap.key_up = SDL_SCANCODE_UP;
  current_keymap.key_down = SDL_SCANCODE_DOWN;
  current_keymap.key_left = SDL_SCANCODE_LEFT;
  current_keymap.key_right = SDL_SCANCODE_RIGHT;
  current_keymap.key_menu = SDL_SCANCODE_TAB;
}

void input_config_init(void) { input_config_set_defaults(); }

const char *input_config_get_key_name(SDL_Scancode key) {
  return SDL_GetScancodeName(key);
}
