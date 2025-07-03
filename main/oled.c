/**
* @file oled.c
 *
 * Utilities for working with the SSD1306 OLED display.
 *
 * Copyright (c) 2025 Caden Howell (cadenhowell@gmail.com)
 *
 * Developed with assistance from ChatGPT 4o (2025) and Google Gemini 2.5 Pro (2025).
 *
 * For drivers and example code
 * Credit to Eric Gionet<gionet.c.eric@gmail.com>
 * https://github.com/K0I05/ESP32-S3_ESP-IDF_COMPONENTS/tree/main/components/peripherals/i2c/esp_ssd1306
 *
 * Apache 2.0 Licensed as described in the file LICENSE
 */

#include <esp_log.h>
#include "app_config.h"
#include "oled.h"

#define TAG "OLED"


void dump_oled_info(ssd1306_handle_t oled_hdl)
{
    if(oled_hdl->dev_config.panel_size == SSD1306_PANEL_128x32) {
        ESP_LOGI(APP_TAG, "Display Panel: 128x32");
    } else if(oled_hdl->dev_config.panel_size == SSD1306_PANEL_128x64) {
        ESP_LOGI(APP_TAG, "Display Panel: 128x64");
    } else {
        ESP_LOGI(APP_TAG, "Display Panel: 128x128");
    }
}

esp_err_t display_info(ssd1306_handle_t oled_hdl, char lux_str[32], float lux)
{
    snprintf(lux_str, 32, "%8.1f", lux);
    ESP_LOGI(APP_TAG, "Display Text");

    esp_err_t err = ssd1306_display_text_x2(oled_hdl, 0, "LUX:", false);
    if (err != ESP_OK) {
        ESP_LOGE(TAG, "OLED draw failed: %s", esp_err_to_name(err));
        return err;
    }

    err = ssd1306_display_text_x2(oled_hdl, 2, lux_str, false);
    if (err != ESP_OK)
    {
        ESP_LOGE(TAG, "OLED draw failed: %s", esp_err_to_name(err));
    }
    return err;
}

esp_err_t oled_init(i2c_master_bus_handle_t i2c_bus_handle, ssd1306_handle_t *oled_handle)
{
    ssd1306_config_t oled_cfg = I2C_SSD1306_128x64_CONFIG_DEFAULT;
    esp_err_t err = ssd1306_init(i2c_bus_handle, &oled_cfg, oled_handle);
    if (err != ESP_OK || *oled_handle == NULL)
    {
        ESP_LOGE(TAG, "ssd1306 handle init failed");
        return err;
    }

    dump_oled_info(*oled_handle);
    ESP_LOGI(TAG, "Display x3 Text");
    ssd1306_clear_display(*oled_handle, false);
    ssd1306_set_contrast(*oled_handle, 0xff);
    err = ssd1306_display_text_x2(*oled_handle, 0, "LUX:", false);
    if (err != ESP_OK)
    {
        ESP_LOGE(TAG, "OLED draw failed: %s", esp_err_to_name(err));
    }
    return err;
}
