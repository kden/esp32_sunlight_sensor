/**
 * @file task_send_data.c
 *
 * ESP-IDF task for sending buffered data to sensor API.
 * Clean version with no conditional compilation.
 *
 * Copyright (c) 2025 Caden Howell (cadenhowell@gmail.com)
 */

#include "task_send_data.h"
#include "data_sender_core.h"
#include "esp_wifi.h"
#include "esp_log.h"
#include "wifi.h"
#include "http.h"
#include "ntp.h"
#include "sdkconfig.h"
#include <stdlib.h>
#include <time.h>
#include <string.h>
#include <stdio.h>

#define TAG "SEND_DATA_TASK"

#define DATA_SEND_INTERVAL_MINUTES 5
#define DATA_SEND_INTERVAL_S (DATA_SEND_INTERVAL_MINUTES * 60)
#define UNSENT_BUFFER_SIZE (4 * 60 * (60 / 15))

// Internal storage for unsent readings - always static in production
static sensor_reading_t g_unsent_readings_storage[UNSENT_BUFFER_SIZE];
static int g_unsent_readings_count = 0;

// Conversion helpers between ESP-IDF types and pure business logic types
static void convert_to_pure_readings(const sensor_reading_t* esp_readings, int count, reading_t* pure_readings) {
    for (int i = 0; i < count; i++) {
        pure_readings[i].timestamp = esp_readings[i].timestamp;
        pure_readings[i].lux = esp_readings[i].lux;
    }
}

static void convert_from_pure_readings(const reading_t* pure_readings, int count, sensor_reading_t* esp_readings) {
    for (int i = 0; i < count; i++) {
        esp_readings[i].timestamp = pure_readings[i].timestamp;
        esp_readings[i].lux = pure_readings[i].lux;
    }
}

// ESP-IDF Network Interface Implementation
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
    // Convert to ESP-IDF format and send
    sensor_reading_t esp_readings[count];
    convert_from_pure_readings(readings, count, esp_readings);

    esp_err_t result = send_sensor_data(esp_readings, count, CONFIG_SENSOR_ID, CONFIG_BEARER_TOKEN);
    return result == ESP_OK;
}

static bool esp_should_sync_time(time_t last_sync, time_t now) {
    return (now - last_sync) >= (60 * 60); // 1 hour
}

