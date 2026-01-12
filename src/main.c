#include "cpu.h"
#include "gui.h"
#include "input.h"
#include "memory.h"
#include "ppu.h"
#include "rom.h"
#include <SDL2/SDL.h>
#include <stdio.h>
#include <stdlib.h>
#include <sys/stat.h>

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

int main(int argc, char *argv[]) {
  init_logging();
  printf("NEStupid - NES Emulator\n");

  if (argc < 2) {
    fprintf(stderr, "Usage: %s <rom_path>\n", argv[0]);
    return 1;
  }

  // Load ROM
  ROM *rom = rom_load(argv[1]);
  if (!rom) {
    fprintf(stderr, "Failed to load ROM: %s\n", argv[1]);
    return 1;
  }

  // Initialize Memory
  memory_init(rom);

  // Initialize PPU
  ppu_init(rom);
  ppu_reset();

  // Initialize CPU
  cpu_init();
  cpu_reset();

  // Initialize Input
  input_init();

  if (!gui_init()) {
    fprintf(stderr, "Failed to initialize GUI\n");
    rom_free(rom);
    return 1;
  }

  while (gui_is_running()) {
    // --- Input Handling ---
    gui_poll_events();

    const uint8_t *keys = SDL_GetKeyboardState(NULL);
    uint8_t buttons = 0;

    // Map SDL Keys to NES Buttons
    // A, B, Select, Start, Up, Down, Left, Right
    if (keys[SDL_SCANCODE_Z])
      buttons |= BUTTON_A;
    if (keys[SDL_SCANCODE_X])
      buttons |= BUTTON_B;
    if (keys[SDL_SCANCODE_RSHIFT] || keys[SDL_SCANCODE_LSHIFT])
      buttons |= BUTTON_SELECT;
    if (keys[SDL_SCANCODE_RETURN])
      buttons |= BUTTON_START;
    if (keys[SDL_SCANCODE_UP])
      buttons |= BUTTON_UP;
    if (keys[SDL_SCANCODE_DOWN])
      buttons |= BUTTON_DOWN;
    if (keys[SDL_SCANCODE_LEFT])
      buttons |= BUTTON_LEFT;
    if (keys[SDL_SCANCODE_RIGHT])
      buttons |= BUTTON_RIGHT;

    // Update Controller 1 (Standard)
    input_update(0, buttons);

    // --- Emulation Step ---
    // Run until one frame is complete
    while (!ppu_is_frame_complete()) {
      // CPU Step
      uint8_t cpu_cycles = cpu_step();

      // PPU Step (3x CPU cycles)
      for (int i = 0; i < cpu_cycles * 3; i++) {
        ppu_step();
      }
    }
    ppu_clear_frame_complete();

    // --- Audio Update (TODO) ---
    // apu_update();

    // --- Video Update ---
    gui_update_framebuffer(ppu_get_framebuffer());

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
  rom_free(rom);
  printf("NEStupid Exiting...\n");
  return 0;
}
