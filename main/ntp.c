/**
* @file ntp.c
 *
 * Client for time synchronization with improved reliability
 *
 * Copyright (c) 2025 Caden Howell (cadenhowell@gmail.com)
 *
 * Developed with assistance from ChatGPT 4o (2025), Google Gemini 2.5 Pro (2025) and Claude Sonnet 4 (2025).
 *
 * Apache 2.0 Licensed as described in the file LICENSE
 */
#include <esp_log.h>
#include "app_config.h"
#include "esp_sntp.h"
#include "ntp.h"
#include <time.h>

#define TAG "NTP"

// Minimum reasonable timestamp (January 1, 2024)
#define MIN_REASONABLE_TIMESTAMP 1704067200

static bool is_time_reasonable(time_t timestamp) {
    return timestamp >= MIN_REASONABLE_TIMESTAMP;
}

// SNTP initialization with improved reliability
bool initialize_sntp(void)
{
    ESP_LOGI(TAG, "Initializing SNTP");

    // Stop any existing SNTP service
    if (esp_sntp_enabled()) {
        esp_sntp_stop();
    }

    esp_sntp_setoperatingmode(SNTP_OPMODE_POLL);
    esp_sntp_setservername(0, "pool.ntp.org");
    esp_sntp_setservername(1, "time.google.com");
    esp_sntp_setservername(2, "time.cloudflare.com");
    esp_sntp_init();

    // Wait for time to be set with better validation
    time_t now = 0;
    struct tm timeinfo = { 0 };
    int retry = 0;
    const int retry_count = 30; // Increased retry count

    while (retry < retry_count) {
        time(&now);

        // Check if SNTP sync is complete AND time is reasonable
        if (esp_sntp_get_sync_status() == SNTP_SYNC_STATUS_COMPLETED && is_time_reasonable(now)) {
            localtime_r(&now, &timeinfo);
            ESP_LOGI(TAG, "Time synchronized successfully: %04d-%02d-%02d %02d:%02d:%02d UTC",
                     timeinfo.tm_year + 1900, timeinfo.tm_mon + 1, timeinfo.tm_mday,
                     timeinfo.tm_hour, timeinfo.tm_min, timeinfo.tm_sec);
            return true;
        }

        retry++;
        ESP_LOGI(TAG, "Waiting for system time to be set... (%d/%d) [sync_status=%d, timestamp=%lld]",
                 retry, retry_count, esp_sntp_get_sync_status(), (long long)now);
        vTaskDelay(2000 / portTICK_PERIOD_MS);
    }

    time(&now);
    ESP_LOGE(TAG, "SNTP synchronization failed after %d attempts. Current timestamp: %lld (reasonable: %s)",
             retry_count, (long long)now, is_time_reasonable(now) ? "yes" : "no");
    return false;
}

bool is_system_time_valid(void) {
    time_t now;
    time(&now);
    return is_time_reasonable(now);
}