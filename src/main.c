#include "cpu.h"
#include "gui.h"
#include "input.h"
#include "input_config.h"
#include "memory.h"
#include "ppu.h"
#include "rom.h"
#include <SDL2/SDL.h>
#include <stdio.h>
#include <stdlib.h>
#include <sys/stat.h>

#ifdef __APPLE__
#include "platform_mac.h"
#endif

void init_logging(void) {
  // Ensure logs directory exists
  struct stat st = {0};
  if (stat("logs", &st) == -1) {
    mkdir("logs", 0700);
  }

  // Open a log file
  FILE *log_file = fopen("logs/startup.log", "w");
  if (log_file) {
    fprintf(log_file, "NEStupid Starting...\n");
    fclose(log_file);
  }
}

static ROM *current_rom = NULL;

void emulator_load_rom(const char *path) {
  if (current_rom) {
    rom_free(current_rom);
    current_rom = NULL;
  }

  current_rom = rom_load(path);
  if (!current_rom) {
    fprintf(stderr, "Failed to load ROM: %s\n", path);
    return;
  }

  // Re-initialize subsystems with new ROM
  memory_init(current_rom);
  ppu_init(current_rom); // PPU needs ROM for mirroring/CHR
  ppu_reset();
  cpu_init();
  cpu_reset();

  printf("ROM Loaded: %s\n", path);
}

int main(int argc, char *argv[]) {
  init_logging();
  printf("NEStupid - NES Emulator\n");

  // Initialize Input (Independent of ROM)
  input_init();
  input_config_init();

  if (!gui_init()) {
    fprintf(stderr, "Failed to initialize GUI\n");
    return 1;
  }

  // Load ROM from CLI if provided
  if (argc > 1) {
    emulator_load_rom(argv[1]);
  } else {
    printf("No ROM provided. Waiting for GUI load...\n");
  }

#ifdef __APPLE__
  mac_init_menu();
#endif

  while (gui_is_running()) {
    // --- Input Handling ---
    SDL_Scancode pressed = gui_poll_events();

    // --- Game Update ---
    const uint8_t *keys = SDL_GetKeyboardState(NULL);
    uint8_t buttons = 0;

    // Map SDL Keys to NES Buttons
    if (keys[current_keymap.key_a])
      buttons |= BUTTON_A;
    if (keys[current_keymap.key_b])
      buttons |= BUTTON_B;
    if (keys[current_keymap.key_select])
      buttons |= BUTTON_SELECT;
    if (keys[current_keymap.key_start])
      buttons |= BUTTON_START;
    if (keys[current_keymap.key_up])
      buttons |= BUTTON_UP;
    if (keys[current_keymap.key_down])
      buttons |= BUTTON_DOWN;
    if (keys[current_keymap.key_left])
      buttons |= BUTTON_LEFT;
    if (keys[current_keymap.key_right])
      buttons |= BUTTON_RIGHT;

    input_update(0, buttons);

    // --- Emulation Step ---
    if (current_rom) {
      while (!ppu_is_frame_complete()) {
        uint8_t cpu_cycles = cpu_step();
        for (int i = 0; i < cpu_cycles * 3; i++) {
          ppu_step();
        }
      }
      ppu_clear_frame_complete();
    }

    // --- Audio Update (TODO) ---
    // apu_update();

    // --- Video Update ---
    // Render Game
    gui_update_framebuffer(ppu_get_framebuffer());

    // --- Video Update ---
    gui_update_framebuffer(ppu_get_framebuffer());

    // Final Present
    gui_render_present();

    // --- Timing (Simple Cap at 60 FPS) ---
    // 60 FPS = 16.67ms per frame
    static uint64_t last_time = 0;
    uint64_t current_time = SDL_GetTicks64();
    uint64_t elapsed = current_time - last_time;
    if (elapsed < 16) {
      SDL_Delay(16 - elapsed);
    }
    last_time = SDL_GetTicks64();
  }

  gui_cleanup();
  if (current_rom)
    rom_free(current_rom);
  printf("NEStupid Exiting...\n");
  return 0;
}
