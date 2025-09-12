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