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


void send_sensor_data(const sensor_reading_t* readings, int count, const char* sensor_id, const char* bearer_token);
