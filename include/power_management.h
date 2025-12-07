/**
* @file power_management.h
 *
 * Power management utilities including deep sleep for ESP32-C3 battery-powered devices.
 *
 * Copyright (c) 2025 Caden Howell (cadenhowell@gmail.com)
 *
 * Developed with assistance from Claude Sonnet 4 (2025).
 *
 * Apache 2.0 Licensed as described in the file LICENSE
 */

#pragma once

#include <stdint.h>
#include <stdbool.h>
#include "esp_sleep.h"

// ============================================================================
// TESTING FLAGS
// ============================================================================

// Set to true to ignore nighttime detection and always operate in daytime mode
// This allows testing the 15-second reading cycle even during nighttime hours
#define IGNORE_NIGHTTIME_FOR_TESTING true

// Set to true to force deep sleep even when USB power is detected
// This allows testing deep sleep behavior while monitoring via USB serial
// WARNING: Serial output will be lost during deep sleep, use serial plotter or
// connect after wake to see logs
#define IGNORE_USB_POWER_FOR_TESTING true

// ============================================================================
// CONFIGURATION
// ============================================================================

// Reading interval in seconds (15 seconds)
#define READING_INTERVAL_S 15

// Send interval in seconds (5 minutes)
#define SEND_INTERVAL_S (5 * 60)

// RTC memory structure to persist across deep sleep
typedef struct {
    uint32_t boot_count;           // Total number of boots/wakes
    uint32_t readings_taken;       // Number of readings taken since last send
    uint64_t last_send_timestamp;  // Timestamp of last data send (in seconds since epoch)
    bool wifi_send_failed;         // Flag indicating if last send failed
    uint32_t magic;                // Magic number to validate RTC memory
} rtc_sleep_state_t;

// Magic number for validating RTC memory
#define RTC_MEMORY_MAGIC 0xDEADBEEF

/**
 * @brief Initialize RTC memory state
 *
 * @return true if RTC memory is valid and restored, false if initialized fresh
 */
bool rtc_state_init(void);

/**
 * @brief Get pointer to RTC state
 *
 * @return Pointer to RTC state structure
 */
rtc_sleep_state_t* rtc_state_get(void);

/**
 * @brief Save current state to RTC memory (explicit save point)
 */
void rtc_state_save(void);

/**
 * @brief Check if device should enter deep sleep (checks for battery presence)
 *
 * @return true if battery is present (should deep sleep), false if USB powered
 */
bool should_enter_deep_sleep(void);

/**
 * @brief Calculate sleep duration based on current time and schedule
 *
 * @param is_nighttime Whether it's currently nighttime
 * @return Sleep duration in microseconds
 */
uint64_t calculate_sleep_duration_us(bool is_nighttime);

/**
 * @brief Enter deep sleep with appropriate timer wakeup
 *
 * @param sleep_time_us Sleep duration in microseconds
 */
void enter_deep_sleep(uint64_t sleep_time_us);

/**
 * @brief Check and log the wakeup reason
 *
 * @return Wakeup cause
 */
esp_sleep_wakeup_cause_t check_wakeup_reason(void);

/**
 * @brief Check if it's time to send data
 *
 * @return true if send interval has elapsed
 */
bool is_time_to_send_data(void);

/**
 * @brief Get number of seconds until next send is due
 *
 * @return Seconds until next send, or 0 if send is overdue
 */
uint32_t seconds_until_next_send(void);