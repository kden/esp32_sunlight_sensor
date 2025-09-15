/**
* @file time_utils.h
 *
 * Time utilities for power management.
 *
 * Copyright (c) 2025 Caden Howell (cadenhowell@gmail.com)
 *
 * Developed with assistance from Claude Sonnet 4 (2025).
 *
 * Apache 2.0 Licensed as described in the file LICENSE
 */

#pragma once

#include <stdbool.h>
#include <time.h>

/**
 * @brief Check if it's currently nighttime in the configured local timezone
 *
 * @return true if current local time is between configured night hours
 */
bool is_nighttime_local(void);

/**
 * @brief Log the current local time and day/night status
 */
void log_local_time_status(void);

/**
 * @brief Execute a function with local timezone temporarily set
 *
 * This helper function temporarily switches to the configured local timezone,
 * gets the current time, calls the provided callback function with local time
 * data, then restores the original timezone.
 *
 * @param func Callback function to execute with local timezone active
 * @param user_data Optional data pointer passed to the callback function
 */
void with_local_timezone(void (*func)(const struct tm*, time_t, void*), void* user_data);

/**
 * @brief Calculate sleep duration in microseconds until end of night period
 *
 * @return Sleep duration in microseconds, or 0 if not nighttime
 */
uint64_t calculate_night_sleep_duration_us(void);