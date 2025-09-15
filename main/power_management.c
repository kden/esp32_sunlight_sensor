/**
* @file power_management.c
 *
 * Power management utilities including deep sleep for ESP32-C3 battery-powered devices.
 *
 * Copyright (c) 2025 Caden Howell (cadenhowell@gmail.com)
 *
 * Developed with assistance from Claude Sonnet 4 (2025).
 *
 * Apache 2.0 Licensed as described in the file LICENSE
 */

#include "power_management.h"
#include "time_utils.h"
#include "app_config.h"
#include "battery_monitor.h"
#include "esp_sleep.h"
#include "esp_log.h"
#include "driver/rtc_io.h"
#include <time.h>

#define TAG "POWER_MGMT"

// Wake up every 30 minutes during night to check if it's time to resume
#define NIGHT_CHECK_INTERVAL_US (30 * 60 * 1000000ULL)  // 30 minutes in microseconds

bool should_enter_deep_sleep(void) {
    // Only ESP32-C3 supports our deep sleep implementation
#if !CONFIG_IDF_TARGET_ESP32C3
    ESP_LOGD(TAG, "Deep sleep only supported on ESP32-C3, skipping");
    return false;
#endif

    // Don't sleep if no battery is detected (USB-powered)
    if (!battery_is_present()) {
        ESP_LOGI(TAG, "No battery detected (USB power) - skipping deep sleep");
        return false;
    }

    // Don't sleep if not nighttime
    if (!is_nighttime_local()) {
        ESP_LOGD(TAG, "Not nighttime - skipping deep sleep");
        return false;
    }

    ESP_LOGD(TAG, "Conditions met for deep sleep: ESP32-C3 + battery + nighttime");
    return true;
}

uint64_t calculate_sleep_duration_us(void) {
    time_t now;
    struct tm local_time;

    time(&now);

    // Convert to local timezone
    char* old_tz = getenv("TZ");
    setenv("TZ", CONFIG_LOCAL_TIMEZONE, 1);
    tzset();
    localtime_r(&now, &local_time);

    // Restore timezone
    if (old_tz) {
        setenv("TZ", old_tz, 1);
    } else {
        unsetenv("TZ");
    }
    tzset();

    int current_hour = local_time.tm_hour;
    int current_min = local_time.tm_min;

    // If we're not in night hours, don't sleep
    if (!(current_hour >= CONFIG_NIGHT_START_HOUR || current_hour < CONFIG_NIGHT_END_HOUR)) {
        return 0;
    }

    // Calculate minutes until wake time
    int minutes_until_wake;

    if (current_hour >= CONFIG_NIGHT_START_HOUR) {
        // After night start hour, sleep until end hour next day
        minutes_until_wake = (24 - current_hour) * 60 - current_min + (CONFIG_NIGHT_END_HOUR * 60);
    } else {
        // Before end hour, sleep until end hour today
        minutes_until_wake = (CONFIG_NIGHT_END_HOUR * 60) - (current_hour * 60 + current_min);
    }

    // Convert to microseconds, but cap at our check interval
    uint64_t sleep_duration_us = (uint64_t)minutes_until_wake * 60 * 1000000ULL;

    if (sleep_duration_us > NIGHT_CHECK_INTERVAL_US) {
        sleep_duration_us = NIGHT_CHECK_INTERVAL_US;
    }

    ESP_LOGD(TAG, "Calculated sleep duration: %llu minutes", sleep_duration_us / (60 * 1000000ULL));
    return sleep_duration_us;
}

void enter_night_sleep(void) {
    if (!should_enter_deep_sleep()) {
        ESP_LOGI(TAG, "Conditions not met for deep sleep, continuing normal operation");
        return;
    }

    uint64_t sleep_time = calculate_sleep_duration_us();

    if (sleep_time == 0) {
        ESP_LOGI(TAG, "Not nighttime, skipping sleep");
        return;
    }

    ESP_LOGI(TAG, "Entering deep sleep for %llu minutes", sleep_time / (60 * 1000000ULL));

    // Configure wake up source - timer only for now
    esp_sleep_enable_timer_wakeup(sleep_time);

    // Enter deep sleep with automatic power domain configuration
    // ESP32-C3 will automatically configure power domains for optimal deep sleep
    esp_deep_sleep_start();
}

esp_sleep_wakeup_cause_t check_wakeup_reason(void) {
    esp_sleep_wakeup_cause_t wakeup_reason = esp_sleep_get_wakeup_cause();

    switch (wakeup_reason) {
        case ESP_SLEEP_WAKEUP_TIMER:
            ESP_LOGI(TAG, "Wakeup caused by timer");
            break;
        case ESP_SLEEP_WAKEUP_EXT1:
            ESP_LOGI(TAG, "Wakeup caused by external signal");
            break;
        case ESP_SLEEP_WAKEUP_UNDEFINED:
        default:
            ESP_LOGI(TAG, "Normal boot (not from deep sleep)");
            break;
    }

    return wakeup_reason;
}