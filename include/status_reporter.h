/**
* @file status_reporter.h
 *
 * Status and notification management functions including boot reason detection.
 *
 * Copyright (c) 2025 Caden Howell (cadenhowell@gmail.com)
 *
 * Developed with assistance from ChatGPT 4o (2025), Google Gemini 2.5 Pro (2025) and Claude Sonnet 4 (2025).
 *
 * Apache 2.0 Licensed as described in the file LICENSE
 */

#pragma once

#include <stdbool.h>
#include <stddef.h>

/**
 * @brief Send combined device status (battery + wifi) if appropriate
 */
void send_device_status_if_appropriate(void);

/**
 * @brief Send status update with retry mechanism
 *
 * @param status_message Status message string
 * @return true if successful, false on failure
 */
bool send_status_update_with_retry(const char* status_message);

/**
 * @brief Create enhanced status message with boot/wake reason prefix
 *
 * @param original_message Original status message
 * @param buffer Output buffer for enhanced message
 * @param buffer_size Size of output buffer
 */
void create_enhanced_status_message(const char* original_message, char* buffer, size_t buffer_size);

/**
 * @brief Get battery status string for logging/reporting
 *
 * @param buffer Buffer to store the status string
 * @param buffer_size Size of the buffer
 * @return true if successful, false on error
 */
bool get_battery_status_string(char *buffer, size_t buffer_size);

/**
 * @brief Get combined device status (battery + wifi) for reporting
 *
 * @param buffer Buffer to store the combined status string
 * @param buffer_size Size of the buffer
 * @return true if successful, false on error
 */
bool get_device_status_string(char *buffer, size_t buffer_size);