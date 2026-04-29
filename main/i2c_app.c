#include "i2c_app.h"

static i2c_master_bus_handle_t i2c_bus_handle = NULL;

esp_err_t i2c_app_master_init(i2c_master_dev_handle_t *oled_dev_handle_out)
{
    if (oled_dev_handle_out == NULL)
    {
        return ESP_ERR_INVALID_ARG;
    }

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