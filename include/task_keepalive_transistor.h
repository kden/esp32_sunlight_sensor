/**
* @file task_keepalive_transistor.h
 *
 * Smart power draw control using transistor-switched resistor for USB power bank keep-alive.
 *
 * Copyright (c) 2025 Caden Howell (cadenhowell@gmail.com)
 *
 * Developed with assistance from ChatGPT 4o (2025), Google Gemini 2.5 Pro (2025) and Claude Sonnet 4 (2025).
 *
 * Apache 2.0 Licensed as described in the file LICENSE
 */

#pragma once

#include <stdint.h>
#include <time.h>
#include "esp_err.h"

/**
 * @brief Configuration structure for keep-alive timing
 */
typedef struct {
    uint32_t on_duration_seconds;    // How long to keep the resistor connected (default: 10 seconds)
    uint32_t interval_minutes;       // How often to activate the resistor (default: 4 minutes)
    uint8_t control_gpio;            // GPIO pin controlling the transistor base (default: GPIO 2)
} keepalive_config_t;

/**
 * @brief Default configuration for keep-alive system
 */
#define KEEPALIVE_CONFIG_DEFAULT() { \
.on_duration_seconds = 10, \
.interval_minutes = 4, \
.control_gpio = 18 \
}

/**
 * @brief Initialize and start the smart keep-alive task
 *
 * @param config Pointer to configuration structure, or NULL to use defaults
 * @return esp_err_t ESP_OK on success
 */
esp_err_t init_smart_keepalive_task(const keepalive_config_t *config);

/**
 * @brief Update the timing configuration while running
 *
 * @param on_duration_seconds New on duration in seconds
 * @param interval_minutes New interval in minutes
 * @return esp_err_t ESP_OK on success
 */
esp_err_t update_keepalive_timing(uint32_t on_duration_seconds, uint32_t interval_minutes);

/**
 * @brief Get current keepalive statistics
 *
 * @param total_activations Output: total number of activations since boot
 * @param last_activation_time Output: timestamp of last activation
 * @return esp_err_t ESP_OK on success
 */
esp_err_t get_keepalive_stats(uint32_t *total_activations, time_t *last_activation_time);