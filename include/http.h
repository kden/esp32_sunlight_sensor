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
#include "esp_err.h"

esp_err_t send_sensor_data_with_status(const sensor_reading_t* readings, int count,
                                     const char* sensor_id, const char* bearer_token);

esp_err_t send_status_update_with_status(const char* status_message, const char* sensor_id,
                                       const char* bearer_token);