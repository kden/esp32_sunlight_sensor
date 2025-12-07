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
#include "adc_battery.h"
#include "esp_sleep.h"
#include "esp_log.h"
#include <time.h>
#include <sys/time.h>

#define TAG "POWER_MGMT"

// Wake up every 30 minutes during night to check if it's time to resume
#define NIGHT_CHECK_INTERVAL_US (30 * 60 * 1000000ULL)

// RTC memory attribute - survives deep sleep on ESP32-C3
static RTC_DATA_ATTR rtc_sleep_state_t rtc_state;

bool rtc_state_init(void) {
    if (rtc_state.magic == RTC_MEMORY_MAGIC) {
        ESP_LOGI(TAG, "RTC state restored: boots=%lu, readings=%lu, last_send=%llu",
                 rtc_state.boot_count, rtc_state.readings_taken, rtc_state.last_send_timestamp);
        rtc_state.boot_count++;
        return true;
    }

    // Initialize fresh RTC state
    ESP_LOGI(TAG, "Initializing fresh RTC state");
    rtc_state.boot_count = 0;
    rtc_state.readings_taken = 0;
    rtc_state.last_send_timestamp = 0;
    rtc_state.wifi_send_failed = false;
    rtc_state.magic = RTC_MEMORY_MAGIC;
    return false;
}

rtc_sleep_state_t* rtc_state_get(void) {
    return &rtc_state;
}

void rtc_state_save(void) {
    // On ESP32-C3, RTC memory is automatically preserved
    // This function exists for explicit save points if needed
    ESP_LOGD(TAG, "RTC state saved");
}

bool should_enter_deep_sleep(void) {
    // Testing override - force deep sleep even with USB power
    if (IGNORE_USB_POWER_FOR_TESTING) {
        ESP_LOGW(TAG, "⚠️  USB POWER CHECK DISABLED FOR TESTING - FORCING DEEP SLEEP ⚠️");
        return true;
    }

    // Normal operation - don't sleep if no battery is detected (USB-powered)
    if (!adc_battery_is_present()) {
        ESP_LOGI(TAG, "No battery detected (USB power) - skipping deep sleep");
        return false;
    }

    ESP_LOGI(TAG, "Battery detected - deep sleep enabled");
    return true;
}

uint64_t calculate_sleep_duration_us(bool is_nighttime) {
    if (is_nighttime) {
        // During night, sleep for longer periods (30 minutes)
        return calculate_night_sleep_duration_us();
    }

    // During day, calculate time until next reading or send
    uint32_t seconds_to_send = seconds_until_next_send();

    if (seconds_to_send == 0 || seconds_to_send > READING_INTERVAL_S) {
        // Sleep until next reading
        return (uint64_t)READING_INTERVAL_S * 1000000ULL;
    } else {
        // Sleep until send time
        return (uint64_t)seconds_to_send * 1000000ULL;
    }
}

void enter_deep_sleep(uint64_t sleep_time_us) {
    // Minimum sleep time to make deep sleep worthwhile
    const uint64_t MIN_SLEEP_US = 1000000; // 1 second

    if (sleep_time_us < MIN_SLEEP_US) {
        ESP_LOGW(TAG, "Sleep time too short (%llu us), using light sleep instead", sleep_time_us);
        esp_sleep_enable_timer_wakeup(sleep_time_us);
        esp_light_sleep_start();
        return;
    }

    ESP_LOGI(TAG, "Entering deep sleep for %llu seconds", sleep_time_us / 1000000ULL);

    // Save RTC state before sleeping
    rtc_state_save();

    // Configure wake up source - timer only
    esp_sleep_enable_timer_wakeup(sleep_time_us);

    // Enter deep sleep - ESP32-C3 automatically configures power domains
    esp_deep_sleep_start();
}

esp_sleep_wakeup_cause_t check_wakeup_reason(void) {
    esp_sleep_wakeup_cause_t wakeup_reason = esp_sleep_get_wakeup_cause();

    switch (wakeup_reason) {
        case ESP_SLEEP_WAKEUP_TIMER:
            ESP_LOGI(TAG, "Wakeup caused by timer");
            break;
        case ESP_SLEEP_WAKEUP_UNDEFINED:
        default:
            ESP_LOGI(TAG, "Normal boot (not from deep sleep)");
            break;
    }

    return wakeup_reason;
}

bool is_time_to_send_data(void) {
    time_t now = time(NULL);

    // First boot or invalid timestamp
    if (rtc_state.last_send_timestamp == 0) {
        ESP_LOGI(TAG, "First send cycle");
        return true;
    }

    uint64_t elapsed = (uint64_t)now - rtc_state.last_send_timestamp;

    if (elapsed >= SEND_INTERVAL_S) {
        ESP_LOGI(TAG, "Send interval reached (%llu >= %d seconds)", elapsed, SEND_INTERVAL_S);
        return true;
    }

    ESP_LOGD(TAG, "Not time to send yet (%llu < %d seconds)", elapsed, SEND_INTERVAL_S);
    return false;
}

uint32_t seconds_until_next_send(void) {
    time_t now = time(NULL);

    // First boot or invalid timestamp
    if (rtc_state.last_send_timestamp == 0) {
        return 0; // Send immediately
    }

    uint64_t elapsed = (uint64_t)now - rtc_state.last_send_timestamp;

    if (elapsed >= SEND_INTERVAL_S) {
        return 0; // Send overdue
    }

    return SEND_INTERVAL_S - (uint32_t)elapsed;
}