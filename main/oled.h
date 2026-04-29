#ifndef OLED_H
#define OLED_H

#include <stdint.h>
#include <stddef.h>

#include "esp_err.h"
#include "driver/i2c_master.h"

/*
 * SSD1306 display size used by this project.
 * 128 columns x 64 rows = 8 pages of 8 pixels each.
 */
#define OLED_WIDTH              128
#define OLED_HEIGHT             64

/*
 * SSD1306 I2C control bytes.
 * 0x00 = following byte is a command.
 * 0x40 = following bytes are display data.
 */
#define OLED_CONTROL_BYTE_CMD   0x00
#define OLED_CONTROL_BYTE_DATA  0x40

/*
 * Public OLED API used by app_main.c.
 * Low-level write_cmd/write_data functions stay private inside oled.c.
 */
esp_err_t oled_init(i2c_master_dev_handle_t dev_handle);
esp_err_t oled_clear(void);
esp_err_t oled_set_cursor(uint8_t page, uint8_t col);
esp_err_t oled_draw_bar(uint8_t page, uint8_t width);

#endif