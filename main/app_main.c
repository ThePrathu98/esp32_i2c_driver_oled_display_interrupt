/*
 * Project objective:
 * Convert the ESP-IDF GPIO interrupt example into:
 *
 * GPIO interrupt
 *      ↓
 * ISR sends event to FreeRTOS queue
 *      ↓
 * display_task receives event
 *      ↓
 * display_task updates OLED over I2C
 *
 * Important real-time rule:
 * ISR must NOT call I2C or OLED functions.
 * ISR only sends an event to the queue and exits quickly.
 */

#include <stdio.h>
#include <stdint.h>
#include <inttypes.h>

#include "freertos/FreeRTOS.h"
#include "freertos/task.h"
#include "freertos/queue.h"

#include "driver/gpio.h"
#include "driver/i2c_master.h"

#include "esp_err.h"
#include "esp_system.h"

#include "i2c_app.h"
#include "oled.h"


/*
 * GPIO loopback test:
 *
 * sdkconfig values:
 * CONFIG_GPIO_OUTPUT_0 = 18
 * CONFIG_GPIO_OUTPUT_1 = 19
 * CONFIG_GPIO_INPUT_0  = 4
 * CONFIG_GPIO_INPUT_1  = 5
 *
 * Required wiring:
 * GPIO18 → GPIO4
 * GPIO19 → GPIO5
 *
 * GPIO18/19 generate pulses.
 * GPIO4/5 receive those pulses and trigger interrupts.
 */
#define GPIO_OUTPUT_IO_0     CONFIG_GPIO_OUTPUT_0
#define GPIO_OUTPUT_IO_1     CONFIG_GPIO_OUTPUT_1
#define GPIO_INPUT_IO_0      CONFIG_GPIO_INPUT_0
#define GPIO_INPUT_IO_1      CONFIG_GPIO_INPUT_1

#define GPIO_OUTPUT_PIN_SEL  ((1ULL << GPIO_OUTPUT_IO_0) | (1ULL << GPIO_OUTPUT_IO_1))
#define GPIO_INPUT_PIN_SEL   ((1ULL << GPIO_INPUT_IO_0)  | (1ULL << GPIO_INPUT_IO_1))

#define ESP_INTR_FLAG_DEFAULT 0

typedef struct
{
    uint32_t gpio_num;
    uint32_t gpio_level;
} gpio_event_t;

static QueueHandle_t gpio_evt_queue = NULL;

static void IRAM_ATTR gpio_isr_handler(void *arg)
{
    uint32_t gpio_num = (uint32_t)(uintptr_t)arg;

    gpio_event_t event = {
        .gpio_num = gpio_num,
        .gpio_level = 0
    };

    BaseType_t higher_priority_task_woken = pdFALSE;

    BaseType_t queue_status = xQueueSendFromISR(gpio_evt_queue,
                                                &event,
                                                &higher_priority_task_woken);

    if (queue_status != pdPASS)
    {
        /*
         * Do not print inside ISR.
         * In a real project, you could increment an ISR drop counter here.
         */
    }

    if (higher_priority_task_woken == pdTRUE)
    {
        portYIELD_FROM_ISR();
    }
}

static void display_task(void *arg)
{
    gpio_event_t event;
    uint32_t event_count = 0;

    for (;;)
    {
        BaseType_t queue_status = xQueueReceive(gpio_evt_queue,
                                                &event,
                                                portMAX_DELAY);

        if (queue_status == pdPASS)
        {
            event.gpio_level = (uint32_t)gpio_get_level(event.gpio_num);
            event_count++;

            uint8_t count_bar = (uint8_t)(event_count % OLED_WIDTH);

            if (count_bar == 0)
            {
                count_bar = OLED_WIDTH;
            }

            esp_err_t ret = oled_clear();

            if (ret != ESP_OK)
            {
                printf("ERROR: oled_clear failed: %d\n", ret);
                continue;
            }

            ret = oled_draw_bar(0, count_bar);

            if (ret != ESP_OK)
            {
                printf("ERROR: oled_draw_bar page 0 failed: %d\n", ret);
                continue;
            }

            ret = oled_draw_bar(2, (uint8_t)(event.gpio_num * 4));

            if (ret != ESP_OK)
            {
                printf("ERROR: oled_draw_bar page 2 failed: %d\n", ret);
                continue;
            }

            ret = oled_draw_bar(4, event.gpio_level ? 100 : 30);

            if (ret != ESP_OK)
            {
                printf("ERROR: oled_draw_bar page 4 failed: %d\n", ret);
                continue;
            }

            printf("EVT#%" PRIu32 " GPIO=%" PRIu32 " LEVEL=%" PRIu32 " OLED=UPDATED\n",
                   event_count,
                   event.gpio_num,
                   event.gpio_level);
        }
    }
}

