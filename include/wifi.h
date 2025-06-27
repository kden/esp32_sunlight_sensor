/**
* @file wifi.h
 *
 * Utilities for working with the onboard WiFi..
 *
 * Copyright (c) 2025 Caden Howell (cadenhowell@gmail.com)
 *
 * Apache 2.0 Licensed as described in the file LICENSE
 */
#pragma once
#include "freertos/event_groups.h"


void wifi_init_sta(void);

esp_err_t wifi_get_mac_address(char *mac_str);
