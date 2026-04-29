#include <string.h>

#include "freertos/FreeRTOS.h"
#include "freertos/task.h"

#include "oled.h"
#include "oled_cmd_table.h"

/*
 * Private OLED device handle.
 * oled_init() stores the handle here so the remaining OLED functions can use it.
 */
static i2c_master_dev_handle_t oled_dev_handle = NULL;

/*
 * Sends one SSD1306 command byte.
 * Control byte 0x00 tells the OLED that the next byte is a command.
 */
static esp_err_t oled_write_cmd(uint8_t cmd)
{
    if (oled_dev_handle == NULL)
    {
        return ESP_ERR_INVALID_STATE;
    }

    uint8_t data[2] = {
        OLED_CONTROL_BYTE_CMD,
        cmd
    };

    esp_err_t ret = i2c_master_transmit(oled_dev_handle,
                                        data,
                                        sizeof(data),
                                        1000);

    if (ret != ESP_OK)
    {
        return ret;
    }

    return ESP_OK;
}

/*
 * Sends display data bytes to the OLED.
 * Control byte 0x40 tells the OLED that following bytes are pixel data.
 */
static esp_err_t oled_write_data(const uint8_t *data, size_t len)
{
    if (oled_dev_handle == NULL || data == NULL)
    {
        return ESP_ERR_INVALID_ARG;
    }

    /*
     * One OLED page is 128 columns.
     * Extra byte at index 0 is the SSD1306 control byte.
     */
    uint8_t buffer[OLED_WIDTH + 1];

    if (len > OLED_WIDTH)
    {
        len = OLED_WIDTH;
    }

    buffer[0] = OLED_CONTROL_BYTE_DATA;
    memcpy(&buffer[1], data, len);

    esp_err_t ret = i2c_master_transmit(oled_dev_handle,
                                        buffer,
                                        len + 1,
                                        1000);

    if (ret != ESP_OK)
    {
        return ret;
    }

    return ESP_OK;
}

/*
 * Initializes the SSD1306 OLED.
 * The main init sequence is stored in oled_cmd_table.c and sent using a loop.
 */
esp_err_t oled_init(i2c_master_dev_handle_t dev_handle)
{
    if (dev_handle == NULL)
    {
        return ESP_ERR_INVALID_ARG;
    }

    oled_dev_handle = dev_handle;

    vTaskDelay(pdMS_TO_TICKS(100));

    esp_err_t ret = oled_write_cmd(0xAE);   /* Display OFF */

    if (ret != ESP_OK)
    {
        return ret;
    }

    vTaskDelay(pdMS_TO_TICKS(20));

    for (size_t i = 0; i < oled_init_cmd_table_size; i++)
    {
        ret = oled_write_cmd(oled_init_cmd_table[i]);

        if (ret != ESP_OK)
        {
            return ret;
        }
    }

    vTaskDelay(pdMS_TO_TICKS(20));

    ret = oled_write_cmd(0xAF);   /* Display ON */

    if (ret != ESP_OK)
    {
        return ret;
    }

    vTaskDelay(pdMS_TO_TICKS(20));

    return ESP_OK;
}

/*
 * Clears all 8 OLED pages.
 * 128x64 OLED = 8 pages, each page is 8 pixels high and 128 columns wide.
 */
esp_err_t oled_clear(void)
{
    uint8_t zeros[OLED_WIDTH];
    memset(zeros, 0x00, sizeof(zeros));

    esp_err_t ret = ESP_OK;

    for (uint8_t page = 0; page < 8; page++)
    {
        ret = oled_write_cmd((uint8_t)(0xB0 + page));

        if (ret != ESP_OK)
        {
            return ret;
        }

        ret = oled_write_cmd(0x00);

        if (ret != ESP_OK)
        {
            return ret;
        }

        ret = oled_write_cmd(0x10);

        if (ret != ESP_OK)
        {
            return ret;
        }

        ret = oled_write_data(zeros, sizeof(zeros));

        if (ret != ESP_OK)
        {
            return ret;
        }
    }

    return ESP_OK;
}

/*
 * Sets the current OLED page and column.
 * page selects vertical 8-pixel row group, col selects horizontal position.
 */
esp_err_t oled_set_cursor(uint8_t page, uint8_t col)
{
    if (page > 7 || col >= OLED_WIDTH)
    {
        return ESP_ERR_INVALID_ARG;
    }

    esp_err_t ret = oled_write_cmd((uint8_t)(0xB0 + page));

    if (ret != ESP_OK)
    {
        return ret;
    }

    /*
     * SSD1306 column address is split into lower nibble and upper nibble.
     * Bit masking extracts the required 4-bit parts.
     */
    ret = oled_write_cmd((uint8_t)(0x00 + (col & 0x0F)));

    if (ret != ESP_OK)
    {
        return ret;
    }

    ret = oled_write_cmd((uint8_t)(0x10 + ((col >> 4) & 0x0F)));

    if (ret != ESP_OK)
    {
        return ret;
    }

    return ESP_OK;
}

/*
 * Draws a simple horizontal bar on one OLED page.
 * Each 0xFF byte turns on 8 vertical pixels in that column.
 */
esp_err_t oled_draw_bar(uint8_t page, uint8_t width)
{
    uint8_t data[OLED_WIDTH];
    memset(data, 0x00, sizeof(data));

    if (width > OLED_WIDTH)
    {
        width = OLED_WIDTH;
    }

    for (uint8_t i = 0; i < width; i++)
    {
        data[i] = 0xFF;
    }

    esp_err_t ret = oled_set_cursor(page, 0);

    if (ret != ESP_OK)
    {
        return ret;
    }

    ret = oled_write_data(data, sizeof(data));

    if (ret != ESP_OK)
    {
        return ret;
    }

    return ESP_OK;
}