/**
* @file http.h
 *
 * HTTP client for sending sensor data.
 *
 * Copyright (c) 2025 Caden Howell (cadenhowell@gmail.com)
 *
 * Developed with assistance from ChatGPT 4o (2025) and Google Gemini 2.5 Pro (2025).
 *
 * Apache 2.0 Licensed as described in the file LICENSE
 */
#pragma once

#include "sensor_data.h"
#include "esp_http_client.h"
#include "esp_err.h"

/**
 * @brief Sends a batch of sensor readings to the API.
 *
 * @param readings Pointer to an array of sensor readings.
 * @param count The number of readings in the array.
 * @param sensor_id The ID of the sensor.
 * @param bearer_token The authorization token.
 * @return ESP_OK on success, or an error code on failure.
 */
esp_err_t send_sensor_data(const sensor_reading_t* readings, int count, const char* sensor_id, const char* bearer_token);

void send_status_update(const char* status_message, const char* sensor_id, const char* bearer_token);