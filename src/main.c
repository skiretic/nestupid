#include "apu.h"
#include "cpu.h"
#include "gui.h"
#include "input.h"
#include "input_config.h"
#include "memory.h"
#include "ppu.h"
#include "rom.h"
#include <SDL2/SDL.h>
#include <stdbool.h>
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

  bool headless = false;
  for (int i = 1; i < argc; i++) {
    if (strcmp(argv[i], "--headless") == 0) {
      headless = true;
    }
  }

  if (!headless) {
    if (!gui_init()) {
      fprintf(stderr, "Failed to initialize GUI\n");
      // return 1; // Don't crash if GUI fails in headless (though here we are
      // !headless) Actually if !headless and gui fails, we should return.
      return 1;
    }
  } else {
    printf("Running in Headless Mode\n");
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

  // --- Main Loop ---
  while (true) {
    if (!headless) {
      if (!gui_is_running())
        break;

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
    }

    // --- Emulation Step ---
    if (current_rom) {
      while (!ppu_is_frame_complete()) {
        uint8_t cpu_cycles = cpu_step();
        for (int i = 0; i < cpu_cycles * 3; i++) {
          ppu_step();
        }
        for (int i = 0; i < cpu_cycles; i++) {
          apu_step();
        }
      }
      ppu_clear_frame_complete();
    }

    if (!headless) {
      // --- Video Update ---
      gui_update_framebuffer(ppu_get_framebuffer());

      // Final Present
      gui_render_present();

      // --- Timing (Simple Cap at 60 FPS) ---
      static uint64_t last_time = 0;
      uint64_t current_time = SDL_GetTicks64();
      uint64_t elapsed = current_time - last_time;
      if (elapsed < 16) {
        SDL_Delay(16 - elapsed);
      }
      last_time = SDL_GetTicks64();
    } else {
      // Headless timing (uncapped or simple delay to prevent 100% CPU usage
      // loop if rom not loaded - though rom should be loaded) If ROM is loaded,
      // we just loop as fast as possible for tests? Actually blargg tests might
      // take a few seconds. Let's just yield a tiny bit or let it burn. If we
      // want to exit automatically, we need a condition. For now, let user kill
      // it or rely on timeout.

      // We can add a "frame limit" for headless or just let it run.
      // For debugging, faster is better.
    }
  }

  gui_cleanup();
  if (current_rom)
    rom_free(current_rom);
  printf("NEStupid Exiting...\n");
  return 0;
}
