/**
* @file crash_handler.h
 *
 * Reports the device's reset reason on boot.
 *
 * Copyright (c) 2025 Caden Howell (cadenhowell@gmail.com)
 *
 * Developed with assistance from ChatGPT 4o (2025) and Google Gemini 2.5 Pro (2025).
 *
 * Apache 2.0 Licensed as described in the file LICENSE
 */

#pragma once

/**
 * @brief Checks the device's reset reason on boot and logs it.
 *
 * This function should be called early in the boot process.
 */
void check_and_report_crash(void);