/**
* @file task_get_sensor_data.c
 *
 * Retrieve data from sensor and write to buffer.
 *
 * Copyright (c) 2025 Caden Howell (cadenhowell@gmail.com)
 *
 * Developed with assistance from ChatGPT 4o (2025) and Google Gemini 2.5 Pro (2025).
 *
 * Apache 2.0 Licensed as described in the file LICENSE
 */

#include "task_get_sensor_data.h"
#include "app_context.h"
#include "light_sensor.h"
#include "esp_log.h"
#include <string.h>
#include <time.h>

#define TAG "SENSOR_TASK"
#define READING_INTERVAL_S 15

void task_get_sensor_data(void *arg) {
    app_context_t *context = (app_context_t *)arg;

    ESP_LOGI(TAG, "Sensor reading task started.");

    while (1) {
        vTaskDelay(pdMS_TO_TICKS(READING_INTERVAL_S * 1000));

        float lux = 0;
        esp_err_t err = get_ambient_light(context->light_sensor_hdl, &lux);
        if (err != ESP_OK) {
            ESP_LOGE(TAG, "Failed to get light reading: %s", esp_err_to_name(err));
            continue;
        }

        time_t now;
        time(&now);

        // Lock the mutex to safely access the shared buffer
        if (xSemaphoreTake(context->buffer_mutex, portMAX_DELAY) == pdTRUE) {
            if (*(context->reading_idx) < context->buffer_size) {
                context->reading_buffer[*(context->reading_idx)].timestamp = now;
                context->reading_buffer[*(context->reading_idx)].lux = lux;
                (*(context->reading_idx))++;
                ESP_LOGI(TAG, "Reading #%d saved (Lux: %.2f)", *(context->reading_idx), lux);
            } else {
                ESP_LOGW(TAG, "Reading buffer is full. Overwriting oldest entry.");
                // Shift everything down and add to the end to keep the latest data
                memmove(&context->reading_buffer[0], &context->reading_buffer[1], (context->buffer_size - 1) * sizeof(sensor_reading_t));
                context->reading_buffer[context->buffer_size - 1].timestamp = now;
                context->reading_buffer[context->buffer_size - 1].lux = lux;
            }

            // Release the mutex
            xSemaphoreGive(context->buffer_mutex);
        }
    }
}