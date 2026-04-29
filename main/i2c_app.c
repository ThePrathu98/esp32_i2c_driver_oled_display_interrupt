#include "i2c_app.h"

/*
 * Keep the bus handle private to this file.
 * Other modules only need the OLED device handle, not the whole I2C bus object.
 */
static i2c_master_bus_handle_t i2c_bus_handle = NULL;

/*
 * Initializes ESP32 as an I2C master and registers the OLED as a device.
 *
 * oled_dev_handle_out is an output parameter. The caller receives this handle
 * and passes it to the OLED driver.
 */
esp_err_t i2c_app_master_init(i2c_master_dev_handle_t *oled_dev_handle_out)
{
    if (oled_dev_handle_out == NULL)
    {
        return ESP_ERR_INVALID_ARG;
    }

    /*
     * Designated initializer syntax sets only the fields we need.
     * GPIO21 = SDA, GPIO22 = SCL, and internal pull-ups are enabled for I2C.
     */
    i2c_master_bus_config_t bus_config = {
        .clk_source = I2C_CLK_SRC_DEFAULT,
        .i2c_port = I2C_NUM_0,
        .scl_io_num = I2C_MASTER_SCL_IO,
        .sda_io_num = I2C_MASTER_SDA_IO,
        .glitch_ignore_cnt = 7,
        .flags.enable_internal_pullup = true,
    };

    esp_err_t ret = i2c_new_master_bus(&bus_config, &i2c_bus_handle);

    if (ret != ESP_OK)
    {
        return ret;
    }

    /*
     * Add the SSD1306 OLED as a 7-bit I2C device.
     * 100 kHz is used for reliable breadboard/jumper-wire testing.
     */
    i2c_device_config_t oled_config = {
        .dev_addr_length = I2C_ADDR_BIT_LEN_7,
        .device_address = OLED_I2C_ADDR,
        .scl_speed_hz = I2C_MASTER_FREQ_HZ,
    };

    ret = i2c_master_bus_add_device(i2c_bus_handle,
                                    &oled_config,
                                    oled_dev_handle_out);

    if (ret != ESP_OK)
    {
        return ret;
    }

    return ESP_OK;
}