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
#include <string.h>
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

bool process_buffered_readings(app_context_t *context, bool (*processor)(sensor_reading_t*, int)) {
    sensor_reading_t *temp_buffer = malloc(context->buffer_size * sizeof(sensor_reading_t));
    if (temp_buffer == NULL) {
        ESP_LOGE(TAG, "Failed to allocate temporary buffer for readings");
        return false;
    }

    int temp_count = 0;
    bool success = true;

    if (xSemaphoreTake(context->buffer_mutex, portMAX_DELAY) == pdTRUE) {
        if (*(context->reading_idx) > 0) {
            memcpy(temp_buffer, context->reading_buffer,
                   *(context->reading_idx) * sizeof(sensor_reading_t));
            temp_count = *(context->reading_idx);
            *(context->reading_idx) = 0;
        }
        xSemaphoreGive(context->buffer_mutex);
    }

    if (temp_count > 0) {
        success = processor(temp_buffer, temp_count);
    }

    free(temp_buffer);
    return success;
}

bool send_readings_processor(sensor_reading_t* readings, int count) {
    ESP_LOGI(TAG, "Sending %d batched readings.", count);

    // Create filtered readings
    int filtered_count;
    sensor_reading_t* filtered_readings = create_filtered_readings(readings, count, &filtered_count);

    if (filtered_readings == NULL || filtered_count == 0) {
        ESP_LOGW(TAG, "No valid readings to send after timestamp filtering");
        if (filtered_readings) free(filtered_readings);
        return true; // No valid readings is considered success
    }

    bool success = false;
    for (int attempt = 1; attempt <= MAX_HTTP_RETRY_ATTEMPTS; attempt++) {
        ESP_LOGI(TAG, "Sensor data send attempt %d/%d (%d filtered readings)",
                 attempt, MAX_HTTP_RETRY_ATTEMPTS, filtered_count);

        esp_err_t result = api_send_sensor_data(filtered_readings, filtered_count);

        if (result == ESP_OK) {
            ESP_LOGI(TAG, "Sensor data sent successfully on attempt %d", attempt);
            success = true;
            break;
        }

        ESP_LOGE(TAG, "Sensor data attempt %d failed: %s", attempt, esp_err_to_name(result));

        if (result == ESP_ERR_INVALID_ARG || result == ESP_ERR_NOT_ALLOWED) {
            ESP_LOGE(TAG, "Non-retryable error, aborting retry attempts");
            break;
        }

        if (attempt < MAX_HTTP_RETRY_ATTEMPTS) {
            ESP_LOGI(TAG, "Waiting %d ms before retry...", HTTP_RETRY_DELAY_MS);
            vTaskDelay(pdMS_TO_TICKS(HTTP_RETRY_DELAY_MS));
        }
    }

    free(filtered_readings);

    if (!success) {
        ESP_LOGE(TAG, "Sensor data send failed after %d attempts", MAX_HTTP_RETRY_ATTEMPTS);
    }

    return success;
}

bool save_readings_processor(sensor_reading_t* readings, int count) {
    ESP_LOGI(TAG, "Saving %d readings to persistent storage due to WiFi failure", count);
    esp_err_t storage_err = persistent_storage_save_readings(readings, count);
    if (storage_err != ESP_OK) {
        ESP_LOGE(TAG, "Failed to save readings to persistent storage: %s", esp_err_to_name(storage_err));
        return false;
    } else {
        ESP_LOGI(TAG, "Successfully saved readings to persistent storage");
        return true;
    }
}

bool send_all_stored_readings(void) {
    // Check if we have any stored readings first
    int stored_count = 0;
    esp_err_t err = persistent_storage_get_count(&stored_count);
    if (err != ESP_OK) {
        ESP_LOGE(TAG, "Failed to get stored reading count: %s", esp_err_to_name(err));
        return false;
    }

    if (stored_count == 0) {
        ESP_LOGI(TAG, "No stored readings to send");
        return true;
    }

    // Allocate memory for stored readings
    sensor_reading_t *stored_readings = malloc(PERSISTENT_STORAGE_MAX_READINGS * sizeof(sensor_reading_t));
    if (stored_readings == NULL) {
        ESP_LOGE(TAG, "Failed to allocate memory for stored readings");
        return false;
    }

    // Load stored readings
    int loaded_count = 0;
    err = persistent_storage_load_readings(stored_readings, PERSISTENT_STORAGE_MAX_READINGS, &loaded_count);
    if (err != ESP_OK) {
        ESP_LOGE(TAG, "Failed to load stored readings: %s", esp_err_to_name(err));
        free(stored_readings);
        return false;
    }

    if (loaded_count == 0) {
        ESP_LOGI(TAG, "No stored readings loaded");
        free(stored_readings);
        return true;
    }

    ESP_LOGI(TAG, "Attempting to send %d stored readings", loaded_count);

    // Send the stored readings
    bool send_success = send_readings_processor(stored_readings, loaded_count);

    if (send_success) {
        // Clear stored readings after successful send
        err = persistent_storage_clear_readings();
        if (err != ESP_OK) {
            ESP_LOGE(TAG, "Failed to clear stored readings after send: %s", esp_err_to_name(err));
            free(stored_readings);
            return false;
        }
        ESP_LOGI(TAG, "Successfully sent and cleared %d stored readings", loaded_count);
    } else {
        ESP_LOGE(TAG, "Failed to send stored readings");
    }

    free(stored_readings);
    return send_success;
}