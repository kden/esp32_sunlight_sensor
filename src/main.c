#include <stdio.h>
#include "freertos/FreeRTOS.h"
#include "freertos/task.h"
#include "app_config.h"
#include "bh1750.h"
#include "i2cdev.h"
#include "ssd1306.h"
#include "esp_log.h"

#define SDA_GPIO 21
#define SCL_GPIO 22

void app_main(void) {
    // Initialize I2C
    i2c_master_bus_config_t  i2c0_bus_cfg = I2C0_MASTER_CONFIG_DEFAULT;
    i2c_master_bus_handle_t  i2c0_bus_hdl;
    i2cdev_init();

    // Set up BH1750 descriptor
    i2c_dev_t dev = { 0 };
    ESP_ERROR_CHECK(bh1750_init_desc(&dev, 0x23, I2C_NUM_0, SDA_GPIO, SCL_GPIO));
    ESP_ERROR_CHECK(bh1750_setup(&dev, BH1750_MODE_CONTINUOUS, BH1750_RES_HIGH));

    // Set up SSD1306 descriptor
    // Credit to Eric Gionet<gionet.c.eric@gmail.com>
    // https://github.com/K0I05/ESP32-S3_ESP-IDF_COMPONENTS/tree/main/components/peripherals/i2c/esp_ssd1306

    ssd1306_config_t dev_cfg         = I2C_SSD1306_128x64_CONFIG_DEFAULT;
    ssd1306_handle_t dev_hdl;
    ESP_ERROR_CHECK( i2c_new_master_bus(&i2c0_bus_cfg, &i2c0_bus_hdl) );


    ssd1306_init(i2c0_bus_hdl, &dev_cfg, &dev_hdl);
    if (dev_hdl == NULL) {
        ESP_LOGE(APP_TAG, "ssd1306 handle init failed");
        assert(dev_hdl);
    }

    char lineChar[16];

    // Main loop: read and print light levels
    while (1) {
        uint16_t lux = 0;
        if (bh1750_read(&dev, &lux) == ESP_OK) {
            ESP_LOGE(APP_TAG,"Ambient light: %u lux\n", lux);
        } else {
            ESP_LOGE(APP_TAG,"Failed to read from BH1750\n");
        }
        ESP_LOGI(APP_TAG, "######################## SSD1306 - START #########################");

        lineChar[0] = 0x01;
        sprintf(&lineChar[1], "%02d lux", lux);

        ESP_LOGI(APP_TAG, "Panel is 128x64");
        // Display x3 text
        ESP_LOGI(APP_TAG, "Display x3 Text");
        ssd1306_clear_display(dev_hdl, false);
        ssd1306_set_contrast(dev_hdl, 0xff);
        ssd1306_display_text_x3(dev_hdl, 0, lineChar, false);
        vTaskDelay(3000 / portTICK_PERIOD_MS);

        vTaskDelay(pdMS_TO_TICKS(1000));
    }
}
