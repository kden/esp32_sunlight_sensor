/**
* @file time_utils.c
 *
 * Time utilities for power management.
 *
 * Copyright (c) 2025 Caden Howell (cadenhowell@gmail.com)
 *
 * Developed with assistance from Claude Sonnet 4 (2025).
 *
 * Apache 2.0 Licensed as described in the file LICENSE
 */

#include "time_utils.h"
#include "app_config.h"
#include <time.h>
#include <esp_log.h>

#define TAG "TIME_UTILS"

bool is_nighttime_local(void) {
    time_t now;
    struct tm local_time;

    time(&now);

    // Convert to local timezone (handles DST automatically)
    char* old_tz = getenv("TZ");
    setenv("TZ", CONFIG_LOCAL_TIMEZONE, 1);
    tzset();

    localtime_r(&now, &local_time);

    // Restore previous timezone
    if (old_tz) {
        setenv("TZ", old_tz, 1);
    } else {
        unsetenv("TZ");
    }
    tzset();

    int hour = local_time.tm_hour;

    // Night time is from configured start hour to configured end hour
    bool is_night = (hour >= CONFIG_NIGHT_START_HOUR || hour < CONFIG_NIGHT_END_HOUR);

    ESP_LOGD(TAG, "Local time: %02d:%02d, is_night: %s",
             hour, local_time.tm_min, is_night ? "true" : "false");

    return is_night;
}

void log_local_time_status(void) {
    time_t now;
    struct tm local_time;

    time(&now);

    // Convert to local timezone
    char* old_tz = getenv("TZ");
    setenv("TZ", CONFIG_LOCAL_TIMEZONE, 1);
    tzset();

    localtime_r(&now, &local_time);

    // Restore previous timezone
    if (old_tz) {
        setenv("TZ", old_tz, 1);
    } else {
        unsetenv("TZ");
    }
    tzset();

    const char* status = is_nighttime_local() ? "NIGHT (power save)" : "DAY (active)";

    ESP_LOGI(TAG, "Local time: %04d-%02d-%02d %02d:%02d:%02d (%02d:00-%02d:00) - %s",
             local_time.tm_year + 1900, local_time.tm_mon + 1, local_time.tm_mday,
             local_time.tm_hour, local_time.tm_min, local_time.tm_sec,
             CONFIG_NIGHT_START_HOUR, CONFIG_NIGHT_END_HOUR, status);
}