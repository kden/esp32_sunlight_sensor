/**
 * @file wifi.h
 *
 * Utilities for working with the onboard WiFi.
 *
 * Copyright (c) 2025 Caden Howell (cadenhowell@gmail.com)
 *
 * Developed with assistance from ChatGPT 4o (2025) and Google Gemini 2.5 Pro (2025).
 *
 * Apache 2.0 Licensed as described in the file LICENSE
 */
#pragma once

void wifi_manager_init(void);
bool wifi_is_connected(void);
esp_err_t wifi_get_mac_address(char *mac_str);
