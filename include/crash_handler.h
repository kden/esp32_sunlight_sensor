/**
* @file crash_handler.h
 *
 * Reports the device's reset reason on boot and sends logs for analysis.
 *
 * Copyright (c) 2025 Caden Howell (cadenhowell@gmail.com)
 *
 * Developed with assistance from ChatGPT 4o (2025) and Google Gemini 2.5 Pro (2025).
 *
 * Apache 2.0 Licensed as described in the file LICENSE
 */

#pragma once

/**
 * @brief Checks the device's reset reason on boot. If the last reset was
 *        due to a crash OR manual reset, it sends a status report with logs.
 *
 * This function should be called early in the boot process, after NVS has
 * been initialized. It will manage its own WiFi connection to send the report.
 */
void check_and_report_crash(void);