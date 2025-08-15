/**
* @file ntp.h
 *
 * Client for time synchronization
 *
 * Copyright (c) 2025 Caden Howell (cadenhowell@gmail.com)
 *
 * Developed with assistance from ChatGPT 4o (2025), Google Gemini 2.5 Pro (2025) and Claude Sonnet 4 (2025).
 *
 * Apache 2.0 Licensed as described in the file LICENSE
 */

#pragma once

#include <stdbool.h>

/**
 * @brief Initialize SNTP and synchronize time
 * @return true if time synchronization was successful, false otherwise
 */
bool initialize_sntp(void);

/**
 * @brief Check if the system time is valid (reasonable timestamp)
 * @return true if system time appears valid, false otherwise
 */
bool is_system_time_valid(void);