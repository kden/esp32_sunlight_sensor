/**
* @file sensor_buffer_core.h
 *
 * Pure business logic for sensor data buffering with no ESP-IDF dependencies
 * Keep it this way for unit testing
 *
 * Copyright (c) 2025 Caden Howell (cadenhowell@gmail.com)
 *
 * Developed with assistance from Claude Sonnet 4 (2025).
 *
 * Apache 2.0 Licensed as described in the file LICENSE
 */
#pragma once

#include <time.h>
#include <stdbool.h>

// IMPORTANT: This header must NOT include any ESP-IDF headers!
// It should only use standard C library headers for pure testability.

// Pure data structure - matches sensor_reading_t but without ESP-IDF dependency
typedef struct {
    time_t timestamp;
    float lux;
} sensor_data_t;

typedef struct {
    sensor_data_t *buffer;
    int *current_count;  // Pointer to shared count
    int capacity;
} sensor_buffer_t;

typedef enum {
    BUFFER_RESULT_SUCCESS,
    BUFFER_RESULT_FULL,
    BUFFER_RESULT_ERROR
} buffer_result_t;

// Pure function interfaces for dependency injection
typedef struct {
    bool (*acquire_buffer_lock)(void);
    void (*release_buffer_lock)(void);
    void (*log_message)(const char* level, const char* message);
} buffer_interface_t;

// Core business logic functions - pure and testable
buffer_result_t add_sensor_reading(
    sensor_buffer_t* buffer,
    time_t timestamp,
    float lux_value,
    const buffer_interface_t* interface
);

bool should_drop_reading_when_full(void);

void init_sensor_buffer(
    sensor_buffer_t* buffer,
    sensor_data_t* storage,
    int* count_ptr,
    int capacity
);

int get_buffer_usage(const sensor_buffer_t* buffer);
bool is_buffer_full(const sensor_buffer_t* buffer);
void clear_sensor_buffer(sensor_buffer_t* buffer);