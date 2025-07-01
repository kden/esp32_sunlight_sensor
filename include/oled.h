/**
 * @file oled.h
 *
 * Utilities for working with the SSD1306 OLED display.
 *
 * Copyright (c) 2025 Caden Howell (cadenhowell@gmail.com)
 *
 * Developed with assistance from ChatGPT and Google Gemini.
 *
 * Apache 2.0 Licensed as described in the file LICENSE
 */
#pragma once

#include "ssd1306.h"

esp_err_t oled_init(i2c_master_bus_handle_t i2c_bus_handle, ssd1306_handle_t *oled_handle);
void dump_oled_info(ssd1306_handle_t);
esp_err_t display_info(ssd1306_handle_t oled_hdl, char lux_str[32], float lux);
