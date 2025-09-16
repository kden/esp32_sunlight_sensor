/**
* @file http_client.h
 *
 * Pure HTTP client operations.
 *
 * Copyright (c) 2025 Caden Howell (cadenhowell@gmail.com)
 *
 * Developed with assistance from ChatGPT 4o (2025), Google Gemini 2.5 Pro (2025) and Claude Sonnet 4 (2025).
 *
 * Apache 2.0 Licensed as described in the file LICENSE
 */

#pragma once

#include "esp_err.h"

/**
 * @brief Send a JSON payload via HTTP POST
 *
 * @param json_payload JSON string to send
 * @param bearer_token Authentication bearer token
 * @return esp_err_t ESP_OK on success, error code on failure
 */
esp_err_t http_send_json_payload(const char* json_payload, const char* bearer_token);