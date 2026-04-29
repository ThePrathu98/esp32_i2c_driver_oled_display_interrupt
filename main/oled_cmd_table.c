#include "oled_cmd_table.h"

/*
 * SSD1306 initialization command table.
 *
 * Keeping commands in a table avoids repeated oled_write_cmd() calls in
 * oled_init() and makes the startup sequence easier to review or modify.
 */
const uint8_t oled_init_cmd_table[] = {
    0x20, 0x00,   /* Memory addressing mode: horizontal */
    0xB0,         /* Page start address */
    0xC8,         /* COM output scan direction */
    0x00,         /* Low column address */
    0x10,         /* High column address */
    0x40,         /* Start line address */
    0x81, 0x7F,   /* Contrast control */
    0xA1,         /* Segment remap */
    0xA6,         /* Normal display */
    0xA8, 0x3F,   /* Multiplex ratio */
    0xA4,         /* Display follows RAM content */
    0xD3, 0x00,   /* Display offset */
    0xD5, 0x80,   /* Display clock divide */
    0xD9, 0xF1,   /* Pre-charge period */
    0xDA, 0x12,   /* COM pins configuration */
    0xDB, 0x40,   /* VCOMH deselect level */
    0x8D, 0x14    /* Charge pump enable */
};

/*
 * Number of commands in the table.
 * sizeof(array) / sizeof(array[0]) is the standard C way to count elements.
 */
const size_t oled_init_cmd_table_size =
    sizeof(oled_init_cmd_table) / sizeof(oled_init_cmd_table[0]);