/**
* @file status_reporter.c
 *
 * Status and notification management functions.
 *
 * Copyright (c) 2025 Caden Howell (cadenhowell@gmail.com)
 *
 * Developed with assistance from ChatGPT 4o (2025), Google Gemini 2.5 Pro (2025) and Claude Sonnet 4 (2025).
 *
 * Apache 2.0 Licensed as described in the file LICENSE
 */

#include "status_reporter.h"
#include "app_config.h"
#include "http.h"
#include "battery_monitor.h"
#include "esp_log.h"

#define TAG "STATUS_REPORTER"
#define MAX_HTTP_RETRY_ATTEMPTS 3
#define HTTP_RETRY_DELAY_MS 5000

void send_device_status_if_appropriate(void) {
    static bool initial_no_battery_sent = false;

    // Try to send detailed battery status first
    esp_err_t battery_result = send_battery_status_update(CONFIG_SENSOR_ID, CONFIG_BEARER_TOKEN);

    if (battery_result == ESP_OK) {
        ESP_LOGI(TAG, "Battery status sent successfully");
    } else if (battery_result == ESP_ERR_NOT_FOUND) {
        // No battery detected - only send this once after boot
        if (!initial_no_battery_sent) {
            send_status_update_with_retry("no battery detected", CONFIG_SENSOR_ID, CONFIG_BEARER_TOKEN);
            initial_no_battery_sent = true;
            ESP_LOGI(TAG, "Initial 'no battery' status sent");
        } else {
            ESP_LOGD(TAG, "Skipping repeated 'no battery' status");
        }
    } else {
        ESP_LOGW(TAG, "Failed to get battery status: %s", esp_err_to_name(battery_result));
    }
}

bool send_status_update_with_retry(const char* status_message, const char* sensor_id, const char* bearer_token) {
    for (int attempt = 1; attempt <= MAX_HTTP_RETRY_ATTEMPTS; attempt++) {
        ESP_LOGI(TAG, "Status update send attempt %d/%d", attempt, MAX_HTTP_RETRY_ATTEMPTS);

        esp_err_t result = send_status_update(status_message, sensor_id, bearer_token);

        if (result == ESP_OK) {
            ESP_LOGI(TAG, "Status update sent successfully on attempt %d", attempt);
            return true;
        }

        ESP_LOGE(TAG, "Status update attempt %d failed: %s", attempt, esp_err_to_name(result));

        if (result == ESP_ERR_INVALID_ARG || result == ESP_ERR_NOT_ALLOWED) {
            ESP_LOGE(TAG, "Non-retryable error, aborting retry attempts");
            break;
        }

        if (attempt < MAX_HTTP_RETRY_ATTEMPTS) {
            ESP_LOGI(TAG, "Waiting %d ms before retry...", HTTP_RETRY_DELAY_MS);
            vTaskDelay(pdMS_TO_TICKS(HTTP_RETRY_DELAY_MS));
        }
    }

    ESP_LOGE(TAG, "Status update send failed after %d attempts", MAX_HTTP_RETRY_ATTEMPTS);
    return false;
}