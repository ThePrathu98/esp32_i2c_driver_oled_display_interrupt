#ifndef OLED_CMD_TABLE_H
#define OLED_CMD_TABLE_H

#include <stdint.h>
#include <stddef.h>

/*
 * Extern means the table is defined in oled_cmd_table.c but can be used by
 * oled.c without creating a second copy.
 */
extern const uint8_t oled_init_cmd_table[];
extern const size_t oled_init_cmd_table_size;

#endif