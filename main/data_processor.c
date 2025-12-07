/**
* @file data_processor.c
 *
 * Sensor data processing, storage management, and transmission functions.
 *
 * Copyright (c) 2025 Caden Howell (cadenhowell@gmail.com)
 *
 * Developed with assistance from ChatGPT 4o (2025), Google Gemini 2.5 Pro (2025) and Claude Sonnet 4 (2025).
 *
 * Apache 2.0 Licensed as described in the file LICENSE
 */

#include "data_processor.h"
#include "app_config.h"
#include "api_client.h"
#include "persistent_storage.h"
#include "esp_log.h"
#include <stdlib.h>
#include <time.h>

#define TAG "DATA_PROCESSOR"
#define MAX_HTTP_RETRY_ATTEMPTS 3
#define HTTP_RETRY_DELAY_MS 5000

/**
 * @brief Create a filtered copy of sensor readings with valid timestamps only
 */
static sensor_reading_t* create_filtered_readings(const sensor_reading_t* original_readings, int count, int* filtered_count) {
    if (original_readings == NULL) {
        *filtered_count = 0;
        return NULL;
    }

    if (count <= 0) {
        *filtered_count = 0;
        return NULL;
    }

    sensor_reading_t* filtered = malloc(count * sizeof(sensor_reading_t));
    if (filtered == NULL) {
        ESP_LOGE(TAG, "Failed to allocate memory for filtered readings");
        *filtered_count = 0;
        return NULL;
    }

    *filtered_count = 0;
    time_t now;
    time(&now);

    for (int i = 0; i < count; i++) {
        // Only include readings with reasonable timestamps
        time_t timestamp = original_readings[i].timestamp;

        // Skip readings that are unreasonable
        if (timestamp < 1704067200) { // Before 2024
            ESP_LOGW(TAG, "Skipping reading with invalid timestamp: %lld", (long long)timestamp);
            continue;
        }

        // Don't send readings from the future (more than 1 hour ahead)
        if (timestamp > now + 3600) {
            ESP_LOGW(TAG, "Skipping reading with future timestamp: %lld", (long long)timestamp);
            continue;
        }

        filtered[*filtered_count] = original_readings[i];
        (*filtered_count)++;
    }

    if (*filtered_count == 0) {
        free(filtered);
        return NULL;
    }

    ESP_LOGI(TAG, "Filtered %d/%d readings", *filtered_count, count);
    return filtered;
}

bool data_processor_init(void) {
    esp_err_t err = persistent_storage_init();
    if (err != ESP_OK) {
        ESP_LOGE(TAG, "Failed to initialize persistent storage: %s", esp_err_to_name(err));
        return false;
    }

    ESP_LOGI(TAG, "Data processor initialized successfully");

    int stored_count = 0;
    err = persistent_storage_get_count(&stored_count);
    if (err == ESP_OK && stored_count > 0) {
        ESP_LOGI(TAG, "Found %d stored readings from previous session", stored_count);
    }

    return true;
}

bool send_all_stored_readings(void) {
    int stored_count = 0;
    esp_err_t err = persistent_storage_get_count(&stored_count);

    if (err != ESP_OK || stored_count == 0) {
        ESP_LOGI(TAG, "No stored readings to send");
        return true; // Not an error - just nothing to send
    }

    ESP_LOGI(TAG, "Found %d stored readings to send", stored_count);

    sensor_reading_t* stored_readings = malloc(stored_count * sizeof(sensor_reading_t));
    if (stored_readings == NULL) {
        ESP_LOGE(TAG, "Failed to allocate memory for stored readings");
        return false;
    }

    int loaded_count = 0;
    err = persistent_storage_load_readings(stored_readings, stored_count, &loaded_count);

    if (err != ESP_OK || loaded_count == 0) {
        ESP_LOGE(TAG, "Failed to load stored readings: %s", esp_err_to_name(err));
        free(stored_readings);
        return false;
    }

    ESP_LOGI(TAG, "Loaded %d stored readings from persistent storage", loaded_count);

    // Filter readings with valid timestamps
    int filtered_count = 0;
    sensor_reading_t* filtered_readings = create_filtered_readings(stored_readings, loaded_count, &filtered_count);

    // Free the original buffer
    free(stored_readings);

    if (filtered_readings == NULL || filtered_count == 0) {
        ESP_LOGW(TAG, "No valid readings to send after filtering");
        if (filtered_readings) free(filtered_readings);
        // Clear invalid readings from storage
        persistent_storage_clear_readings();
        return true;
    }

    ESP_LOGI(TAG, "Sending %d filtered stored readings", filtered_count);

    // FIXED: Call api_send_sensor_data directly instead of using processor callback
    esp_err_t send_result = api_send_sensor_data(filtered_readings, filtered_count);

    free(filtered_readings);

    if (send_result == ESP_OK) {
        ESP_LOGI(TAG, "Successfully sent stored readings, clearing storage");
        persistent_storage_clear_readings();
        return true;
    } else {
        ESP_LOGE(TAG, "Failed to send stored readings: %s", esp_err_to_name(send_result));
        return false;
    }
}