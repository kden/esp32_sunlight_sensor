/**
* @file log_capture.c
 *
 * Capture ESP_LOG messages to persistent storage for debugging.
 *
 * Copyright (c) 2025 Caden Howell (cadenhowell@gmail.com)
 *
 * Developed with assistance from Claude Sonnet 4 (2025).
 *
 * Apache 2.0 Licensed as described in the file LICENSE
 */

#include "log_capture.h"
#include "nvs_flash.h"
#include "nvs.h"
#include "esp_log.h"
#include <string.h>
#include <stdarg.h>
#include <time.h>
#include "freertos/FreeRTOS.h"
#include "freertos/semphr.h"

#define TAG "LOG_CAPTURE"
#define NVS_NAMESPACE "debug_log"
#define LOG_BUFFER_SIZE 300  // Increased from 40 to 300 entries
#define LOG_ENTRY_SIZE 220   // Increased from 180 to 220 bytes per entry
// Total: ~66KB storage (well within ESP32 limits)

static nvs_handle_t log_nvs_handle;
static bool initialized = false;
static SemaphoreHandle_t log_mutex = NULL;

// Original log function pointer
static vprintf_like_t original_log_func = NULL;

// Custom log function that captures to NVS
static int custom_log_func(const char* format, va_list args) {
    // Call original logging first
    int ret = 0;
    if (original_log_func) {
        ret = original_log_func(format, args);
    }

    // Capture to NVS if initialized
    if (initialized && log_mutex && xSemaphoreTake(log_mutex, pdMS_TO_TICKS(100))) {
        char log_message[LOG_ENTRY_SIZE];
        vsnprintf(log_message, sizeof(log_message), format, args);

        // Remove newlines for cleaner storage
        size_t len = strlen(log_message);
        if (len > 0 && log_message[len-1] == '\n') {
            log_message[len-1] = '\0';
        }

        // Get current index
        uint32_t index = 0;
        nvs_get_u32(log_nvs_handle, "log_index", &index);

        // Write log entry with timestamp
        time_t now;
        time(&now);
        struct tm timeinfo;
        gmtime_r(&now, &timeinfo);

        char timestamped_entry[LOG_ENTRY_SIZE + 32];
        snprintf(timestamped_entry, sizeof(timestamped_entry),
                "%02d:%02d:%02d %s",
                timeinfo.tm_hour, timeinfo.tm_min, timeinfo.tm_sec,
                log_message);

        char key[16];
        snprintf(key, sizeof(key), "log_%lu", index % LOG_BUFFER_SIZE);

        if (nvs_set_str(log_nvs_handle, key, timestamped_entry) == ESP_OK) {
            nvs_set_u32(log_nvs_handle, "log_index", index + 1);
            nvs_commit(log_nvs_handle);
        }

        xSemaphoreGive(log_mutex);
    }

    return ret;
}

esp_err_t log_capture_init(void) {
    esp_err_t err = nvs_open(NVS_NAMESPACE, NVS_READWRITE, &log_nvs_handle);
    if (err != ESP_OK) {
        return err;
    }

    log_mutex = xSemaphoreCreateMutex();
    if (!log_mutex) {
        nvs_close(log_nvs_handle);
        return ESP_ERR_NO_MEM;
    }

    // Hook into the ESP logging system
    original_log_func = esp_log_set_vprintf(custom_log_func);

    initialized = true;
    ESP_LOGI(TAG, "Log capture initialized - now capturing all ESP_LOG messages (buffer: %d entries)", LOG_BUFFER_SIZE);

    return ESP_OK;
}

esp_err_t log_capture_dump(char* buffer, size_t buffer_size) {
    if (!initialized || !buffer || !log_mutex) {
        return ESP_ERR_INVALID_STATE;
    }

    if (xSemaphoreTake(log_mutex, pdMS_TO_TICKS(1000)) != pdTRUE) {
        return ESP_ERR_TIMEOUT;
    }

    uint32_t index = 0;
    nvs_get_u32(log_nvs_handle, "log_index", &index);

    buffer[0] = '\0';
    size_t used = 0;

    // Start from oldest entry in circular buffer
    uint32_t start_index = (index >= LOG_BUFFER_SIZE) ?
                          (index - LOG_BUFFER_SIZE) : 0;

    for (uint32_t i = 0; i < LOG_BUFFER_SIZE && i < index && used < buffer_size - 50; i++) {
        char key[16];
        snprintf(key, sizeof(key), "log_%lu", (start_index + i) % LOG_BUFFER_SIZE);

        size_t required_size = 0;
        if (nvs_get_str(log_nvs_handle, key, NULL, &required_size) == ESP_OK && required_size > 1) {
            if (used + required_size + 1 < buffer_size) {
                nvs_get_str(log_nvs_handle, key, buffer + used, &required_size);
                used += required_size - 1; // -1 for null terminator
                if (used < buffer_size - 2) {
                    buffer[used++] = '\n';
                    buffer[used] = '\0';
                }
            }
        }
    }

    xSemaphoreGive(log_mutex);
    return ESP_OK;
}

esp_err_t log_capture_clear(void) {
    if (!initialized || !log_mutex) {
        return ESP_ERR_INVALID_STATE;
    }

    if (xSemaphoreTake(log_mutex, pdMS_TO_TICKS(1000)) != pdTRUE) {
        return ESP_ERR_TIMEOUT;
    }

    // Clear all log entries
    for (int i = 0; i < LOG_BUFFER_SIZE; i++) {
        char key[16];
        snprintf(key, sizeof(key), "log_%d", i);
        nvs_erase_key(log_nvs_handle, key);
    }

    // Reset index
    nvs_set_u32(log_nvs_handle, "log_index", 0);
    nvs_commit(log_nvs_handle);

    xSemaphoreGive(log_mutex);
    ESP_LOGI(TAG, "Log capture cleared");
    return ESP_OK;
}