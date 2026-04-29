idf_component_register(
    SRCS "oled.c" "oled_cmd_table.c" "i2c_app.c" "app_main.c"
    INCLUDE_DIRS "."
    REQUIRES esp_driver_gpio esp_driver_i2c
)
