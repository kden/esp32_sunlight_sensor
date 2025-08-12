/**
* @file task_get_sensor_data.c
 *
 * ESP-IDF adapter for sensor data collection.
 * This is the only sensor task file that knows about ESP-IDF.
 *
 * Copyright (c) 2025 Caden Howell (cadenhowell@gmail.com)
 *
 * Developed with assistance from ChatGPT 4o (2025) and Google Gemini 2.5 Pro (2025).
 *
 * Apache 2.0 Licensed as described in the file LICENSE
 */

#include "task_get_sensor_data.h"
#include "sensor_buffer_core.h"
#include "app_context.h"
#include "light_sensor.h"
#include "esp_log.h"
#include <string.h>
#include <time.h>

#define TAG "SENSOR_TASK"
#define READING_INTERVAL_S 15

// ESP-IDF implementation of buffer interface
static app_context_t* g_context = NULL;  // Set during task startup

static bool esp_acquire_buffer_lock(void) {
    if (!g_context || !g_context->buffer_mutex) {
        return false;
    }
    return xSemaphoreTake(g_context->buffer_mutex, portMAX_DELAY) == pdTRUE;
}

static void esp_release_buffer_lock(void) {
    if (g_context && g_context->buffer_mutex) {
        xSemaphoreGive(g_context->buffer_mutex);
    }
}

static void esp_log_message(const char* level, const char* message) {
    if (!level || !message) {
        return;
    }

    if (strcmp(level, "INFO") == 0) {
        ESP_LOGI(TAG, "%s", message);
    } else if (strcmp(level, "ERROR") == 0) {
        ESP_LOGE(TAG, "%s", message);
    } else if (strcmp(level, "WARN") == 0) {
        ESP_LOGW(TAG, "%s", message);
    }
}

// ESP-IDF Buffer Interface
static const buffer_interface_t esp_buffer_interface = {
    .acquire_buffer_lock = esp_acquire_buffer_lock,
    .release_buffer_lock = esp_release_buffer_lock,
    .log_message = esp_log_message
};

void task_get_sensor_data(void *arg) {
    app_context_t *context = (app_context_t *)arg;
    g_context = context;  // Store for interface functions

    if (!context) {
        ESP_LOGE(TAG, "Invalid context passed to task_get_sensor_data");
        vTaskDelete(NULL);
        return;
    }

    ESP_LOGI(TAG, "Sensor reading task started.");

    // Initialize pure business logic buffer
    // Note: We reuse the ESP-IDF buffer memory but wrap it in pure types
    sensor_buffer_t sensor_buffer;

    // Create a pure C buffer that points to the ESP-IDF buffer
    // This is safe because sensor_data_t has the same layout as sensor_reading_t
    sensor_data_t* pure_buffer = (sensor_data_t*)context->reading_buffer;

    init_sensor_buffer(&sensor_buffer, pure_buffer, context->reading_idx, context->buffer_size);

    while (1) {
        vTaskDelay(pdMS_TO_TICKS(READING_INTERVAL_S * 1000));

        // Get sensor reading (ESP-IDF specific)
        float lux = 0;
        esp_err_t err = get_ambient_light(context->light_sensor_hdl, &lux);
        if (err != ESP_OK) {
            ESP_LOGE(TAG, "Failed to get light reading: %s", esp_err_to_name(err));
            continue;
        }

        // Get timestamp (standard C)
        time_t now;
        time(&now);

        // Call pure business logic
        buffer_result_t result = add_sensor_reading(
            &sensor_buffer,
            now,
            lux,
            &esp_buffer_interface
        );

        // Handle result (ESP-IDF specific logging/actions)
        switch (result) {
            case BUFFER_RESULT_SUCCESS:
                // Normal case - already logged by business logic
                break;

            case BUFFER_RESULT_FULL:
                // Buffer full - business logic already logged warning
                // Could add ESP-IDF specific actions here (e.g., trigger early send)
                break;

            case BUFFER_RESULT_ERROR:
                ESP_LOGE(TAG, "Critical error in sensor buffering");
                break;
        }
    }
}