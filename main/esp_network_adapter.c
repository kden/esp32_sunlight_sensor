/**
 * @file esp_network_adapter.c
 *
 * Thin adapter that implements network_interface_t using ESP-IDF
 * This is the only send data task file that knows about ESP-IDF
 *
 * Copyright (c) 2025 Caden Howell (cadenhowell@gmail.com)
 *
 * Developed with assistance from Claude Sonnet 4 (2025).
 *
 * Apache 2.0 Licensed as described in the file LICENSE
 */

#include "data_sender_core.h"
#include "esp_wifi.h"
#include "esp_log.h"
#include "wifi.h"
#include "http.h"
#include "ntp.h"
#include "sdkconfig.h"
#include <string.h>

static const char* TAG = "ESP_NETWORK";

// ESP-IDF implementations of the interface
static bool esp_is_network_connected(void) {
    return wifi_is_connected();
}

static void esp_connect_network(void) {
    wifi_manager_init();
    int retries = 15;
    while (!wifi_is_connected() && retries-- > 0) {
        vTaskDelay(pdMS_TO_TICKS(2000));
    }
}

static void esp_disconnect_network(void) {
    esp_wifi_disconnect();
    esp_wifi_stop();
}

static bool esp_send_data(const reading_t* readings, int count) {
    // Convert from pure reading_t to ESP-IDF sensor_reading_t
    sensor_reading_t esp_readings[count];
    for (int i = 0; i < count; i++) {
        esp_readings[i].timestamp = readings[i].timestamp;
        esp_readings[i].lux = readings[i].lux;
    }

    return send_sensor_data(esp_readings, count, CONFIG_SENSOR_ID, CONFIG_BEARER_TOKEN) == ESP_OK;
}

static bool esp_should_sync_time(time_t last_sync, time_t now) {
    return (now - last_sync) >= (60 * 60); // 1 hour
}

static void esp_sync_time(void) {
    initialize_sntp();
}

static power_mode_t esp_get_power_mode(void) {
    return strcmp(CONFIG_SENSOR_POWER_DRAIN, "high") == 0 ? POWER_MODE_HIGH : POWER_MODE_LOW;
}

static void esp_log_message(const char* level, const char* message) {
    if (strcmp(level, "INFO") == 0) {
        ESP_LOGI(TAG, "%s", message);
    } else if (strcmp(level, "ERROR") == 0) {
        ESP_LOGE(TAG, "%s", message);
    } else if (strcmp(level, "WARN") == 0) {
        ESP_LOGW(TAG, "%s", message);
    }
}

// Create the ESP-IDF network interface
const network_interface_t* get_esp_network_interface(void) {
    static const network_interface_t esp_interface = {
        .is_network_connected = esp_is_network_connected,
        .connect_network = esp_connect_network,
        .disconnect_network = esp_disconnect_network,
        .send_data = esp_send_data,
        .should_sync_time = esp_should_sync_time,
        .sync_time = esp_sync_time,
        .get_power_mode = esp_get_power_mode,
        .log_message = esp_log_message
    };
    return &esp_interface;
}