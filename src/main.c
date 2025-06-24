#include <stdio.h>
#include "freertos/FreeRTOS.h"
#include "freertos/task.h"
#include "bh1750.h"
#include "i2cdev.h"

#define SDA_GPIO 21
#define SCL_GPIO 22

void app_main(void) {
    // Initialize I2C
    i2cdev_init();

    // Set up BH1750 descriptor
    i2c_dev_t dev = { 0 };
    ESP_ERROR_CHECK(bh1750_init_desc(&dev, 0x23, I2C_NUM_0, SDA_GPIO, SCL_GPIO));
    ESP_ERROR_CHECK(bh1750_setup(&dev, BH1750_MODE_CONTINUOUS, BH1750_RES_HIGH));

    // Main loop: read and print light levels
    while (1) {
        uint16_t lux = 0;
        if (bh1750_read(&dev, &lux) == ESP_OK) {
            printf("Ambient light: %u lux\n", lux);
        } else {
            printf("Failed to read from BH1750\n");
        }

        vTaskDelay(pdMS_TO_TICKS(1000));
    }
}
