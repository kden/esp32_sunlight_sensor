/**
* @file network_manager.c
 *
 * WiFi and NTP coordination functions.
 *
 * Copyright (c) 2025 Caden Howell (cadenhowell@gmail.com)
 *
 * Developed with assistance from ChatGPT 4o (2025), Google Gemini 2.5 Pro (2025) and Claude Sonnet 4 (2025).
 *
 * Apache 2.0 Licensed as described in the file LICENSE
 */

#include "network_manager.h"
#include "app_config.h"
#include "wifi_manager.h"
#include "esp_wifi.h"
#include "ntp.h"
#include "time_utils.h"
#include "status_reporter.h"
#include "esp_sleep.h"
#include "esp_log.h"
#include <string.h>

#define TAG "NETWORK_MANAGER"

// Global time tracking variables
static bool g_time_is_valid = false;

/**
 * @brief Helper structure for timezone callback functions
 */
typedef struct {
    char* buffer;
    size_t buffer_size;
    const char* prefix;
} time_format_data_t;

/**
 * @brief Callback function to log system time in both UTC and local formats
 */
static void log_system_time_callback(const struct tm* local_time, time_t now, void* user_data) {
    char time_str_buf[64];
    struct tm utc_time;

    // Format UTC time
    gmtime_r(&now, &utc_time);
    strftime(time_str_buf, sizeof(time_str_buf), "%Y-%m-%d %H:%M:%S", &utc_time);

    ESP_LOGI(TAG, "System time: %s (UTC) / %d:%02d:%02d (local) [valid: %s]",
             time_str_buf, local_time->tm_hour, local_time->tm_min, local_time->tm_sec,
             is_system_time_valid() ? "yes" : "no");
}

/**
 * @brief Callback function to format time status message for API transmission
 */
static void format_time_status_callback(const struct tm* local_time, time_t now, void* user_data) {
    time_format_data_t* data = (time_format_data_t*)user_data;
    char time_str_buf[64];
    struct tm utc_time;

    // Format UTC time
    gmtime_r(&now, &utc_time);
    strftime(time_str_buf, sizeof(time_str_buf), "%Y-%m-%d %H:%M:%S", &utc_time);

    snprintf(data->buffer, data->buffer_size, "%s %s (UTC) / %d:%02d:%02d (local) [valid: %s]",
             data->prefix, time_str_buf, local_time->tm_hour, local_time->tm_min, local_time->tm_sec,
             is_system_time_valid() ? "yes" : "no");
}

bool initialize_network_connection(int max_retries) {
    wifi_manager_init();
    int wifi_retries = max_retries;
    while (!wifi_is_connected() && wifi_retries-- > 0) {
        vTaskDelay(pdMS_TO_TICKS(2000));
    }
    return wifi_is_connected();
}

void send_wifi_connection_status(bool is_initial_connection) {
    wifi_config_t wifi_config;
    char ip_address[16];
    char status_msg[128];
    int8_t rssi;

    esp_err_t ip_err = wifi_get_ip_address(ip_address, sizeof(ip_address));
    esp_err_t rssi_err = wifi_get_rssi(&rssi);

    if (esp_wifi_get_config(WIFI_IF_STA, &wifi_config) == ESP_OK) {
        if (ip_err == ESP_OK && rssi_err == ESP_OK) {
            snprintf(status_msg, sizeof(status_msg), "wifi connected to %s IP %s %ddBm",
                     (char *)wifi_config.sta.ssid, ip_address, rssi);
        } else if (ip_err == ESP_OK) {
            snprintf(status_msg, sizeof(status_msg), "wifi connected to %s IP %s",
                     (char *)wifi_config.sta.ssid, ip_address);
        } else {
            snprintf(status_msg, sizeof(status_msg), "wifi connected to %s",
                     (char *)wifi_config.sta.ssid);
        }
    } else {
        ESP_LOGE(TAG, "Failed to get WiFi config, sending generic status.");
        if (ip_err == ESP_OK && rssi_err == ESP_OK) {
            snprintf(status_msg, sizeof(status_msg), "wifi connected IP %s %ddBm", ip_address, rssi);
        } else if (ip_err == ESP_OK) {
            snprintf(status_msg, sizeof(status_msg), "wifi connected IP %s", ip_address);
        } else {
            strcpy(status_msg, "wifi connected");
        }
    }

    // Only send status updates for initial connections or wake from sleep
    if (is_initial_connection || esp_sleep_get_wakeup_cause() == ESP_SLEEP_WAKEUP_TIMER) {
        send_status_update_with_retry(status_msg);
    }
}

void handle_ntp_sync(time_t *last_ntp_sync_time, bool is_initial_boot) {
    time_t now = time(NULL);
    bool need_sync = false;
    const char* sync_reason = "";
    const int NTP_SYNC_INTERVAL_S = 60 * 60; // 1 hour

    // Always sync if time is invalid
    if (!is_system_time_valid() || !g_time_is_valid) {
        need_sync = true;
        sync_reason = "Time invalid, performing NTP sync";
    }
    // Regular interval sync for valid time
    else if ((now - *last_ntp_sync_time) >= NTP_SYNC_INTERVAL_S) {
        need_sync = true;
        sync_reason = "Regular NTP sync interval reached";
    }

    if (need_sync) {
        ESP_LOGI(TAG, "%s", sync_reason);

        if (!is_system_time_valid() || !g_time_is_valid) {
            // Critical sync needed
            bool ntp_success = initialize_sntp();
            if (ntp_success) {
                with_local_timezone(log_system_time_callback, NULL);
                g_time_is_valid = true;

                if (is_initial_boot) {
                    char ntp_status_msg[128];
                    time_format_data_t data = {
                        .buffer = ntp_status_msg,
                        .buffer_size = sizeof(ntp_status_msg),
                        .prefix = "ntp set"
                    };
                    with_local_timezone(format_time_status_callback, &data);
                    send_status_update_with_retry(ntp_status_msg);
                } else {
                    ESP_LOGI(TAG, "NTP sync successful (status not sent - post-boot sync)");
                }
                *last_ntp_sync_time = time(NULL);
            } else {
                ESP_LOGE(TAG, "NTP sync failed despite internet connection");
                if (is_initial_boot) {
                    send_status_update_with_retry("ntp sync failed despite connection");
                }
            }
        } else {
            // Regular interval sync - don't send status messages for these
            bool ntp_success = initialize_sntp();
            if (ntp_success) {
                with_local_timezone(log_system_time_callback, NULL);
                *last_ntp_sync_time = time(NULL);
                ESP_LOGI(TAG, "Regular NTP sync completed successfully");
            } else {
                ESP_LOGE(TAG, "Regular NTP sync failed");
            }
        }
    }
}

void disconnect_wifi_for_power_saving(void) {
    esp_wifi_disconnect();
    esp_wifi_stop();
    ESP_LOGI(TAG, "WiFi disconnected to save power.");
}