/**
 * @file persistent_storage.c
 *
 * Persistent storage for sensor readings using NVS.
 * NEW STRATEGY: This version uses an append-only method, saving each batch
 * of readings as a separate NVS entry to avoid large read-modify-write cycles
 * and prevent heap corruption.
 *
 * Copyright (c) 2025 Caden Howell (cadenhowell@gmail.com)
 *
 * Developed with assistance from Claude Sonnet 4 (2025) and Google Gemini Pro 2.5 (2025).
 *
 * Apache 2.0 Licensed as described in the file LICENSE
 */

#include "persistent_storage.h"
#include "nvs_flash.h"
#include "nvs.h"
#include "esp_log.h"
#include "freertos/FreeRTOS.h"
#include "freertos/semphr.h"
#include <string.h>
#include <stdio.h>

#define TAG "PERSISTENT_STORAGE"
#define NVS_NAMESPACE "sensor_data"
#define KEY_BATCH_COUNT "batch_count"
#define KEY_BATCH_PREFIX "batch_"

static nvs_handle_t s_nvs_handle = 0;
static bool s_initialized = false;
static SemaphoreHandle_t s_nvs_mutex = NULL;

esp_err_t persistent_storage_init(void) {
    if (s_initialized) {
        return ESP_OK;
    }

    if (s_nvs_mutex == NULL) {
        s_nvs_mutex = xSemaphoreCreateMutex();
    }
    if (s_nvs_mutex == NULL) {
        ESP_LOGE(TAG, "Failed to create NVS mutex");
        return ESP_FAIL;
    }

    esp_err_t err = nvs_open(NVS_NAMESPACE, NVS_READWRITE, &s_nvs_handle);
    if (err != ESP_OK) {
        ESP_LOGE(TAG, "Failed to open NVS handle: %s", esp_err_to_name(err));
        return err;
    }

    s_initialized = true;
    ESP_LOGD(TAG, "Persistent storage initialized");
    return ESP_OK;
}

esp_err_t persistent_storage_save_readings(const sensor_reading_t* readings, int count) {
    if (!s_initialized || s_nvs_mutex == NULL) {
        ESP_LOGE(TAG, "Storage not initialized");
        return ESP_ERR_INVALID_STATE;
    }
    if (readings == NULL || count <= 0) {
        return ESP_ERR_INVALID_ARG;
    }

    if (xSemaphoreTake(s_nvs_mutex, portMAX_DELAY) != pdTRUE) {
        ESP_LOGE(TAG, "Failed to take NVS mutex");
        return ESP_ERR_TIMEOUT;
    }

    esp_err_t err;
    int32_t batch_count = 0;

    // Get the current number of stored batches
    err = nvs_get_i32(s_nvs_handle, KEY_BATCH_COUNT, &batch_count);
    if (err == ESP_ERR_NVS_NOT_FOUND) {
        batch_count = 0; // This is the first batch
    } else if (err != ESP_OK) {
        ESP_LOGE(TAG, "Failed to get batch count: %s", esp_err_to_name(err));
        xSemaphoreGive(s_nvs_mutex);
        return err;
    }

    // Create the key for the new batch, e.g., "batch_0", "batch_1"
    char batch_key[32];
    snprintf(batch_key, sizeof(batch_key), "%s%d", KEY_BATCH_PREFIX, (int)batch_count);

    // Save the new batch of readings as a blob
    err = nvs_set_blob(s_nvs_handle, batch_key, readings, count * sizeof(sensor_reading_t));
    if (err != ESP_OK) {
        ESP_LOGE(TAG, "Failed to save batch '%s': %s", batch_key, esp_err_to_name(err));
        xSemaphoreGive(s_nvs_mutex);
        return err;
    }

    // Increment the batch count and save it
    batch_count++;
    err = nvs_set_i32(s_nvs_handle, KEY_BATCH_COUNT, batch_count);
    if (err != ESP_OK) {
        ESP_LOGE(TAG, "Failed to update batch count: %s", esp_err_to_name(err));
        xSemaphoreGive(s_nvs_mutex);
        return err;
    }

    // Commit changes to flash
    err = nvs_commit(s_nvs_handle);
    if (err != ESP_OK) {
        ESP_LOGE(TAG, "Failed to commit NVS changes: %s", esp_err_to_name(err));
    } else {
        ESP_LOGI(TAG, "Successfully saved batch #%d (%d readings)", (int)(batch_count - 1), count);
    }

    xSemaphoreGive(s_nvs_mutex);
    return err;
}

