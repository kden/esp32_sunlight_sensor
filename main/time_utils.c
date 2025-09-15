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

// Wake up every 30 minutes during night to check if it's time to resume
#define NIGHT_CHECK_INTERVAL_US (30 * 60 * 1000000ULL)  // 30 minutes in microseconds

/**
 * @brief Execute a function with local timezone temporarily set
 *
 * This helper function temporarily switches to the configured local timezone,
 * gets the current time, calls the provided callback function with local time
 * data, then restores the original timezone. This centralizes the repetitive
 * timezone save/restore pattern used throughout the application.
 *
 * @param func Callback function to execute with local timezone active
 * @param user_data Optional data pointer passed to the callback function
 */
void with_local_timezone(void (*func)(const struct tm*, time_t, void*), void* user_data) {
    char* old_tz = getenv("TZ");
    setenv("TZ", CONFIG_LOCAL_TIMEZONE, 1);
    tzset();

    time_t now;
    time(&now);
    struct tm local_time;
    localtime_r(&now, &local_time);

    func(&local_time, now, user_data);

    // Restore timezone
    if (old_tz) {
        setenv("TZ", old_tz, 1);
    } else {
        unsetenv("TZ");
    }
    tzset();
}

/**
 * @brief Callback function to check if current time is nighttime
 *
 * Used by is_nighttime_local() to determine if the current local time falls
 * within the configured night hours (CONFIG_NIGHT_START_HOUR to CONFIG_NIGHT_END_HOUR).
 *
 * @param local_time Current time in local timezone
 * @param now Current timestamp (unused in this callback)
 * @param user_data Pointer to bool that will receive the nighttime status
 *
 * Example debug output: "Local time: 23:45, is_night: true"
 */
static void check_nighttime_callback(const struct tm* local_time, time_t now, void* user_data) {
    bool* is_night = (bool*)user_data;
    int hour = local_time->tm_hour;
    *is_night = (hour >= CONFIG_NIGHT_START_HOUR || hour < CONFIG_NIGHT_END_HOUR);

    ESP_LOGD(TAG, "Local time: %02d:%02d, is_night: %s",
             hour, local_time->tm_min, *is_night ? "true" : "false");
}

/**
 * @brief Callback function to log current local time status
 *
 * Generates a comprehensive log message showing the current local time,
 * configured night hours, and whether the device is in power-saving mode.
 *
 * @param local_time Current time in local timezone
 * @param now Current timestamp (unused in this callback)
 * @param user_data Unused (pass NULL)
 *
 * Example output: "Local time: 2025-01-15 23:45:30 (22:00-04:00) - NIGHT (power save)"
 * Example output: "Local time: 2025-01-15 14:30:15 (22:00-04:00) - DAY (active)"
 */
static void log_time_status_callback(const struct tm* local_time, time_t now, void* user_data) {
    bool is_night = (local_time->tm_hour >= CONFIG_NIGHT_START_HOUR ||
                     local_time->tm_hour < CONFIG_NIGHT_END_HOUR);
    const char* status = is_night ? "NIGHT (power save)" : "DAY (active)";

    ESP_LOGI(TAG, "Local time: %04d-%02d-%02d %02d:%02d:%02d (%02d:00-%02d:00) - %s",
             local_time->tm_year + 1900, local_time->tm_mon + 1, local_time->tm_mday,
             local_time->tm_hour, local_time->tm_min, local_time->tm_sec,
             CONFIG_NIGHT_START_HOUR, CONFIG_NIGHT_END_HOUR, status);
}

/**
 * @brief Callback function to calculate sleep duration
 *
 * Used by calculate_night_sleep_duration_us() to calculate how long to sleep
 * until the end of the night period, capped at the check interval.
 *
 * @param local_time Current time in local timezone
 * @param now Current timestamp (unused)
 * @param user_data Pointer to uint64_t that will receive the sleep duration
 */
static void calculate_sleep_callback(const struct tm* local_time, time_t now, void* user_data) {
    uint64_t* sleep_duration = (uint64_t*)user_data;

    int current_hour = local_time->tm_hour;
    int current_min = local_time->tm_min;

    // If we're not in night hours, don't sleep
    if (!(current_hour >= CONFIG_NIGHT_START_HOUR || current_hour < CONFIG_NIGHT_END_HOUR)) {
        *sleep_duration = 0;
        return;
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
    *sleep_duration = (uint64_t)minutes_until_wake * 60 * 1000000ULL;

    if (*sleep_duration > NIGHT_CHECK_INTERVAL_US) {
        *sleep_duration = NIGHT_CHECK_INTERVAL_US;
    }

    ESP_LOGI(TAG, "Calculated sleep duration: %llu minutes", *sleep_duration / (60 * 1000000ULL));
}

/**
 * @brief Check if it's currently nighttime in the configured local timezone
 *
 * Determines whether the current local time falls within the configured
 * nighttime hours for power management purposes.
 *
 * @return true if current local time is between CONFIG_NIGHT_START_HOUR and CONFIG_NIGHT_END_HOUR
 * @return false if current local time is during daytime hours
 */
bool is_nighttime_local(void) {
    bool is_night = false;
    with_local_timezone(check_nighttime_callback, &is_night);
    return is_night;
}

/**
 * @brief Log the current local time and day/night status
 *
 * Outputs a detailed log message showing current local time, configured
 * night hours, and current power management mode (day/night).
 */
void log_local_time_status(void) {
    with_local_timezone(log_time_status_callback, NULL);
}

/**
 * @brief Calculate sleep duration in microseconds until end of night period
 *
 * @return Sleep duration in microseconds, or 0 if not nighttime
 */
uint64_t calculate_night_sleep_duration_us(void) {
    uint64_t sleep_duration = 0;
    with_local_timezone(calculate_sleep_callback, &sleep_duration);
    return sleep_duration;
}