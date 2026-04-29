#ifndef I2C_APP_H
#define I2C_APP_H

#include "esp_err.h"
#include "driver/i2c_master.h"

/*
 * I2C/OLED configuration used by the whole project.
 * These are kept in the header so app_main.c can print the same values.
 */
#define I2C_MASTER_SCL_IO     22
#define I2C_MASTER_SDA_IO     21
#define I2C_MASTER_FREQ_HZ    100000
#define OLED_I2C_ADDR         0x3C

/*
 * Initializes the I2C bus and returns the OLED device handle through
 * oled_dev_handle_out.
 */
esp_err_t i2c_app_master_init(i2c_master_dev_handle_t *oled_dev_handle_out);

#endif