/**
* @file task_get_sensor_data.c
 *
 * Retrieve data from sensor and write to buffer.
 *
 * Copyright (c) 2025 Caden Howell (cadenhowell@gmail.com)
 *
 * Developed with assistance from ChatGPT 4o (2025), Google Gemini 2.5 Pro (2025) and Claude Sonnet 4.5 (2025).
 *
 * Apache 2.0 Licensed as described in the file LICENSE
 */

#include "task_get_sensor_data.h"
#include "app_context.h"
#include "light_sensor.h"
#include "internal_temp.h"
#include "esp_log.h"
#include "persistent_storage.h"
#include <string.h>
#include <time.h>

#define TAG "SENSOR_TASK"
#define READING_INTERVAL_S 15

void task_get_sensor_data(void *arg) {
    app_context_t *context = (app_context_t *)arg;

    ESP_LOGI(TAG, "Sensor reading task started.");

    // Initialize internal temperature sensor
    internal_temp_init();

    while (1) {
        vTaskDelay(pdMS_TO_TICKS(READING_INTERVAL_S * 1000));

        float lux = 0;
        float chip_temp_c = 0;

        esp_err_t light_err = get_ambient_light(context->light_sensor_hdl, &lux);
        esp_err_t temp_err = internal_temp_read(&chip_temp_c);

        if (light_err != ESP_OK) {
            ESP_LOGE(TAG, "Failed to get light reading: %s", esp_err_to_name(light_err));
            continue;
        }

        time_t now;
        time(&now);

        if (xSemaphoreTake(context->buffer_mutex, portMAX_DELAY) == pdTRUE) {

            if (*(context->reading_idx) >= context->buffer_size) {
                ESP_LOGW(TAG, "Reading buffer is full. Saving batch to persistent storage to prevent data loss.");
                esp_err_t storage_err = persistent_storage_save_readings(context->reading_buffer, context->buffer_size);
                if (storage_err != ESP_OK) {
                    ESP_LOGE(TAG, "Failed to save full buffer to persistent storage: %s", esp_err_to_name(storage_err));
                }
                *(context->reading_idx) = 0;
            }

            context->reading_buffer[*(context->reading_idx)].timestamp = now;
            context->reading_buffer[*(context->reading_idx)].lux = lux;

            if (temp_err == ESP_OK) {
                context->reading_buffer[*(context->reading_idx)].chip_temp_c = chip_temp_c;
                context->reading_buffer[*(context->reading_idx)].chip_temp_f = (chip_temp_c * 9.0f / 5.0f) + 32.0f;
            } else {
                context->reading_buffer[*(context->reading_idx)].chip_temp_c = -999.0f;
                context->reading_buffer[*(context->reading_idx)].chip_temp_f = -999.0f;
            }

            (*(context->reading_idx))++;

            if (temp_err == ESP_OK) {
                ESP_LOGI(TAG, "Reading #%d saved (Lux: %.2f, Chip: %.1f°C/%.1f°F)",
                         *(context->reading_idx), lux, chip_temp_c,
                         context->reading_buffer[*(context->reading_idx) - 1].chip_temp_f);
            } else {
                ESP_LOGI(TAG, "Reading #%d saved (Lux: %.2f, Chip: temp error)",
                         *(context->reading_idx), lux);
            }

            xSemaphoreGive(context->buffer_mutex);
        }
    }
}