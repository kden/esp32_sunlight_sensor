//
// Created by caden on 6/23/25.
//

#include <stdio.h>
#include "freertos/FreeRTOS.h"
#include "freertos/task.h"
#include "driver/gpio.h"

#define BLINK_GPIO GPIO_NUM_18

void app_main(void)
{
    gpio_set_direction(BLINK_GPIO, GPIO_MODE_OUTPUT);
    while (1)
    {
        gpio_set_level(BLINK_GPIO, 1);
        printf("LED ON (GPIO18)\n");
        vTaskDelay(pdMS_TO_TICKS(1000)); // Delay for 1000 ms (1 second)
        printf("LED OFF (GPIO18)\n");
        gpio_set_level(BLINK_GPIO, 0);
        vTaskDelay(pdMS_TO_TICKS(1000));
    }
}
