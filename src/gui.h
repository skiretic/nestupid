#ifndef GUI_H
#define GUI_H

#include <stdbool.h>
#include <stdint.h>

bool gui_init(void);
void gui_cleanup(void);
void gui_poll_events(void);
bool gui_is_running(void);
void gui_update_framebuffer(const uint8_t *nes_framebuffer);

#endif
