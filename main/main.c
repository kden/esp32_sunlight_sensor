/**
* @file main.c
 *
 * ESP-IDF application to read and display ambient light levels using the BH1750 sensor.
 *
 * Copyright (c) 2025 Caden Howell (cadenhowell@gmail.com)
 *
 * Apache 2.0 Licensed as described in the file LICENSE
 */

#include <stdio.h>
#include <esp_log.h>
#include <esp_check.h>
#include "freertos/FreeRTOS.h"
#include "freertos/task.h"
#include "app_config.h"
#include "sdkconfig.h"

#include "bh1750.h"
#include "ssd1306.h"
#include "oled_utils.h"

#define SDA_GPIO 21
#define SCL_GPIO 22
#define TAG "MAIN"

void app_main(void)
{
    printf("Sensor ID: %s\n", CONFIG_SENSOR_ID);
    printf("Bearer Token: %s\n", CONFIG_BEARER_TOKEN);
    printf("Wifi Credentials: %s\n", CONFIG_WIFI_CREDENTIALS);
    // Initialize I2C
    i2c_master_bus_handle_t i2c0_bus_hdl;
    i2c_master_bus_config_t i2c0_bus_cfg = I2C0_MASTER_CONFIG_DEFAULT;
    ESP_ERROR_CHECK(i2c_new_master_bus(&i2c0_bus_cfg, &i2c0_bus_hdl));

    // Set up BH1750 descriptor
    // Credit to Eric Gionet<gionet.c.eric@gmail.com>
    // https://github.com/K0I05/ESP32-S3_ESP-IDF_COMPONENTS/tree/main/components/peripherals/i2c/esp_bh1750
    bh1750_config_t dev_cfg = I2C_BH1750_CONFIG_DEFAULT;
    bh1750_handle_t dev_hdl;

    bh1750_init(i2c0_bus_hdl, &dev_cfg, &dev_hdl);
    if (dev_hdl == NULL)
    {
        ESP_LOGE(APP_TAG, "bh1750 handle init failed");
        assert(dev_hdl);
    }

    // Set up SSD1306 descriptor
    // Credit to Eric Gionet<gionet.c.eric@gmail.com>
    // https://github.com/K0I05/ESP32-S3_ESP-IDF_COMPONENTS/tree/main/components/peripherals/i2c/esp_ssd1306

    ssd1306_config_t oled_cfg = I2C_SSD1306_128x64_CONFIG_DEFAULT;

    ssd1306_handle_t oled_hdl;
    ssd1306_init(i2c0_bus_hdl, &oled_cfg, &oled_hdl);
    if (oled_hdl == NULL)
    {
        ESP_LOGE(APP_TAG, "ssd1306 handle init failed");
    }

    dump_oled_info(oled_hdl);
    ESP_LOGI(APP_TAG, "Display x3 Text");
    ssd1306_clear_display(oled_hdl, false);
    ssd1306_set_contrast(oled_hdl, 0xff);
    esp_err_t err = ssd1306_display_text_x2(oled_hdl, 0, "LUX:", false);
    if (err != ESP_OK)
    {
        ESP_LOGE(TAG, "OLED draw failed: %s", esp_err_to_name(err));
    }

    char lux_str[32];

    // Main loop: read and print light levels
    while (1)
    {
        float lux = 0;
        ESP_LOGI(APP_TAG, "Sensor ID (%s)", CONFIG_SENSOR_ID);
        ESP_LOGI(APP_TAG, "Bearer Token (%s)", CONFIG_BEARER_TOKEN);
        ESP_LOGI(APP_TAG, "Wifi Credentials (%s)", CONFIG_WIFI_CREDENTIALS);


        esp_err_t result = bh1750_get_ambient_light(dev_hdl, &lux);
        if (result != ESP_OK)
        {
            ESP_LOGE(APP_TAG, "bh1750 device read failed (%s)", esp_err_to_name(result));
        }
        else
        {
            ESP_LOGI(APP_TAG, "ambient light:     %.2f lux", lux);
        }

        snprintf(lux_str, sizeof(lux_str), "%8.1f", lux);
        ESP_LOGI(APP_TAG, "Display Text");

        err = ssd1306_display_text_x2(oled_hdl, 0, "LUX:", false);
        err = ssd1306_display_text_x2(oled_hdl, 2, lux_str, false);
        if (err != ESP_OK)
        {
            ESP_LOGE(TAG, "OLED draw failed: %s", esp_err_to_name(err));
        }
        vTaskDelay(1000 / portTICK_PERIOD_MS);
    }
}



