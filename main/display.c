#include <stdio.h>
#include <stdlib.h>
#include "freertos/FreeRTOS.h"
#include "freertos/task.h"
#include "driver/spi_master.h"
#include "driver/gpio.h"
#include "esp_log.h"
#include "display.h"

#define TAG "DISPLAY"

// TTGO T4 v1.3 Pin Configuration
#define PIN_MISO    12
#define PIN_MOSI    23
#define PIN_CLK     18
#define PIN_CS      27
#define PIN_DC      32
#define PIN_RST     5
#define PIN_BL      4

#define LCD_HOST    SPI2_HOST
#define LCD_H_RES   320
#define LCD_V_RES   240
#define LCD_PIXEL_CLOCK_HZ  (26 * 1000 * 1000)

static spi_device_handle_t spi_handle = NULL;

// Forward declarations
static void lcd_write_cmd(uint8_t cmd);
static void lcd_write_data(const uint8_t *data, int len);
static void lcd_init_cmds(void);

/**
 * Write a command to the LCD
 */
static void lcd_write_cmd(uint8_t cmd)
{
    gpio_set_level(PIN_DC, 0);  // DC = 0 for command
    spi_transaction_t t = {
        .length = 8,
        .tx_buffer = &cmd,
    };
    spi_device_polling_transmit(spi_handle, &t);
}

/**
 * Write data to the LCD
 */
static void lcd_write_data(const uint8_t *data, int len)
{
    if (len == 0) return;
    
    gpio_set_level(PIN_DC, 1);  // DC = 1 for data
    spi_transaction_t t = {
        .length = len * 8,
        .tx_buffer = data,
    };
    spi_device_polling_transmit(spi_handle, &t);
}

/**
 * Initialize LCD controller with commands
 */
static void lcd_init_cmds(void)
{
    // Hardware reset
    gpio_set_level(PIN_RST, 0);
    vTaskDelay(pdMS_TO_TICKS(100));
    gpio_set_level(PIN_RST, 1);
    vTaskDelay(pdMS_TO_TICKS(100));

    // ILI9341 initialization sequence
    lcd_write_cmd(0xCF);
    lcd_write_data((uint8_t[]){0x00, 0x83, 0x30}, 3);

    lcd_write_cmd(0xED);
    lcd_write_data((uint8_t[]){0x64, 0x03, 0x12, 0x81}, 4);

    lcd_write_cmd(0xE8);
    lcd_write_data((uint8_t[]){0x85, 0x01, 0x79}, 3);

    lcd_write_cmd(0xCB);
    lcd_write_data((uint8_t[]){0x39, 0x2C, 0x00, 0x34, 0x02}, 5);

    lcd_write_cmd(0xF7);
    lcd_write_data((uint8_t[]){0x20}, 1);

    lcd_write_cmd(0xEA);
    lcd_write_data((uint8_t[]){0x00, 0x00}, 2);

    lcd_write_cmd(0xC0);  // Power control 1
    lcd_write_data((uint8_t[]){0x26}, 1);

    lcd_write_cmd(0xC1);  // Power control 2
    lcd_write_data((uint8_t[]){0x11}, 1);

    lcd_write_cmd(0xC5);  // VCOM control 1
    lcd_write_data((uint8_t[]){0x35, 0x3E}, 2);

    lcd_write_cmd(0xC7);  // VCOM control 2
    lcd_write_data((uint8_t[]){0xBE}, 1);

    lcd_write_cmd(0x36);  // Memory access control
    lcd_write_data((uint8_t[]){0x28}, 1);

    lcd_write_cmd(0x3A);  // Pixel format set (16-bit)
    lcd_write_data((uint8_t[]){0x55}, 1);

    lcd_write_cmd(0xB1);  // Frame rate control
    lcd_write_data((uint8_t[]){0x00, 0x18}, 2);

    lcd_write_cmd(0xB6);  // Display function control
    lcd_write_data((uint8_t[]){0x08, 0x82, 0x27}, 3);

    lcd_write_cmd(0xF2);  // Enable 3G
    lcd_write_data((uint8_t[]){0x00}, 1);

    lcd_write_cmd(0x26);  // Gamma set
    lcd_write_data((uint8_t[]){0x01}, 1);

    // Gamma correction
    lcd_write_cmd(0xE0);
    lcd_write_data((uint8_t[]){0x0F, 0x31, 0x2B, 0x0C, 0x0E, 0x08, 0x4E, 0xF1, 0x37, 0x07, 0x10, 0x03, 0x0E, 0x09, 0x00}, 15);

    lcd_write_cmd(0xE1);
    lcd_write_data((uint8_t[]){0x00, 0x0E, 0x14, 0x03, 0x11, 0x07, 0x31, 0xC1, 0x48, 0x08, 0x0F, 0x0C, 0x31, 0x36, 0x0F}, 15);

    // Exit sleep mode
    lcd_write_cmd(0x11);
    vTaskDelay(pdMS_TO_TICKS(120));

    // Display ON
    lcd_write_cmd(0x29);
    vTaskDelay(pdMS_TO_TICKS(50));

    ESP_LOGI(TAG, "LCD initialization commands completed");
}