esp_err_t persistent_storage_load_readings(sensor_reading_t* readings, int max_count, int* loaded_count) {
    if (!s_initialized || s_nvs_mutex == NULL) {
        ESP_LOGE(TAG, "Storage not initialized");
        return ESP_ERR_INVALID_STATE;
    }
    if (readings == NULL || loaded_count == NULL || max_count <= 0) {
        return ESP_ERR_INVALID_ARG;
    }

    *loaded_count = 0;

    if (xSemaphoreTake(s_nvs_mutex, portMAX_DELAY) != pdTRUE) {
        ESP_LOGE(TAG, "Failed to take NVS mutex");
        return ESP_ERR_TIMEOUT;
    }

    esp_err_t err;
    int32_t batch_count = 0;

    err = nvs_get_i32(s_nvs_handle, KEY_BATCH_COUNT, &batch_count);
    if (err == ESP_ERR_NVS_NOT_FOUND) {
        ESP_LOGI(TAG, "No batches found in storage.");
        xSemaphoreGive(s_nvs_mutex);
        return ESP_OK;
    } else if (err != ESP_OK) {
        ESP_LOGE(TAG, "Failed to get batch count: %s", esp_err_to_name(err));
        xSemaphoreGive(s_nvs_mutex);
        return err;
    }

    if (batch_count == 0) {
        xSemaphoreGive(s_nvs_mutex);
        return ESP_OK;
    }

    // Iterate through each stored batch and load it
    for (int i = 0; i < batch_count; i++) {
        char batch_key[32];
        snprintf(batch_key, sizeof(batch_key), "%s%d", KEY_BATCH_PREFIX, i);

        size_t required_size;
        // First, get the size of the blob
        err = nvs_get_blob(s_nvs_handle, batch_key, NULL, &required_size);
        if (err != ESP_OK) {
            ESP_LOGW(TAG, "Failed to get size for batch '%s': %s", batch_key, esp_err_to_name(err));
            continue; // Skip corrupted batch
        }

        int readings_in_batch = required_size / sizeof(sensor_reading_t);
        if (*loaded_count + readings_in_batch > max_count) {
            ESP_LOGW(TAG, "Buffer full. Cannot load more readings.");
            break;
        }

        // Now, load the blob into the correct position in the output buffer
        err = nvs_get_blob(s_nvs_handle, batch_key, readings + *loaded_count, &required_size);
        if (err != ESP_OK) {
            ESP_LOGW(TAG, "Failed to load batch '%s': %s", batch_key, esp_err_to_name(err));
            continue; // Skip corrupted batch
        }
        *loaded_count += readings_in_batch;
    }

    ESP_LOGI(TAG, "Loaded %d readings from %d batches.", *loaded_count, (int)batch_count);

    xSemaphoreGive(s_nvs_mutex);
    return ESP_OK;
}

esp_err_t persistent_storage_clear_readings(void) {
    if (!s_initialized || s_nvs_mutex == NULL) {
        ESP_LOGE(TAG, "Storage not initialized");
        return ESP_ERR_INVALID_STATE;
    }
    if (xSemaphoreTake(s_nvs_mutex, portMAX_DELAY) != pdTRUE) {
        ESP_LOGE(TAG, "Failed to take NVS mutex");
        return ESP_ERR_TIMEOUT;
    }

    esp_err_t err;
    int32_t batch_count = 0;

    // Get the number of batches to erase
    nvs_get_i32(s_nvs_handle, KEY_BATCH_COUNT, &batch_count);

    // Erase each batch
    for (int i = 0; i < batch_count; i++) {
        char batch_key[32];
        snprintf(batch_key, sizeof(batch_key), "%s%d", KEY_BATCH_PREFIX, i);
        nvs_erase_key(s_nvs_handle, batch_key);
    }

    // Erase the counter itself
    nvs_erase_key(s_nvs_handle, KEY_BATCH_COUNT);

    err = nvs_commit(s_nvs_handle);
    if (err != ESP_OK) {
        ESP_LOGE(TAG, "Failed to commit clear operation: %s", esp_err_to_name(err));
    } else {
        ESP_LOGI(TAG, "Cleared all stored readings.");
    }

    xSemaphoreGive(s_nvs_mutex);
    return err;
}

esp_err_t persistent_storage_get_count(int* count) {
    if (!s_initialized || s_nvs_mutex == NULL) {
        ESP_LOGE(TAG, "Storage not initialized");
        return ESP_ERR_INVALID_STATE;
    }
    if (count == NULL) {
        return ESP_ERR_INVALID_ARG;
    }
    *count = 0; // Default to zero

    // This function now sums up readings from all batches
    // FIX: Declare the large buffer as 'static' to move it off the stack
    static sensor_reading_t temp_buffer[PERSISTENT_STORAGE_MAX_READINGS];
    return persistent_storage_load_readings(temp_buffer, PERSISTENT_STORAGE_MAX_READINGS, count);
}