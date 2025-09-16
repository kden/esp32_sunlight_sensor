/**
* @file api_client.h
 *
 * High-level API client with JSON construction and chunked sending.
 *
 * Copyright (c) 2025 Caden Howell (cadenhowell@gmail.com)
 *
 * Developed with assistance from ChatGPT 4o (2025), Google Gemini 2.5 Pro (2025) and Claude Sonnet 4 (2025).
 *
 * Apache 2.0 Licensed as described in the file LICENSE
 */

#pragma once

#include "sensor_data.h"
#include "esp_err.h"

/**
 * @brief Send sensor data via API with chunked sending
 *
 * @param readings Array of sensor readings
 * @param count Number of readings in the array
 * @return esp_err_t ESP_OK on success, error code on failure
 */
esp_err_t api_send_sensor_data(const sensor_reading_t* readings, int count);

/**
 * @brief Send status update via API
 *
 * @param status_message Status message string
 * @return esp_err_t ESP_OK on success, error code on failure
 */
esp_err_t api_send_status_update(const char* status_message);

/**
 * @brief Send battery status update with detailed metrics
 *
 * @return esp_err_t ESP_OK on success, ESP_ERR_NOT_FOUND if no battery
 */
esp_err_t api_send_battery_status(void);