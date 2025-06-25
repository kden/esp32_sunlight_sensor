/**
* @file oled_utils.c
 *
 * Utilities for working with the SSD1306 OLED display.
 *
 * Copyright (c) 2025 Caden Howell (cadenhowell@gmail.com)
 *
 * Apache 2.0 Licensed as described in the file LICENSE
 */
#include <esp_log.h>
#include "app_config.h"
#include "oled_utils.h"


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