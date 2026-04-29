#ifndef OLED_H
#define OLED_H

#include <stdint.h>
#include <stddef.h>

#include "esp_err.h"
#include "driver/i2c_master.h"

#define OLED_WIDTH              128
#define OLED_HEIGHT             64

#define OLED_CONTROL_BYTE_CMD   0x00
#define OLED_CONTROL_BYTE_DATA  0x40

esp_err_t oled_init(i2c_master_dev_handle_t dev_handle);
esp_err_t oled_clear(void);
esp_err_t oled_set_cursor(uint8_t page, uint8_t col);
esp_err_t oled_draw_bar(uint8_t page, uint8_t width);

#endif