#include "gui.h"
#include "ppu.h"
#include <SDL2/SDL.h>
#include <stdio.h>

#define WINDOW_WIDTH 256
#define WINDOW_HEIGHT 240
#define SCALE 3

static SDL_Window *window = NULL;
static SDL_Renderer *renderer = NULL;
static SDL_Texture *texture = NULL;
static bool running = false;

bool gui_init(void) {
  if (SDL_Init(SDL_INIT_VIDEO | SDL_INIT_AUDIO) < 0) {
    printf("SDL Init failed: %s\n", SDL_GetError());
    return false;
  }

  window = SDL_CreateWindow("NEStupid", SDL_WINDOWPOS_CENTERED,
                            SDL_WINDOWPOS_CENTERED, WINDOW_WIDTH * SCALE,
                            WINDOW_HEIGHT * SCALE,
                            SDL_WINDOW_SHOWN | SDL_WINDOW_ALLOW_HIGHDPI);
  if (!window) {
    printf("Window creation failed: %s\n", SDL_GetError());
    return false;
  }

  renderer = SDL_CreateRenderer(
      window, -1, SDL_RENDERER_ACCELERATED | SDL_RENDERER_PRESENTVSYNC);
  if (!renderer) {
    printf("Renderer creation failed: %s\n", SDL_GetError());
    return false;
  }

  // Logical size for 256x240 scaling
  SDL_RenderSetLogicalSize(renderer, WINDOW_WIDTH, WINDOW_HEIGHT);

  texture = SDL_CreateTexture(renderer, SDL_PIXELFORMAT_ARGB8888,
                              SDL_TEXTUREACCESS_STREAMING, WINDOW_WIDTH,
                              WINDOW_HEIGHT);
  if (!texture) {
    printf("Texture creation failed: %s\n", SDL_GetError());
    return false;
  }

  running = true;
  return true;
}

void gui_cleanup(void) {
  if (texture)
    SDL_DestroyTexture(texture);
  if (renderer)
    SDL_DestroyRenderer(renderer);
  if (window)
    SDL_DestroyWindow(window);
  SDL_Quit();
}

void gui_poll_events(void) {
  SDL_Event event;
  while (SDL_PollEvent(&event)) {
    if (event.type == SDL_QUIT) {
      running = false;
    }
  }
}

bool gui_is_running(void) { return running; }

// NES 2C02 Palette (RGB)
static const uint32_t palette[64] = {
    0x7C7C7C, 0x0000FC, 0x0000BC, 0x4428BC, 0x940084, 0xA80020, 0xA81000,
    0x881400, 0x503000, 0x007800, 0x006800, 0x005800, 0x004058, 0x000000,
    0x000000, 0x000000, 0xBCBCBC, 0x0078F8, 0x0058F8, 0x6844FC, 0xD800CC,
    0xE40058, 0xF83800, 0xE45C10, 0xAC7C00, 0x00B800, 0x00A800, 0x00A844,
    0x008888, 0x000000, 0x000000, 0x000000, 0xF8F8F8, 0x3CBCFC, 0x6888FC,
    0x9878F8, 0xF878F8, 0xF85898, 0xF87858, 0xFCA044, 0xF8B800, 0xB8F818,
    0x58D854, 0x58F898, 0x00E8D8, 0x787878, 0x000000, 0x000000, 0xFCFCFC,
    0xA4E4FC, 0xB8B8F8, 0xD8B8F8, 0xF8B8F8, 0xF8A4C0, 0xF0D0B0, 0xFCE0A8,
    0xF8D878, 0xD8F878, 0xB8F8B8, 0xB8F8D8, 0x00FCFC, 0xF8D8F8, 0x000000,
    0x000000,
};

void gui_update_framebuffer(const uint8_t *nes_framebuffer) {
  if (!nes_framebuffer)
    return;

  void *pixels;
  int pitch;

  if (SDL_LockTexture(texture, NULL, &pixels, &pitch) == 0) {
    uint32_t *dst = (uint32_t *)pixels;
    for (int i = 0; i < WINDOW_WIDTH * WINDOW_HEIGHT; i++) {
      uint8_t index = nes_framebuffer[i] & 0x3F;
      dst[i] = palette[index] | 0xFF000000;
    }
    SDL_UnlockTexture(texture);

  } else {
    printf("[GUI] Failed to lock texture: %s\n", SDL_GetError());
  }

  SDL_RenderClear(renderer);
  SDL_RenderCopy(renderer, texture, NULL, NULL);
  SDL_RenderPresent(renderer);

  static int frame_count = 0;
  if (frame_count++ % 60 == 0) {
    // Dump Palette
    const uint8_t *pal = ppu_get_palette(); // Implicitly need declaration in
                                            // gui.c or include ppu.h
    // But gui.c doesn't include ppu.h usually? Let's assume we can add the
    // include or just declare extern. Better to check ppu.h include. Assuming
    // we can access it via gui_update_framebuffer caller? No, let's just
    // inspect it here.

    // Need ppu.h included in gui.c? It is not currently.
    // I will add the include in a separate step or just assume current context.
    // Let's rely on the user running this.
    // Check if palette is empty
    bool empty = true;
    for (int i = 0; i < 32; i++) {
      if (pal[i] != 0)
        empty = false;
    }
    printf("Palette State: %s\n", empty ? "EMPTY (All 0s)" : "HAS DATA");
    if (!empty) {
      for (int i = 0; i < 32; i++)
        printf("%02X ", pal[i]);
      printf("\n");
    }
  }
}
