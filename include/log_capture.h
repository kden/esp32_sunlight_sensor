/**
* @file log_capture.h
 *
 * Capture ESP_LOG messages to persistent storage for debugging.
 *
 * Copyright (c) 2025 Caden Howell (cadenhowell@gmail.com)
 *
 * Developed with assistance from Claude Sonnet 4 (2025).
 *
 * Apache 2.0 Licensed as described in the file LICENSE
 */

#pragma once

#include "esp_err.h"

/**
 * @brief Initialize log capture system
 * Must be called early in app_main()
 */
esp_err_t log_capture_init(void);

/**
 * @brief Get captured logs as string
 */
esp_err_t log_capture_dump(char* buffer, size_t buffer_size);

/**
 * @brief Clear captured logs
 */
esp_err_t log_capture_clear(void);