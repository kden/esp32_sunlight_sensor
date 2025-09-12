/**
* @file task_get_sensor_data.c
 *
 * Retrieve data from sensor and write to buffer.
 *
 * Copyright (c) 2025 Caden Howell (cadenhowell@gmail.com)
 *
 * Developed with assistance from ChatGPT 4o (2025), Google Gemini 2.5 Pro (2025) and Claude Sonnet 4 (2025).
 *
 * Apache 2.0 Licensed as described in the file LICENSE
 */

#include "task_get_sensor_data.h"
#include "app_context.h"
#include "light_sensor.h"
#include "esp_log.h"
#include "persistent_storage.h"
#include <string.h>
#include <time.h>

#include "time_utils.h"

#define TAG "SENSOR_TASK"
#define READING_INTERVAL_S 15

void task_get_sensor_data(void *arg) {
    app_context_t *context = (app_context_t *)arg;

    ESP_LOGI(TAG, "Sensor reading task started.");

    while (1) {
        vTaskDelay(pdMS_TO_TICKS(READING_INTERVAL_S * 1000));

        // Check if it's nighttime - skip sensor reading to save power
        if (is_nighttime_local()) {
            ESP_LOGD(TAG, "Nighttime detected - skipping sensor reading for power savings");
            continue;
        }

        float lux = 0;
        esp_err_t err = get_ambient_light(context->light_sensor_dev, &lux);
        if (err != ESP_OK) {
            ESP_LOGE(TAG, "Failed to get light reading: %s", esp_err_to_name(err));
            continue;
        }

        time_t now;
        time(&now);

        // Lock the mutex to safely access the shared buffer
        if (xSemaphoreTake(context->buffer_mutex, portMAX_DELAY) == pdTRUE) {

            // FIX: The logic is now simplified. If the buffer is full, we handle it
            // in a single, consistent way to prevent data loss and logic errors.
            if (*(context->reading_idx) >= context->buffer_size) {
                ESP_LOGW(TAG, "Reading buffer is full. Saving batch to persistent storage to prevent data loss.");

                // Save the full buffer to NVS. This is the safest action.
                esp_err_t storage_err = persistent_storage_save_readings(context->reading_buffer, context->buffer_size);
                if (storage_err != ESP_OK) {
                    ESP_LOGE(TAG, "Failed to save full buffer to persistent storage: %s", esp_err_to_name(storage_err));
                }

                // After saving, reset the buffer index to start a new batch.
                *(context->reading_idx) = 0;
            }

            // Now, add the new reading to the buffer at the correct index.
            context->reading_buffer[*(context->reading_idx)].timestamp = now;
            context->reading_buffer[*(context->reading_idx)].lux = lux;
            (*(context->reading_idx))++; // Increment index for the next reading

            ESP_LOGI(TAG, "Reading #%d saved (Lux: %.2f)", *(context->reading_idx), lux);

            // Release the mutex
            xSemaphoreGive(context->buffer_mutex);
        }
    }
}