/**
 * Set the drawing area (window) on the display
 */
static void lcd_set_window(int xs, int ys, int xe, int ye)
{
    // Column address set
    lcd_write_cmd(0x2A);
    uint8_t data[4] = {(xs >> 8) & 0xFF, xs & 0xFF, (xe >> 8) & 0xFF, xe & 0xFF};
    lcd_write_data(data, 4);

    // Row address set
    lcd_write_cmd(0x2B);
    data[0] = (ys >> 8) & 0xFF;
    data[1] = ys & 0xFF;
    data[2] = (ye >> 8) & 0xFF;
    data[3] = ye & 0xFF;
    lcd_write_data(data, 4);

    // Memory write
    lcd_write_cmd(0x2C);
}

/**
 * Initialize the display
 */
esp_lcd_panel_handle_t display_init(void)
{
    // Initialize GPIO pins
    gpio_set_direction(PIN_DC, GPIO_MODE_OUTPUT);
    gpio_set_direction(PIN_RST, GPIO_MODE_OUTPUT);
    gpio_set_direction(PIN_BL, GPIO_MODE_OUTPUT);
    
    // Initialize SPI bus
    spi_bus_config_t buscfg = {
        .sclk_io_num = PIN_CLK,
        .mosi_io_num = PIN_MOSI,
        .miso_io_num = PIN_MISO,
        .quadwp_io_num = -1,
        .quadhd_io_num = -1,
        .max_transfer_sz = 4096,  // Use smaller transfer size
    };
    
    ESP_ERROR_CHECK(spi_bus_initialize(LCD_HOST, &buscfg, SPI_DMA_CH_AUTO));
    ESP_LOGI(TAG, "SPI bus initialized");

    // Attach SPI device
    spi_device_interface_config_t devcfg = {
        .mode = 0,
        .clock_speed_hz = LCD_PIXEL_CLOCK_HZ,
        .spics_io_num = PIN_CS,
        .queue_size = 7,
    };
    
    ESP_ERROR_CHECK(spi_bus_add_device(LCD_HOST, &devcfg, &spi_handle));
    ESP_LOGI(TAG, "SPI device added");

    // Initialize LCD
    lcd_init_cmds();

    // Turn on backlight
    gpio_set_level(PIN_BL, 1);
    ESP_LOGI(TAG, "Display initialized successfully");

    return (esp_lcd_panel_handle_t)spi_handle;
}

/**
 * Draw a filled rectangle on the display
 */
void display_fill_rect(int16_t x, int16_t y, int16_t w, int16_t h, uint16_t color)
{
    if (spi_handle == NULL) return;
    if (w <= 0 || h <= 0) return;
    
    // Clamp coordinates
    if (x < 0) x = 0;
    if (y < 0) y = 0;
    if (x + w > LCD_H_RES) w = LCD_H_RES - x;
    if (y + h > LCD_V_RES) h = LCD_V_RES - y;
    
    if (w <= 0 || h <= 0) return;

    lcd_set_window(x, y, x + w - 1, y + h - 1);

    // Swap bytes for big-endian SPI
    uint16_t color_swap = ((color << 8) & 0xFF00) | ((color >> 8) & 0xFF);

    uint16_t *row_buf = (uint16_t *)malloc(w * sizeof(uint16_t));
    if (row_buf == NULL) return;

    for (int i = 0; i < w; i++) {
        row_buf[i] = color_swap;
    }

    for (int row = 0; row < h; row++) {
        lcd_write_data((const uint8_t *)row_buf, w * 2);
    }

    free(row_buf);
}

/**
 * Fill the entire screen with a color
 */
void display_fill_screen(uint16_t color)
{
    display_fill_rect(0, 0, LCD_H_RES, LCD_V_RES, color);
}

/**
 * Draw a single pixel
 */
void display_draw_pixel(int16_t x, int16_t y, uint16_t color)
{
    if (spi_handle == NULL) return;
    if (x < 0 || x >= LCD_H_RES || y < 0 || y >= LCD_V_RES) return;

    lcd_set_window(x, y, x, y);
    
    // Swap bytes for big-endian SPI
    uint16_t color_swap = ((color << 8) & 0xFF00) | ((color >> 8) & 0xFF);
    lcd_write_data((const uint8_t *)&color_swap, 2);
}

/**
 * Get SPI handle for direct access
 */
