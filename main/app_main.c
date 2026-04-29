/*
 * ESP32 GPIO Interrupt + FreeRTOS Queue + I2C OLED Demo
 *
 * System flow:
 * GPIO18/GPIO19 generate test pulses.
 * GPIO4/GPIO5 receive those pulses and trigger GPIO interrupts.
 * The ISR sends a small event to a FreeRTOS queue.
 * display_task receives the event and updates the OLED over I2C.
 *
 * Important rule:
 * Do not call OLED/I2C functions inside the ISR. I2C is slower and should run
 * in task context, not interrupt context.
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
 * GPIO loopback setup:
 * GPIO18 -> GPIO4
 * GPIO19 -> GPIO5
 *
 * Output pins generate software test pulses.
 * Input pins receive those pulses and create interrupt events.
 */
#define GPIO_OUTPUT_IO_0     CONFIG_GPIO_OUTPUT_0
#define GPIO_OUTPUT_IO_1     CONFIG_GPIO_OUTPUT_1
#define GPIO_INPUT_IO_0      CONFIG_GPIO_INPUT_0
#define GPIO_INPUT_IO_1      CONFIG_GPIO_INPUT_1

/*
 * Bit mask syntax:
 * 1ULL creates a 64-bit value. Shifting it by the GPIO number selects that pin.
 * ESP-IDF GPIO config uses one bit mask to configure multiple pins at once.
 */
#define GPIO_OUTPUT_PIN_SEL  ((1ULL << GPIO_OUTPUT_IO_0) | (1ULL << GPIO_OUTPUT_IO_1))
#define GPIO_INPUT_PIN_SEL   ((1ULL << GPIO_INPUT_IO_0)  | (1ULL << GPIO_INPUT_IO_1))

#define ESP_INTR_FLAG_DEFAULT 0

/*
 * Event passed from ISR to task.
 * gpio_num tells which pin interrupted.
 * gpio_level is filled later in task context to keep ISR short.
 */
typedef struct
{
    uint32_t gpio_num;
    uint32_t gpio_level;
} gpio_event_t;

/* Queue handle used for ISR-to-task communication. */
static QueueHandle_t gpio_evt_queue = NULL;

/*
 * GPIO interrupt handler.
 *
 * IRAM_ATTR places the ISR in instruction RAM so it can run reliably during
 * interrupt execution. The ISR only sends an event to the queue and exits.
 */
static void IRAM_ATTR gpio_isr_handler(void *arg)
{
    /*
     * GPIO number was passed as a pointer argument.
     * uintptr_t is used for safe pointer-to-integer conversion.
     */
    uint32_t gpio_num = (uint32_t)(uintptr_t)arg;

    gpio_event_t event = {
        .gpio_num = gpio_num,
        .gpio_level = 0
    };

    BaseType_t higher_priority_task_woken = pdFALSE;

    /*
     * xQueueSendFromISR() is the ISR-safe queue API.
     * It may wake display_task if that task is waiting on the queue.
     */
    BaseType_t queue_status = xQueueSendFromISR(gpio_evt_queue,
                                                &event,
                                                &higher_priority_task_woken);

    if (queue_status != pdPASS)
    {
        /*
         * Do not print inside ISR.
         * In a real project, increment an error/drop counter here.
         */
    }

    /*
     * If sending to the queue woke a higher-priority task, request a context
     * switch immediately after ISR exits.
     */
    if (higher_priority_task_woken == pdTRUE)
    {
        portYIELD_FROM_ISR();
    }
}

/*
 * Task that owns OLED updates.
 *
 * This task blocks on the queue. It wakes only when the ISR sends an event.
 * OLED/I2C work is done here because task context is safe for slower drivers.
 */
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

            /*
             * Page 0 bar grows with interrupt count.
             * Modulo keeps the width inside the 128-pixel OLED width.
             */
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

    /*
     * Configure GPIO18/GPIO19 as outputs.
     * These pins create the test pulses used to trigger interrupts.
     */
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

    /*
     * Configure GPIO4/GPIO5 as interrupt inputs.
     * Pull-up is enabled so the input has a known default state.
     */
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

    /*
     * GPIO4 is changed to trigger on both rising and falling edges.
     * GPIO5 remains rising-edge only from the previous gpio_config().
     */
    ret = gpio_set_intr_type(GPIO_INPUT_IO_0, GPIO_INTR_ANYEDGE);

    if (ret != ESP_OK)
    {
        printf("ERROR: gpio_set_intr_type failed: %d\n", ret);
        return;
    }

    /*
     * Queue length 10 means up to 10 GPIO events can wait if display_task
     * is busy updating the OLED.
     */
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

    /*
     * Initialize I2C first, then pass the OLED device handle to oled_init().
     */
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

    /* Draw a small startup bar to confirm OLED communication before interrupts. */
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

    /*
     * Create display_task before enabling ISR handlers so incoming events have
     * a task ready to consume them.
     */
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
                               (void *)(uintptr_t)GPIO_INPUT_IO_1);

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

        /*
         * Toggle output pins.
         * Because GPIO18->GPIO4 and GPIO19->GPIO5 are wired, these toggles
         * create interrupt edges on the input pins.
         */
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
Improvement:

try and implement timer based ISR's instead of GPIO based ISRs, to show that the same display_task can handle events 
from different sources.
*/