/**
* @file battery_monitor.h
 *
 * Battery voltage monitoring functions.
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
 * @brief Initialize the battery monitoring system
 *
 * @return esp_err_t ESP_OK on success, error code on failure
 */
esp_err_t battery_monitor_init(void);

/**
 * @brief Check if a battery is connected
 *
 * @return bool true if battery is detected, false otherwise
 */
bool battery_is_present(void);

/**
 * @brief Get the current battery voltage
 *
 * @param voltage Pointer to store the voltage reading
 * @return esp_err_t ESP_OK on success, error code on failure
 */
esp_err_t battery_get_voltage(float *voltage);

/**
 * @brief Get battery status string for logging/reporting
 *
 * @param buffer Buffer to store the status string
 * @param buffer_size Size of the buffer
 * @return esp_err_t ESP_OK on success, error code on failure
 */
esp_err_t battery_get_status_string(char *buffer, size_t buffer_size);

/**
 * @brief Print debug information about battery monitoring setup
 */
void battery_debug_info(void);