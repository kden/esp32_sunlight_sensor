/**
* @file network_manager.h
 *
 * WiFi and NTP coordination functions.
 *
 * Copyright (c) 2025 Caden Howell (cadenhowell@gmail.com)
 *
 * Developed with assistance from ChatGPT 4o (2025), Google Gemini 2.5 Pro (2025) and Claude Sonnet 4 (2025).
 *
 * Apache 2.0 Licensed as described in the file LICENSE
 */

#pragma once

#include <time.h>
#include <stdbool.h>
#include "esp_err.h"

/**
 * @brief Initialize network connection (WiFi)
 *
 * @param max_retries Maximum number of connection attempts
 * @return true if connected successfully, false otherwise
 */
bool initialize_network_connection(int max_retries);

/**
 * @brief Send WiFi connection status message
 *
 * @param is_initial_connection true for initial boot connection, false for periodic reconnections
 */
void send_wifi_connection_status(bool is_initial_connection);

/**
 * @brief Handle NTP synchronization based on current conditions
 *
 * @param last_ntp_sync_time Pointer to last sync timestamp (updated on successful sync)
 * @param is_initial_boot Whether this is the initial boot sync
 */
void handle_ntp_sync(time_t *last_ntp_sync_time, bool is_initial_boot);

/**
 * @brief Disconnect WiFi to save power
 */
void disconnect_wifi_for_power_saving(void);