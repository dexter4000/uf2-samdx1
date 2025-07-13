#include "uf2.h"

#ifdef BOARD_SCREEN

#ifdef USE_SCREEN_ST7789
// ST7789 specific functions
void st7789_early_init(void);
void st7789_init_display(void);
void st7789_draw_drag(void);
void st7789_show_usb_status(bool connected);
void st7789_update(void);
#endif

// Common screen interface functions
void screen_init(void) {
    #ifdef USE_SCREEN_ST7789
    st7789_init_display();
    #endif
    // If no specific screen driver, just continue without error
    draw_drag();
}

void screen_early_init(void) {
    #ifdef USE_SCREEN_ST7789
    st7789_early_init();
    #endif
    // If no specific screen driver, do nothing
}

void draw_drag(void) {
    #ifdef USE_SCREEN_ST7789
    st7789_draw_drag();
    #endif
    // If no specific screen driver, do nothing
}

void screen_update(void) {
    #ifdef USE_SCREEN_ST7789
    st7789_update();
    #endif
}

void screen_show_usb_status(bool connected) {
    #ifdef USE_SCREEN_ST7789
    st7789_show_usb_status(connected);
    #else
    (void)connected;
    #endif
}

#else

// Empty implementations when no screen
void screen_init(void) {}
void screen_early_init(void) {}
void draw_drag(void) {}
void screen_update(void) {}
void screen_show_usb_status(bool connected) { (void)connected; }

#endif