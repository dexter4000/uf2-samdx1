#include "uf2.h"

#ifndef BOARD_SCREEN

void screen_early_init(void) {}
void screen_init(void) {}
void screen_update(void) {}
void screen_show_usb_status(bool connected) { (void)connected; }
void draw_drag(void) {}
void draw_screen(void) {}
void draw_hf2(void) {}

#endif