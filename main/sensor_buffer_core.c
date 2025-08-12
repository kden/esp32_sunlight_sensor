/**
 * @file sensor_buffer_core.c
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
#include "sensor_buffer_core.h"
#include <stdio.h>

static int g_buffer_full_warning_count = 0;
static const int MAX_BUFFER_FULL_WARNINGS = 5;

buffer_result_t add_sensor_reading(
    sensor_buffer_t* buffer,
    time_t timestamp,
    float lux_value,
    const buffer_interface_t* interface)
{
    // Input validation - pure C, no ESP-IDF
    if (!buffer || !buffer->buffer || !buffer->current_count || !interface) {
        if (interface && interface->log_message) {
            interface->log_message("ERROR", "Invalid parameters for add_sensor_reading");
        }
        return BUFFER_RESULT_ERROR;
    }

    // Acquire lock through interface
    if (!interface->acquire_buffer_lock()) {
        interface->log_message("ERROR", "Failed to acquire buffer lock");
        return BUFFER_RESULT_ERROR;
    }

    buffer_result_t result = BUFFER_RESULT_SUCCESS;

    // Check if buffer has space
    if (*(buffer->current_count) < buffer->capacity) {
        // Normal case: add reading to buffer
        buffer->buffer[*(buffer->current_count)].timestamp = timestamp;
        buffer->buffer[*(buffer->current_count)].lux = lux_value;
        (*(buffer->current_count))++;

        char msg[128];
        snprintf(msg, sizeof(msg), "Reading #%d saved (Lux: %.2f)",
                *(buffer->current_count), lux_value);
        interface->log_message("INFO", msg);

        // Reset warning counter on successful add
        g_buffer_full_warning_count = 0;
    } else {
        // Buffer is full - apply back-pressure
        if (g_buffer_full_warning_count < MAX_BUFFER_FULL_WARNINGS) {
            char msg[128];
            snprintf(msg, sizeof(msg),
                    "Reading buffer full (%d/%d readings). Network may be down - send task should handle overflow.",
                    *(buffer->current_count), buffer->capacity);
            interface->log_message("WARN", msg);
            g_buffer_full_warning_count++;
        } else if (g_buffer_full_warning_count == MAX_BUFFER_FULL_WARNINGS) {
            interface->log_message("WARN", "Buffer full warnings suppressed (network appears to be down)");
            g_buffer_full_warning_count++;
        }

        result = BUFFER_RESULT_FULL;
    }

    // Release lock through interface
    interface->release_buffer_lock();
    return result;
}

bool should_drop_reading_when_full(void) {
    // Policy decision: For now, we drop readings when buffer is full
    // This preserves the existing buffered data for the send task
    // Alternative policy: could block/wait for space
    return true;
}

void init_sensor_buffer(
    sensor_buffer_t* buffer,
    sensor_data_t* storage,
    int* count_ptr,
    int capacity)
{
    if (!buffer || !storage || !count_ptr || capacity <= 0) {
        printf("  [ERROR] Invalid parameters for init_sensor_buffer\n");
        return;
    }

    buffer->buffer = storage;
    buffer->current_count = count_ptr;
    buffer->capacity = capacity;
}

int get_buffer_usage(const sensor_buffer_t* buffer) {
    if (!buffer || !buffer->current_count) {
        return 0;
    }
    return *(buffer->current_count);
}

bool is_buffer_full(const sensor_buffer_t* buffer) {
    if (!buffer || !buffer->current_count) {
        return false;
    }
    return *(buffer->current_count) >= buffer->capacity;
}

void clear_sensor_buffer(sensor_buffer_t* buffer) {
    if (!buffer || !buffer->current_count) {
        printf("  [ERROR] Invalid buffer passed to clear_sensor_buffer\n");
        return;
    }
    *(buffer->current_count) = 0;
}