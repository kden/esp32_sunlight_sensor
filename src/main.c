//
// Created by caden on 6/23/25.
//

#include <stdio.h>
#include "freertos/FreeRTOS.h"
#include "freertos/task.h"

void app_main(void)
{
    while (1)
    {
        printf("Hello from PlatformIO and ESP-IDF\n");
        vTaskDelay(pdMS_TO_TICKS(1000));
    }
}
