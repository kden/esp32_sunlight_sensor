/**
* @file task_send_data.h
 *
 * Send buffered data to sensor API.
 *
 * Copyright (c) 2025 Caden Howell (cadenhowell@gmail.com)
 *
 * Developed with assistance from ChatGPT 4o (2025) and Google Gemini 2.5 Pro (2025).
 *
 * Apache 2.0 Licensed as described in the file LICENSE
 */

#pragma once

#include "app_context.h" // For app_context_t
#include <time.h>         // For time_t

void task_send_data(void *arg);

#ifdef TEST
// This function is only compiled for testing environments
void process_data_send_cycle(app_context_t *context, time_t now, time_t *last_ntp_sync_time);
#endif