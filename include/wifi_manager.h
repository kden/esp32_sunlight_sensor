/**
* @file wifi_manager.h
 *
 * Complete WiFi management including connection and monitoring.
 *
 * Copyright (c) 2025 Caden Howell (cadenhowell@gmail.com)
 *
 * Developed with assistance from ChatGPT 4o (2025), Google Gemini 2.5 Pro (2025) and Claude Sonnet 4 (2025).
 *
 * Apache 2.0 Licensed as described in the file LICENSE
 */

#pragma once

#include "esp_err.h"
#include <stdbool.h>
#include <stdint.h>

// Connection management
void wifi_manager_init(void);
bool wifi_is_connected(void);
esp_err_t wifi_get_mac_address(char *mac_str);

// Signal monitoring
esp_err_t wifi_get_rssi(int8_t *rssi);
esp_err_t wifi_get_signal_quality(uint8_t *quality);
esp_err_t wifi_get_status_string(char *buffer, size_t buffer_size);
esp_err_t wifi_get_ip_address(char *ip_str, size_t buffer_size);