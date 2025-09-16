/**
* @file status_reporter.c
 *
 * Status and notification management functions including boot reason detection.
 *
 * Copyright (c) 2025 Caden Howell (cadenhowell@gmail.com)
 *
 * Developed with assistance from ChatGPT 4o (2025), Google Gemini 2.5 Pro (2025) and Claude Sonnet 4 (2025).
 *
 * Apache 2.0 Licensed as described in the file LICENSE
 */

#include "status_reporter.h"
#include "app_config.h"
#include "api_client.h"
#include "adc_battery.h"
#include "wifi_manager.h"
#include "esp_sleep.h"
#include "esp_log.h"
#include <string.h>

#define TAG "STATUS_REPORTER"
#define MAX_HTTP_RETRY_ATTEMPTS 3
#define HTTP_RETRY_DELAY_MS 5000

// Battery monitoring thresholds
#define BATTERY_LOW_THRESHOLD_V     3.2       // Low battery warning threshold
#define BATTERY_CRITICAL_THRESHOLD_V 3.0      // Critical battery threshold

void create_enhanced_status_message(const char* original_message, char* buffer, size_t buffer_size) {
    static bool first_boot_complete = false;
    esp_sleep_wakeup_cause_t wakeup_cause = esp_sleep_get_wakeup_cause();

    // Only add prefixes for actual boot/wake events
    if (!first_boot_complete && wakeup_cause == ESP_SLEEP_WAKEUP_UNDEFINED) {
        // This is the very first boot after power-on or crash
        snprintf(buffer, buffer_size, "[boot] %s", original_message);
        first_boot_complete = true;
    } else if (wakeup_cause == ESP_SLEEP_WAKEUP_TIMER) {
        // This is a wake from deep sleep
        snprintf(buffer, buffer_size, "[wake] %s", original_message);
    } else {
        // For all other cases (periodic operations), don't add any prefix
        strncpy(buffer, original_message, buffer_size - 1);
        buffer[buffer_size - 1] = '\0';
    }
}

bool get_battery_status_string(char *buffer, size_t buffer_size) {
    if (buffer == NULL || buffer_size == 0) {
        return false;
    }

    if (!adc_battery_is_present()) {
        snprintf(buffer, buffer_size, "no battery detected");
        return true;
    }

    float voltage;
    if (adc_battery_get_voltage(&voltage) != ESP_OK) {
        snprintf(buffer, buffer_size, "battery read error");
        return false;
    }

    // Estimate percentage (rough approximation for Li-ion)
    float percentage = 0.0;
    if (voltage >= 4.0) {
        percentage = 100.0;
    } else if (voltage >= 3.7) {
        percentage = 50.0 + (voltage - 3.7) * (50.0 / 0.3);
    } else if (voltage >= 3.3) {
        percentage = (voltage - 3.3) * (50.0 / 0.4);
    } else {
        percentage = 0.0;
    }

    const char* status;
    if (voltage <= BATTERY_CRITICAL_THRESHOLD_V) {
        status = "critical";
    } else if (voltage <= BATTERY_LOW_THRESHOLD_V) {
        status = "low";
    } else {
        status = "ok";
    }

    snprintf(buffer, buffer_size, "battery %.2fV %.0f%% %s", voltage, percentage, status);
    return true;
}

bool get_device_status_string(char *buffer, size_t buffer_size) {
    if (buffer == NULL || buffer_size == 0) {
        return false;
    }

    char battery_status[64];
    char wifi_status[64];

    // Get battery status
    bool battery_ok = get_battery_status_string(battery_status, sizeof(battery_status));

    // Get WiFi status
    bool wifi_ok = (wifi_get_status_string(wifi_status, sizeof(wifi_status)) == ESP_OK);

    // Combine both statuses
    if (battery_ok && wifi_ok) {
        snprintf(buffer, buffer_size, "%s | %s", battery_status, wifi_status);
    } else if (battery_ok) {
        snprintf(buffer, buffer_size, "%s | wifi error", battery_status);
    } else if (wifi_ok) {
        snprintf(buffer, buffer_size, "battery error | %s", wifi_status);
    } else {
        snprintf(buffer, buffer_size, "battery error | wifi error");
    }

    return true;
}

void send_device_status_if_appropriate(void) {
    static bool initial_no_battery_sent = false;

    // Try to send detailed battery status first
    esp_err_t battery_result = api_send_battery_status();

    if (battery_result == ESP_OK) {
        ESP_LOGI(TAG, "Battery status sent successfully");
    } else if (battery_result == ESP_ERR_NOT_FOUND) {
        // No battery detected - only send this once after boot
        if (!initial_no_battery_sent) {
            send_status_update_with_retry("no battery detected");
            initial_no_battery_sent = true;
            ESP_LOGI(TAG, "Initial 'no battery' status sent");
        } else {
            ESP_LOGD(TAG, "Skipping repeated 'no battery' status");
        }
    } else {
        ESP_LOGW(TAG, "Failed to get battery status: %s", esp_err_to_name(battery_result));
    }
}

bool send_status_update_with_retry(const char* status_message) {
    for (int attempt = 1; attempt <= MAX_HTTP_RETRY_ATTEMPTS; attempt++) {
        ESP_LOGI(TAG, "Status update send attempt %d/%d", attempt, MAX_HTTP_RETRY_ATTEMPTS);

        esp_err_t result = api_send_status_update(status_message);

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