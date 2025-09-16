/**
* @file status_reporter.h
 *
 * Status and notification management functions.
 *
 * Copyright (c) 2025 Caden Howell (cadenhowell@gmail.com)
 *
 * Developed with assistance from ChatGPT 4o (2025), Google Gemini 2.5 Pro (2025) and Claude Sonnet 4 (2025).
 *
 * Apache 2.0 Licensed as described in the file LICENSE
 */

#pragma once

#include <stdbool.h>

/**
 * @brief Send combined device status (battery + wifi) if appropriate
 */
void send_device_status_if_appropriate(void);

/**
 * @brief Send status update with retry mechanism
 *
 * @param status_message Status message string
 * @param sensor_id Sensor identifier string
 * @param bearer_token Authentication bearer token
 * @return true if successful, false on failure
 */
bool send_status_update_with_retry(const char* status_message, const char* sensor_id, const char* bearer_token);