static void esp_sync_time(void) {
    initialize_sntp();

    // Log time after sync
    char time_str_buf[64];
    struct tm timeinfo;
    time_t now;
    time(&now);

    setenv("TZ", "UTC", 1);
    tzset();
    localtime_r(&now, &timeinfo);
    strftime(time_str_buf, sizeof(time_str_buf), "%Y-%m-%d %H:%M:%S", &timeinfo);

    setenv("TZ", "CST6CDT,M3.2.0,M11.1.0", 1);
    tzset();
    localtime_r(&now, &timeinfo);

    ESP_LOGI(TAG, "System time synced to %s (UTC) / %d:%02d:%02d (CST)",
             time_str_buf, timeinfo.tm_hour, timeinfo.tm_min, timeinfo.tm_sec);

    setenv("TZ", "UTC", 1); // Reset to UTC
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

// ESP-IDF Network Interface
static const network_interface_t esp_network_interface = {
    .is_network_connected = esp_is_network_connected,
    .connect_network = esp_connect_network,
    .disconnect_network = esp_disconnect_network,
    .send_data = esp_send_data,
    .should_sync_time = esp_should_sync_time,
    .sync_time = esp_sync_time,
    .get_power_mode = esp_get_power_mode,
    .log_message = esp_log_message
};

// Helper to send status updates
static void send_startup_status_updates(void) {
    if (wifi_is_connected()) {
        wifi_config_t wifi_config;
        if (esp_wifi_get_config(WIFI_IF_STA, &wifi_config) == ESP_OK) {
            char status_msg[64];
            snprintf(status_msg, sizeof(status_msg), "wifi connected to %s", (char *)wifi_config.sta.ssid);
            send_status_update(status_msg, CONFIG_SENSOR_ID, CONFIG_BEARER_TOKEN);
        } else {
            send_status_update("wifi connected", CONFIG_SENSOR_ID, CONFIG_BEARER_TOKEN);
        }
        send_status_update("ntp set", CONFIG_SENSOR_ID, CONFIG_BEARER_TOKEN);
    }
}

void task_send_data(void *arg) {
    app_context_t *context = (app_context_t *)arg;

    ESP_LOGI(TAG, "Data sending task started. Performing initial setup...");

    // Initial network connection and time sync
    esp_connect_network();
    if (wifi_is_connected()) {
        ESP_LOGI(TAG, "Initial network connection successful");
        esp_sync_time();
        send_startup_status_updates();
    } else {
        ESP_LOGE(TAG, "Failed initial network connection. Will retry on first send cycle.");
    }

    // Initialize pure business logic buffers
    reading_t unsent_storage[UNSENT_BUFFER_SIZE];
    reading_buffer_t unsent_buffer;
    init_reading_buffer(&unsent_buffer, unsent_storage, UNSENT_BUFFER_SIZE);

    time_t last_send_time;
    time(&last_send_time);
    time_t last_ntp_sync_time;
    time(&last_ntp_sync_time);

    while (1) {
        time_t now;
        time(&now);

        // Check if it's time to send data
        if (should_send_data(last_send_time, now, DATA_SEND_INTERVAL_S)) {
            ESP_LOGI(TAG, "Data send interval reached. Processing send cycle...");

            // Get current readings from shared buffer (ESP-IDF specific)
            reading_t current_storage[context->buffer_size];
            reading_buffer_t current_buffer;
            init_reading_buffer(&current_buffer, current_storage, context->buffer_size);

            if (xSemaphoreTake(context->buffer_mutex, portMAX_DELAY) == pdTRUE) {
                if (*(context->reading_idx) > 0) {
                    // Convert ESP-IDF readings to pure business logic format
                    convert_to_pure_readings(context->reading_buffer, *(context->reading_idx), current_storage);
                    current_buffer.count = *(context->reading_idx);
                    *(context->reading_idx) = 0; // Clear the shared buffer
                }
                xSemaphoreGive(context->buffer_mutex);
            }

            // Convert unsent readings to pure format
            reading_t temp_unsent[UNSENT_BUFFER_SIZE];
            convert_to_pure_readings(g_unsent_readings_storage, g_unsent_readings_count, temp_unsent);
            init_reading_buffer(&unsent_buffer, temp_unsent, UNSENT_BUFFER_SIZE);
            unsent_buffer.count = g_unsent_readings_count;

            // Call pure business logic
            send_result_t result = process_data_send_cycle(
                &current_buffer,
                &unsent_buffer,
                now,
                &last_ntp_sync_time,
                &esp_network_interface
            );

            // Convert back and update unsent storage
            convert_from_pure_readings(unsent_buffer.buffer, unsent_buffer.count, g_unsent_readings_storage);
            g_unsent_readings_count = unsent_buffer.count;

            // Log result
            switch (result) {
                case SEND_RESULT_SUCCESS:
                    ESP_LOGI(TAG, "Send cycle completed successfully");
                    break;
                case SEND_RESULT_NO_NETWORK:
                    ESP_LOGW(TAG, "Send cycle failed - no network connectivity");
                    break;
                case SEND_RESULT_SEND_FAILED:
                    ESP_LOGW(TAG, "Send cycle completed with some send failures");
                    break;
                case SEND_RESULT_NO_DATA:
                    ESP_LOGD(TAG, "Send cycle completed - no data to send");
                    break;
            }

            last_send_time = now;
        }

        // Sleep until next check
        vTaskDelay(pdMS_TO_TICKS(30000)); // Check every 30 seconds
    }
}