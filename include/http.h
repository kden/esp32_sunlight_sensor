/**
* @file http.h
 *
 * HTTP client for sending sensor data.
 *
 * Copyright (c) 2025 Caden Howell (cadenhowell@gmail.com)
 *
 * Developed with assistance from ChatGPT 4o (2025), Google Gemini 2.5 Pro (2025) and Claude Sonnet 4 (2025).
 *
 * Apache 2.0 Licensed as described in the file LICENSE
 */
#pragma once

#include "sensor_data.h"
#include "esp_http_client.h"
#include "esp_err.h"

/**
 * @brief Send sensor data via HTTP POST
 *
 * @param readings Array of sensor readings
 * @param count Number of readings in the array
 * @param sensor_id Sensor identifier string
 * @param bearer_token Authentication bearer token
 * @return esp_err_t ESP_OK on success, error code on failure
 */
esp_err_t send_sensor_data_with_status(const sensor_reading_t* readings, int count, const char* sensor_id, const char* bearer_token);

/**
 * @brief Send status update via HTTP POST
 *
 * @param status_message Status message string
 * @param sensor_id Sensor identifier string
 * @param bearer_token Authentication bearer token
 * @return esp_err_t ESP_OK on success, error code on failure
 */
esp_err_t send_status_update(const char* status_message, const char* sensor_id, const char* bearer_token);

/**
 * @brief Send battery status update with detailed metrics
 *
 * @param sensor_id Sensor identifier string
 * @param bearer_token Authentication bearer token
 * @return esp_err_t ESP_OK on success, ESP_ERR_NOT_FOUND if no battery
 */
esp_err_t send_battery_status_update(const char* sensor_id, const char* bearer_token);