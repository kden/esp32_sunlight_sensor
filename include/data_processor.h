/**
* @file data_processor.h
 *
 * Sensor data processing, storage management, and transmission functions.
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
 * @brief Initialize data processing and storage systems
 *
 * @return true if initialization was successful, false otherwise
 */
bool data_processor_init(void);

/**
 * @brief Send all stored readings and clear storage on success
 *
 * @return true if successful or no readings to send, false on error
 */
bool send_all_stored_readings(void);