/**
* @file persistent_storage.h
 *
 * Pure NVS operations for sensor readings.
 *
 * Copyright (c) 2025 Caden Howell (cadenhowell@gmail.com)
 *
 * Developed with assistance from Claude Sonnet 4 (2025).
 *
 * Apache 2.0 Licensed as described in the file LICENSE
 */

#pragma once

#include "sensor_data.h"
#include "esp_err.h"

// 4 hours of readings at 15-second intervals = 4 * 60 * 60 / 15 = 960 readings
#define PERSISTENT_STORAGE_MAX_READINGS 960

/**
 * @brief Initialize the persistent storage system
 *
 * @return esp_err_t ESP_OK on success
 */
esp_err_t persistent_storage_init(void);

/**
 * @brief Save readings to persistent storage
 *
 * @param readings Array of sensor readings to save
 * @param count Number of readings in the array
 * @return esp_err_t ESP_OK on success
 */
esp_err_t persistent_storage_save_readings(const sensor_reading_t* readings, int count);

/**
 * @brief Load all stored readings from persistent storage
 *
 * @param readings Buffer to store loaded readings (must be at least PERSISTENT_STORAGE_MAX_READINGS size)
 * @param max_count Maximum number of readings the buffer can hold
 * @param loaded_count Output parameter - actual number of readings loaded
 * @return esp_err_t ESP_OK on success
 */
esp_err_t persistent_storage_load_readings(sensor_reading_t* readings, int max_count, int* loaded_count);

/**
 * @brief Clear all stored readings from persistent storage
 *
 * @return esp_err_t ESP_OK on success
 */
esp_err_t persistent_storage_clear_readings(void);

/**
 * @brief Get the number of readings currently stored
 *
 * @param count Output parameter - number of stored readings
 * @return esp_err_t ESP_OK on success
 */
esp_err_t persistent_storage_get_count(int* count);