esp_lcd_panel_handle_t display_get_panel(void)
{
    return (esp_lcd_panel_handle_t)spi_handle;
}
/**
 * Simple 5x8 pixel font (ASCII 32-126)
 * Each character is 5 pixels wide, 8 pixels tall
 */
static const uint8_t simple_font[95][5] = {
    {0x00, 0x00, 0x00, 0x00, 0x00},  // space
    {0x04, 0x04, 0x04, 0x00, 0x04},  // !
    {0x0A, 0x0A, 0x00, 0x00, 0x00},  // "
    {0x0A, 0x1F, 0x0A, 0x1F, 0x0A},  // #
    {0x0E, 0x14, 0x0E, 0x05, 0x0E},  // $
    {0x11, 0x09, 0x04, 0x12, 0x11},  // %
    {0x0C, 0x12, 0x0C, 0x12, 0x0D},  // &
    {0x04, 0x04, 0x00, 0x00, 0x00},  // '
    {0x02, 0x04, 0x04, 0x04, 0x02},  // (
    {0x08, 0x04, 0x04, 0x04, 0x08},  // )
    {0x04, 0x15, 0x0E, 0x15, 0x04},  // *
    {0x04, 0x04, 0x0E, 0x04, 0x04},  // +
    {0x00, 0x00, 0x00, 0x04, 0x08},  // ,
    {0x00, 0x00, 0x0E, 0x00, 0x00},  // -
    {0x00, 0x00, 0x00, 0x00, 0x04},  // .
    {0x01, 0x02, 0x04, 0x08, 0x10},  // /
    {0x0E, 0x11, 0x13, 0x15, 0x0E},  // 0
    {0x04, 0x0C, 0x04, 0x04, 0x0E},  // 1
    {0x0E, 0x11, 0x02, 0x04, 0x1F},  // 2
    {0x0E, 0x11, 0x06, 0x11, 0x0E},  // 3
    {0x08, 0x0C, 0x0A, 0x1F, 0x08},  // 4
    {0x1F, 0x10, 0x0E, 0x01, 0x0E},  // 5
    {0x06, 0x08, 0x0E, 0x11, 0x0E},  // 6
    {0x1F, 0x01, 0x02, 0x04, 0x08},  // 7
    {0x0E, 0x11, 0x0E, 0x11, 0x0E},  // 8
    {0x0E, 0x11, 0x0F, 0x01, 0x0C},  // 9
    {0x00, 0x04, 0x00, 0x04, 0x00},  // :
    {0x00, 0x04, 0x00, 0x04, 0x08},  // ;
    {0x02, 0x04, 0x08, 0x04, 0x02},  // <
    {0x00, 0x0E, 0x00, 0x0E, 0x00},  // =
    {0x08, 0x04, 0x02, 0x04, 0x08},  // >
    {0x0E, 0x11, 0x02, 0x00, 0x02},  // ?
    {0x0E, 0x15, 0x17, 0x10, 0x0E},  // @
    {0x0E, 0x11, 0x1F, 0x11, 0x11},  // A
    {0x1E, 0x11, 0x1E, 0x11, 0x1E},  // B
    {0x0E, 0x10, 0x10, 0x10, 0x0E},  // C
    {0x1E, 0x11, 0x11, 0x11, 0x1E},  // D
    {0x1F, 0x10, 0x1E, 0x10, 0x1F},  // E
    {0x1F, 0x10, 0x1E, 0x10, 0x10},  // F
    {0x0E, 0x10, 0x13, 0x11, 0x0E},  // G
    {0x11, 0x11, 0x1F, 0x11, 0x11},  // H
    {0x0E, 0x04, 0x04, 0x04, 0x0E},  // I
    {0x07, 0x02, 0x02, 0x12, 0x0C},  // J
    {0x11, 0x12, 0x1C, 0x12, 0x11},  // K
    {0x10, 0x10, 0x10, 0x10, 0x1F},  // L
    {0x11, 0x1B, 0x15, 0x11, 0x11},  // M
    {0x11, 0x19, 0x15, 0x13, 0x11},  // N
    {0x0E, 0x11, 0x11, 0x11, 0x0E},  // O
    {0x1E, 0x11, 0x1E, 0x10, 0x10},  // P
    {0x0E, 0x11, 0x11, 0x12, 0x0D},  // Q
    {0x1E, 0x11, 0x1E, 0x12, 0x11},  // R
    {0x0E, 0x10, 0x0E, 0x01, 0x0E},  // S
    {0x1F, 0x04, 0x04, 0x04, 0x04},  // T
    {0x11, 0x11, 0x11, 0x11, 0x0E},  // U
    {0x11, 0x11, 0x0A, 0x0A, 0x04},  // V
    {0x11, 0x11, 0x15, 0x1B, 0x11},  // W
    {0x11, 0x0A, 0x04, 0x0A, 0x11},  // X
    {0x11, 0x0A, 0x04, 0x04, 0x04},  // Y
    {0x1F, 0x02, 0x04, 0x08, 0x1F},  // Z
    {0x0E, 0x08, 0x08, 0x08, 0x0E},  // [
    {0x10, 0x08, 0x04, 0x02, 0x01},  /* backslash */
    {0x0E, 0x02, 0x02, 0x02, 0x0E},  // ]
    {0x04, 0x0A, 0x11, 0x00, 0x00},  // ^
    {0x00, 0x00, 0x00, 0x00, 0x1F},  // _
    {0x08, 0x04, 0x02, 0x00, 0x00},  // `
    {0x00, 0x0E, 0x01, 0x0F, 0x0E},  // a
    {0x10, 0x1E, 0x11, 0x11, 0x0E},  // b
    {0x00, 0x0E, 0x10, 0x10, 0x0E},  // c
    {0x01, 0x0F, 0x11, 0x11, 0x0E},  // d
    {0x00, 0x0E, 0x1F, 0x10, 0x0E},  // e
    {0x06, 0x08, 0x0E, 0x08, 0x08},  // f
    {0x0E, 0x11, 0x0F, 0x01, 0x0E},  // g
    {0x10, 0x1E, 0x11, 0x11, 0x11},  // h
    {0x04, 0x00, 0x04, 0x04, 0x04},  // i
    {0x02, 0x00, 0x02, 0x02, 0x0C},  // j
    {0x10, 0x12, 0x0C, 0x12, 0x11},  // k
    {0x0C, 0x04, 0x04, 0x04, 0x0E},  // l
    {0x00, 0x1A, 0x15, 0x15, 0x11},  // m
    {0x00, 0x1E, 0x11, 0x11, 0x11},  // n
    {0x00, 0x0E, 0x11, 0x11, 0x0E},  // o
    {0x1E, 0x11, 0x1E, 0x10, 0x10},  // p
    {0x0F, 0x11, 0x0F, 0x01, 0x01},  // q
    {0x00, 0x1E, 0x10, 0x10, 0x10},  // r
    {0x00, 0x0E, 0x10, 0x01, 0x0E},  // s
    {0x08, 0x0E, 0x08, 0x08, 0x06},  // t
    {0x00, 0x11, 0x11, 0x11, 0x0E},  // u
    {0x00, 0x11, 0x0A, 0x0A, 0x04},  // v
    {0x00, 0x11, 0x15, 0x1B, 0x11},  // w
    {0x00, 0x11, 0x0A, 0x04, 0x0A},  // x
    {0x11, 0x0A, 0x04, 0x08, 0x10},  // y
    {0x00, 0x1F, 0x04, 0x08, 0x1F},  // z
    {0x06, 0x04, 0x08, 0x04, 0x06},  // {
    {0x04, 0x04, 0x04, 0x04, 0x04},  // |
    {0x0C, 0x04, 0x02, 0x04, 0x0C},  // }
    {0x08, 0x15, 0x02, 0x00, 0x00},  // ~
};

