/**
* @file internal_temp.h
 *
 * ESP32 internal temperature sensor interface.
 *
 * Copyright (c) 2025 Caden Howell (cadenhowell@gmail.com)
 *
 * Developed with assistance from Claude Sonnet 4 (2025).
 *
 * Apache 2.0 Licensed as described in the file LICENSE
 */

#pragma once

#include "esp_err.h"

/**
 * @brief Initialize internal temperature sensor
 */
esp_err_t internal_temp_init(void);

/**
 * @brief Read internal chip temperature
 * @param temp_celsius Output temperature in degrees Celsius
 * @return ESP_OK on success
 */
esp_err_t internal_temp_read(float *temp_celsius);