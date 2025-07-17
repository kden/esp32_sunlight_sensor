/**
 * @file app_context.h
 *
 * Shared application context for tasks.
 *
 * Copyright (c) 2025 Caden Howell (cadenhowell@gmail.com)
 *
 * Apache 2.0 Licensed as described in the file LICENSE
 */

#pragma once

#include "freertos/FreeRTOS.h"
#include "freertos/semphr.h"
#include "bh1750.h"
#include "sensor_data.h"

/**
 * @brief A structure to hold shared resources for the application tasks.
 */
typedef struct {
    bh1750_handle_t light_sensor_hdl;
    sensor_reading_t *reading_buffer;
    int *reading_idx;
    int buffer_size;
    SemaphoreHandle_t buffer_mutex;
} app_context_t;