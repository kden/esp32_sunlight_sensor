/**
* @file wifi_monitor.h
 *
 * WiFi signal strength monitoring functions.
 *
 * Copyright (c) 2025 Caden Howell (cadenhowell@gmail.com)
 *
 * Developed with assistance from Claude Sonnet 4 (2025).
 *
 * Apache 2.0 Licensed as described in the file LICENSE
 */

#pragma once

#include "esp_err.h"
#include <stdbool.h>

/**
 * @brief Get the current WiFi signal strength (RSSI)
 *
 * @param rssi Pointer to store the RSSI value in dBm
 * @return esp_err_t ESP_OK on success, error code on failure
 */
esp_err_t wifi_get_rssi(int8_t *rssi);

/**
 * @brief Get WiFi signal quality as a percentage (0-100%)
 *
 * @param quality Pointer to store the quality percentage
 * @return esp_err_t ESP_OK on success, error code on failure
 */
esp_err_t wifi_get_signal_quality(uint8_t *quality);

/**
 * @brief Get WiFi status string for logging/reporting
 *
 * @param buffer Buffer to store the status string
 * @param buffer_size Size of the buffer
 * @return esp_err_t ESP_OK on success, error code on failure
 */
esp_err_t wifi_get_status_string(char *buffer, size_t buffer_size);

/**
 * @brief Get the current IP address as a string
 *
 * @param ip_str Buffer to store the IP address string
 * @param buffer_size Size of the buffer
 * @return esp_err_t ESP_OK on success, error code on failure
 */
esp_err_t wifi_get_ip_address(char *ip_str, size_t buffer_size);