void app_main(void)
{
    gpio_config_t io_conf = {};

    io_conf.intr_type = GPIO_INTR_DISABLE;
    io_conf.mode = GPIO_MODE_OUTPUT;
    io_conf.pin_bit_mask = GPIO_OUTPUT_PIN_SEL;
    io_conf.pull_down_en = GPIO_PULLDOWN_DISABLE;
    io_conf.pull_up_en = GPIO_PULLUP_DISABLE;

    esp_err_t ret = gpio_config(&io_conf);

    if (ret != ESP_OK)
    {
        printf("ERROR: output gpio_config failed: %d\n", ret);
        return;
    }

    io_conf.intr_type = GPIO_INTR_POSEDGE;
    io_conf.mode = GPIO_MODE_INPUT;
    io_conf.pin_bit_mask = GPIO_INPUT_PIN_SEL;
    io_conf.pull_down_en = GPIO_PULLDOWN_DISABLE;
    io_conf.pull_up_en = GPIO_PULLUP_ENABLE;

    ret = gpio_config(&io_conf);

    if (ret != ESP_OK)
    {
        printf("ERROR: input gpio_config failed: %d\n", ret);
        return;
    }

    ret = gpio_set_intr_type(GPIO_INPUT_IO_0, GPIO_INTR_ANYEDGE);

    if (ret != ESP_OK)
    {
        printf("ERROR: gpio_set_intr_type failed: %d\n", ret);
        return;
    }

    gpio_evt_queue = xQueueCreate(10, sizeof(gpio_event_t));

    if (gpio_evt_queue == NULL)
    {
        printf("ERROR: Failed to create GPIO event queue\n");
        return;
    }

    printf("I2C config: SDA=%d, SCL=%d, FREQ=%d, OLED_ADDR=0x%02X\n",
           I2C_MASTER_SDA_IO,
           I2C_MASTER_SCL_IO,
           I2C_MASTER_FREQ_HZ,
           OLED_I2C_ADDR);

    i2c_master_dev_handle_t oled_dev_handle = NULL;

    ret = i2c_app_master_init(&oled_dev_handle);

    if (ret != ESP_OK)
    {
        printf("ERROR: i2c_app_master_init failed: %d\n", ret);
        return;
    }

    ret = oled_init(oled_dev_handle);

    if (ret != ESP_OK)
    {
        printf("ERROR: oled_init failed: %d\n", ret);
        return;
    }

    ret = oled_clear();

    if (ret != ESP_OK)
    {
        printf("ERROR: oled_clear failed: %d\n", ret);
        return;
    }

    ret = oled_draw_bar(0, 20);

    if (ret != ESP_OK)
    {
        printf("ERROR: oled_draw_bar failed: %d\n", ret);
        return;
    }

    BaseType_t task_status = xTaskCreate(display_task,
                                         "display_task",
                                         4096,
                                         NULL,
                                         3,
                                         NULL);

    if (task_status != pdPASS)
    {
        printf("ERROR: Failed to create display_task\n");
        return;
    }

    ret = gpio_install_isr_service(ESP_INTR_FLAG_DEFAULT);

    if (ret != ESP_OK)
    {
        printf("ERROR: gpio_install_isr_service failed: %d\n", ret);
        return;
    }

    ret = gpio_isr_handler_add(GPIO_INPUT_IO_0,
                               gpio_isr_handler,
                               (void *)(uintptr_t)GPIO_INPUT_IO_0);

    if (ret != ESP_OK)
    {
        printf("ERROR: gpio_isr_handler_add GPIO_INPUT_IO_0 failed: %d\n", ret);
        return;
    }

    ret = gpio_isr_handler_add(GPIO_INPUT_IO_1,
                               gpio_isr_handler,
                               (void *)GPIO_INPUT_IO_1);

    if (ret != ESP_OK)
    {
        printf("ERROR: gpio_isr_handler_add GPIO_INPUT_IO_1 failed: %d\n", ret);
        return;
    }

    printf("Minimum free heap size: %" PRIu32 " bytes\n",
           esp_get_minimum_free_heap_size());

    printf("GPIO interrupt + queue + OLED demo started\n");

    int cnt = 0;

    while (1)
    {
        printf("cnt: %d\n", cnt++);

        vTaskDelay(pdMS_TO_TICKS(1000));

        ret = gpio_set_level(GPIO_OUTPUT_IO_0, cnt % 2);

        if (ret != ESP_OK)
        {
            printf("ERROR: gpio_set_level output 0 failed: %d\n", ret);
        }

        ret = gpio_set_level(GPIO_OUTPUT_IO_1, cnt % 2);

        if (ret != ESP_OK)
        {
            printf("ERROR: gpio_set_level output 1 failed: %d\n", ret);
        }
    }
}

/*

Suggestion 6:
Github repository:

Ask codec to convert workspace folder into github repository with 
proper .gitignore, README.md, and LICENSE files.

After creating new repo in github.com/import repo,

Email nitish link of github repo, (along with connection diagram photo, video google drive link access shared)
and ask him to clone it, run it, and verify it works as expected.


try and implement timer based ISR's instead of GPIO based ISRs, to show that the same display_task can handle events 
from different sources.
*/