#ifndef DISPLAY_H_
#define DISPLAY_H_

#include <stdint.h>

// Color definitions
#define COLOR_BLACK   0x0000
#define COLOR_BLUE    0x001F
#define COLOR_RED     0xF800
#define COLOR_GREEN   0x07E0
#define COLOR_CYAN    0x07FF
#define COLOR_MAGENTA 0xF81F
#define COLOR_YELLOW  0xFFE0
#define COLOR_WHITE   0xFFFF

// Display dimensions
#define DISPLAY_WIDTH   320
#define DISPLAY_HEIGHT  240

// Opaque handle for panel (just void pointer)
typedef void* esp_lcd_panel_handle_t;

/**
 * Initialize the display
 * Returns: panel_handle on success, NULL on failure
 */
esp_lcd_panel_handle_t display_init(void);

/**
 * Draw a filled rectangle on the display
 */
void display_fill_rect(int16_t x, int16_t y, int16_t w, int16_t h, uint16_t color);

/**
 * Fill the entire screen with a color
 */
void display_fill_screen(uint16_t color);

/**
 * Draw a single pixel
 */
void display_draw_pixel(int16_t x, int16_t y, uint16_t color);

/**
 * Draw a single character at position (x, y) using 5x8 font
 * size: 1 = normal (5x8), 2 = double (10x16), etc.
 */
void display_draw_char(int16_t x, int16_t y, char c, uint16_t color, uint16_t bg, uint8_t size);

/**
 * Draw a text string at position (x, y)
 * size: 1 = normal, 2 = double size, etc.
 * Use bg=COLOR_BLACK for transparent background
 */
void display_draw_text(int16_t x, int16_t y, const char *text, uint16_t color, uint16_t bg, uint8_t size);

/**
 * Get panel handle for direct access if needed
 */
esp_lcd_panel_handle_t display_get_panel(void);

#endif // DISPLAY_H_
