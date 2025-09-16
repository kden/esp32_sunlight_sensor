/**
* @file adc_battery.h
 *
 * Pure ADC interface for battery voltage reading.
 *
 * Copyright (c) 2025 Caden Howell (cadenhowell@gmail.com)
 *
 * Developed with assistance from Claude Sonnet 4 (2025).
 *
 * Apache 2.0 Licensed as described in the file LICENSE
 */

#pragma once

#include "esp_err.h"
#include <stdbool.h>

/**
 * @brief Initialize the battery ADC system
 *
 * @return esp_err_t ESP_OK on success, error code on failure
 */
esp_err_t adc_battery_init(void);

/**
 * @brief Check if a battery is connected
 *
 * @return bool true if battery is detected, false otherwise
 */
bool adc_battery_is_present(void);

/**
 * @brief Get the current battery voltage
 *
 * @param voltage Pointer to store the voltage reading
 * @return esp_err_t ESP_OK on success, error code on failure
 */
esp_err_t adc_battery_get_voltage(float *voltage);

/**
 * @brief Get battery data for API transmission
 *
 * @param voltage Pointer to store voltage value
 * @param percentage Pointer to store percentage value
 * @return esp_err_t ESP_OK on success, ESP_ERR_NOT_FOUND if no battery
 */
esp_err_t adc_battery_get_api_data(float *voltage, int *percentage);