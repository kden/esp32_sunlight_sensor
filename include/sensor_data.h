/**
 * @file sensor_data.h
 *
 * Data structures for sensor readings.
 *
 * Copyright (c) 2025 Caden Howell (cadenhowell@gmail.com)
 *
 * Developed with assistance from ChatGPT 4o (2025) and Google Gemini 2.5 Pro (2025).
 *
 * Apache 2.0 Licensed as described in the file LICENSE
 */

#pragma once

#include <time.h>
#include <stdbool.h>

/**
 * @brief Structure to hold a single sensor reading for batching.
 */
typedef struct {
    time_t timestamp;
    float lux;
} sensor_reading_t;

typedef struct {
    time_t timestamp;
    float voltage;
    bool is_present;
    float percentage;  // Estimated percentage
} battery_reading_t;