/**
* @file task_send_data.h
 *
 * Send buffered data to sensor API.
 *
 * Copyright (c) 2025 Caden Howell (cadenhowell@gmail.com)
 *
 * Developed with assistance from ChatGPT 4o (2025), Google Gemini 2.5 Pro (2025), and Claude Sonnet 4 (2025).
 *
 * Apache 2.0 Licensed as described in the file LICENSE
 */
#pragma once

#include "app_context.h"

/**
 * @brief Main FreeRTOS task for data sending
 * @param arg Pointer to app_context_t
 */
void task_send_data(void *arg);