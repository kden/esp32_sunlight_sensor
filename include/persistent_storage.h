/**
* @file persistent_storage.h
 *
 * Persistent storage for sensor readings using NVS.
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
#include <stdbool.h>

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

/**
 * @brief Initialize persistent storage with logging and error handling
 *
 * @return true if initialization was successful, false otherwise
 */
bool persistent_storage_initialize_and_log(void);

/**
 * @brief Send all stored readings and clear storage on success
 *
 * This function loads all stored readings, sends them using the data processor,
 * and clears the storage if transmission is successful.
 *
 * @return true if successful or no readings to send, false on error
 */
bool persistent_storage_send_all_stored(void);