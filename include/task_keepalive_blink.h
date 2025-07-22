/**
* @file task_keepalive_blink.h
 *
 * Blink the onboard LED so that the power supply doesn't fall asleep.
 *
 * Copyright (c) 2025 Caden Howell (cadenhowell@gmail.com)
 *
 * Developed with assistance from ChatGPT 4o (2025) and Google Gemini 2.5 Pro (2025).
 *
 * Apache 2.0 Licensed as described in the file LICENSE
 */

#pragma once

/**
 * @brief Initializes the keep-alive blink task.
 *
 * This function configures the onboard RGB LED and creates the FreeRTOS task
 * that will blink it. This provides a constant, low power draw to prevent
 * some USB power banks from automatically shutting down, and also serves as
 * a visual "heartbeat" for the device.
 */
void init_keepalive_blink_task(void);