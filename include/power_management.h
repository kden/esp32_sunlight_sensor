/**
* @file power_management.h
 *
 * Power management utilities including deep sleep for battery-powered devices.
 *
 * Copyright (c) 2025 Caden Howell (cadenhowell@gmail.com)
 *
 * Developed with assistance from Claude Sonnet 4 (2025).
 *
 * Apache 2.0 Licensed as described in the file LICENSE
 */

#pragma once

#include "esp_sleep.h"
#include <stdint.h>
#include <stdbool.h>

/**
 * @brief Check if device should enter deep sleep
 * @return true if conditions are met (ESP32-C3 + battery + nighttime)
 */
bool should_enter_deep_sleep(void);

/**
 * @brief Calculate how long to sleep during nighttime hours
 * @return Sleep duration in microseconds, or 0 if it's not nighttime
 */
uint64_t calculate_sleep_duration_us(void);

/**
 * @brief Enter deep sleep during nighttime hours (ESP32-C3 only)
 */
void enter_night_sleep(void);

/**
 * @brief Check and log the reason for waking up from deep sleep
 * @return The wakeup cause
 */
esp_sleep_wakeup_cause_t check_wakeup_reason(void);