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
#ifndef __APP_CONFIG_H__
#define __APP_CONFIG_H__

#include <stdio.h>
#include <unistd.h>
#include <stdint.h>
#include <esp_log.h>
#include <driver/i2c_master.h>
#include <freertos/FreeRTOS.h>
#include <freertos/task.h>

#define TSK_MINIMAL_STACK_SIZE         (1024)
#define I2C0_MASTER_PORT               I2C_NUM_0
 
// Provide default pin definitions if they are not set by the build environment
#ifndef CONFIG_SENSOR_SDA_GPIO
#define CONFIG_SENSOR_SDA_GPIO 21
#endif
#ifndef CONFIG_SENSOR_SCL_GPIO
#define CONFIG_SENSOR_SCL_GPIO 22
#endif
//
#define I2C0_TASK_SAMPLING_RATE        (10) // seconds
#define I2C0_TASK_STACK_SIZE           (TSK_MINIMAL_STACK_SIZE * 8)
#define I2C0_TASK_PRIORITY             (tskIDLE_PRIORITY + 2)

#define UTILS_TASK_SAMPLING_RATE       (30) // seconds
#define UTILS_TASK_STACK_SIZE          (TSK_MINIMAL_STACK_SIZE * 8)
#define UTILS_TASK_PRIORITY            (tskIDLE_PRIORITY + 2)

#define SCH_TASK_SAMPLING_RATE         (30) // seconds
#define SCH_TASK_STACK_SIZE            (TSK_MINIMAL_STACK_SIZE * 8)
#define SCH_TASK_PRIORITY              (tskIDLE_PRIORITY + 2)

#define APP_TAG                         "SUNLIGHT SENSOR [APP]"

// macros

#define I2C0_MASTER_CONFIG_DEFAULT {                                \
        .clk_source                     = I2C_CLK_SRC_DEFAULT,      \
        .i2c_port                       = I2C0_MASTER_PORT,         \
        .scl_io_num                     = CONFIG_SENSOR_SCL_GPIO,   \
        .sda_io_num                     = CONFIG_SENSOR_SDA_GPIO,   \
        .glitch_ignore_cnt              = 7,                        \
        .flags.enable_internal_pullup   = true, }


extern i2c_master_bus_config_t  i2c0_bus_cfg;
extern i2c_master_bus_handle_t  i2c0_bus_hdl;

static inline void vTaskDelaySecUntil(TickType_t *previousWakeTime, const uint sec) {
    const TickType_t xFrequency = ((sec * 1000) / portTICK_PERIOD_MS);
    vTaskDelayUntil( previousWakeTime, xFrequency );
}

/**@}*/

#endif  // __APP_CONFIG_H__