/**
 * Draw a single character at position (x, y) using 5x8 font
 * size: 1 = normal (5x8), 2 = double (10x16), etc.
 */
void display_draw_char(int16_t x, int16_t y, char c, uint16_t color, uint16_t bg, uint8_t size)
{
    if (c < 32 || c > 126) return;  // Only printable ASCII
    
    const uint8_t *glyph = simple_font[c - 32];
    
    for (int8_t row = 0; row < 8; row++) {
        uint8_t bitmap = glyph[row < 5 ? row : 4];
        
        for (int8_t col = 0; col < 5; col++) {
            uint16_t draw_color = (bitmap & (1 << (4 - col))) ? color : bg;
            
            if (size == 1) {
                display_draw_pixel(x + col, y + row, draw_color);
            } else {
                // Draw scaled character
                for (int8_t dx = 0; dx < size; dx++) {
                    for (int8_t dy = 0; dy < size; dy++) {
                        display_draw_pixel(x + col * size + dx, y + row * size + dy, draw_color);
                    }
                }
            }
        }
    }
}

/**
 * Draw a text string at position (x, y)
 * size: 1 = normal, 2 = double size, etc.
 */
void display_draw_text(int16_t x, int16_t y, const char *text, uint16_t color, uint16_t bg, uint8_t size)
{
    if (text == NULL) return;
    
    int16_t curr_x = x;
    const int char_width = 6 * size;  // ~5 pixels + 1 spacing
    
    while (*text) {
        display_draw_char(curr_x, y, *text, color, bg, size);
        curr_x += char_width;
        
        if (curr_x >= LCD_H_RES - 10) break;  // Don't draw off screen
        text++;
    }
}