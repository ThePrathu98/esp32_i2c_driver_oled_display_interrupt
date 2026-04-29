# esp_interrupt_oled_i2c_gpio

ESP-IDF demo project for ESP32 that routes GPIO interrupts into a FreeRTOS queue and updates an SSD1306 OLED over I2C from a task context.

## Overview

This project starts from the GPIO interrupt example pattern and extends it with an OLED status display.

- GPIO18 and GPIO19 generate test pulses
- GPIO4 and GPIO5 receive those pulses as interrupts
- The ISR posts compact events to a FreeRTOS queue
- A display task reads the queue and updates a 128x64 SSD1306 OLED

The design keeps interrupt work short and avoids calling I2C or OLED routines inside the ISR.

## Hardware

- ESP32 development board
- SSD1306 128x64 I2C OLED display
- Two jumper wires for GPIO loopback testing

## Pin Map

| Signal | GPIO | Purpose |
| --- | --- | --- |
| Output 0 | GPIO18 | Test pulse output |
| Output 1 | GPIO19 | Test pulse output |
| Input 0 | GPIO4 | Interrupt input, rising and falling edge |
| Input 1 | GPIO5 | Interrupt input, rising edge |
| OLED SDA | GPIO21 | I2C data |
| OLED SCL | GPIO22 | I2C clock |

## Wiring

Loopback connections:

```text
GPIO18 -> GPIO4
GPIO19 -> GPIO5
```

OLED connections:

```text
GPIO21 -> OLED SDA
GPIO22 -> OLED SCL
3.3V   -> OLED VCC
GND    -> OLED GND
```

Default OLED settings are defined in [`main/i2c_app.h`](main/i2c_app.h) as:

- I2C address: `0x3C`
- Clock speed: `100000`

If your module uses address `0x3D`, update `OLED_I2C_ADDR` before flashing.

## Build And Flash

Prerequisites:

- ESP-IDF installed and configured
- ESP32 target selected
- Board connected over USB

Example CLI flow:

```bash
idf.py set-target esp32
idf.py build
idf.py -p <PORT> flash monitor
```

If you use VS Code with the ESP-IDF extension, open the folder and use the build, flash, and monitor commands from the extension UI.

## Project Layout

- `main/app_main.c`: GPIO setup, ISR, FreeRTOS queue, and display task
- `main/i2c_app.c` and `main/i2c_app.h`: I2C master bus setup for the OLED
- `main/oled.c` and `main/oled.h`: SSD1306 command and drawing helpers
- `main/oled_cmd_table.c` and `main/oled_cmd_table.h`: OLED initialization command table
- `main/Kconfig.projbuild`: configurable GPIO defaults

## Repository Notes

- `build/` is ignored because it contains generated ESP-IDF artifacts
- `sdkconfig` is ignored so local machine configuration stays out of Git
- `.vscode/` is ignored because it currently contains machine-specific settings such as the COM port and local tool paths

## License

This project is licensed under the MIT License. See [`LICENSE`](LICENSE) for details.
