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

#include "app_context.h"
#include "sensor_data.h"
#include <stdbool.h>

/**
 * @brief Initialize data processing and storage systems
 *
 * @return true if initialization was successful, false otherwise
 */
bool data_processor_init(void);

/**
 * @brief Process buffered sensor readings with automatic buffer management
 *
 * @param context Application context containing the shared buffer
 * @param processor Function to process the readings (send or save)
 * @return true if processing was successful, false on error
 */
bool process_buffered_readings(app_context_t *context, bool (*processor)(sensor_reading_t*, int));

/**
 * @brief Processor function to send sensor readings
 *
 * @param readings Array of sensor readings
 * @param count Number of readings
 * @return true if successful, false otherwise
 */
bool send_readings_processor(sensor_reading_t* readings, int count);

/**
 * @brief Processor function to save sensor readings to persistent storage
 *
 * @param readings Array of sensor readings
 * @param count Number of readings
 * @return true if successful, false otherwise
 */
bool save_readings_processor(sensor_reading_t* readings, int count);

/**
 * @brief Send all stored readings and clear storage on success
 *
 * @return true if successful or no readings to send, false on error
 */
bool send_all_stored_readings(void);