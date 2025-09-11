/**
* @file app_context.h
 *
 * Shared application context for tasks.
 *
 * Copyright (c) 2025 Caden Howell (cadenhowell@gmail.com)
 *
 * Developed with assistance from ChatGPT 4o (2025), Google Gemini 2.5 Pro (2025) and Claude Sonnet 4 (2025).
 *
 * Apache 2.0 Licensed as described in the file LICENSE
 */

#pragma once

#include "freertos/FreeRTOS.h"
#include "freertos/semphr.h"
#include "i2cdev.h" // Use the i2cdev descriptor type
#include "sensor_data.h"

/**
 * @brief A structure to hold shared resources for the application tasks.
 */
typedef struct {
    i2c_dev_t *light_sensor_dev; // Pointer to the i2c device descriptor
    sensor_reading_t *reading_buffer;
    int *reading_idx;
    int buffer_size;
    SemaphoreHandle_t buffer_mutex;
    bool wifi_send_failed;  // Flag to indicate if the last Wi-Fi send failed
} app_context_t;

