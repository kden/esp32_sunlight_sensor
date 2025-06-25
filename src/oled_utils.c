//
// Created by caden on 6/25/25.
//
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