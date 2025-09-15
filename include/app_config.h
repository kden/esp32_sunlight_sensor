/*
 * The MIT License (MIT)
 *
 * Copyright (c) 2024 Eric Gionet (gionet.c.eric@gmail.com)
 *
 * Permission is hereby granted, free of charge, to any person obtaining a copy
 * of this software and associated documentation files (the "Software"), to deal
 * in the Software without restriction, including without limitation the rights
 * to use, copy, modify, merge, publish, distribute, sublicense, and/or sell
 * copies of the Software, and to permit persons to whom the Software is
 * furnished to do so, subject to the following conditions:
 * The above copyright notice and this permission notice shall be included in all
 * copies or substantial portions of the Software.
 *
 * THE SOFTWARE IS PROVIDED "AS IS", WITHOUT WARRANTY OF ANY KIND, EXPRESS OR
 * IMPLIED, INCLUDING BUT NOT LIMITED TO THE WARRANTIES OF MERCHANTABILITY,
 * FITNESS FOR A PARTICULAR PURPOSE AND NONINFRINGEMENT. IN NO EVENT SHALL THE
 * AUTHORS OR COPYRIGHT HOLDERS BE LIABLE FOR ANY CLAIM, DAMAGES OR OTHER
 * LIABILITY, WHETHER IN AN ACTION OF CONTRACT, TORT OR OTHERWISE, ARISING FROM,
 * OUT OF OR IN CONNECTION WITH THE SOFTWARE OR THE USE OR OTHER DEALINGS IN THE
 * SOFTWARE.
 */

/**
 * @file app_config.h
 * @defgroup
 * @{
 *
 *
 * Copyright (c) 2024 Eric Gionet (gionet.c.eric@gmail.com)
 *
 * MIT Licensed as described in the file THIRD_PARTY_LICENSES
 */
#pragma once

#include <stdio.h>
#include <unistd.h>
#include <stdint.h>
#include <esp_log.h>
#include <driver/i2c_master.h>
#include <freertos/FreeRTOS.h>
#include <freertos/task.h>
#include "generated_config.h"

#define TSK_MINIMAL_STACK_SIZE         (1024)
#define I2C0_MASTER_PORT               I2C_NUM_0

#define I2C0_TASK_SAMPLING_RATE        (10) // seconds
#define I2C0_TASK_STACK_SIZE           (TSK_MINIMAL_STACK_SIZE * 8)
#define I2C0_TASK_PRIORITY             (tskIDLE_PRIORITY + 2)

#define APP_TAG                         "SUNLIGHT SENSOR [APP]"

// Battery monitoring settings
#define BATTERY_VOLTAGE_DIVIDER_RATIO 2.0     // Two equal 10kΩ resistors
#define BATTERY_PRESENT_THRESHOLD_V 2.5       // Minimum voltage to consider battery present
#define BATTERY_LOW_THRESHOLD_V     3.2       // Low battery warning threshold
#define BATTERY_CRITICAL_THRESHOLD_V 3.0      // Critical battery threshold

static inline void vTaskDelaySecUntil(TickType_t *previousWakeTime, const uint sec) {
    const TickType_t xFrequency = ((sec * 1000) / portTICK_PERIOD_MS);
    vTaskDelayUntil( previousWakeTime, xFrequency );
}

/**@}*/
