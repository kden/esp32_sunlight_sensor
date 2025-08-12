/**
 * @file task_send_data.c
 *
 * Send buffered data to sensor API.
 *
 * Copyright (c) 2025 Caden Howell (cadenhowell@gmail.com)
 *
 * Developed with assistance from ChatGPT 4o (2025) and Google Gemini 2.5 Pro (2025).
 *
 * Apache 2.0 Licensed as described in the file LICENSE
 */

#include "task_send_data.h"
#include "http.h"
#include "app_context.h"
#include "ntp.h"
#include "wifi.h"
#include "esp_wifi.h"
#include "esp_log.h"
#include "sdkconfig.h"
#include <stdlib.h>
#include <time.h>
#include <string.h> // Required for strcmp

#define TAG "SEND_DATA_TASK"

#define DATA_SEND_INTERVAL_MINUTES 5
#define DATA_SEND_INTERVAL_S (DATA_SEND_INTERVAL_MINUTES * 60)
#define NTP_SYNC_INTERVAL_S (60 * 60) // 1 hour
#define TASK_LOOP_CHECK_INTERVAL_S 30

// Buffer for readings that failed to send. Can store up to 4 hours of data.
// 4 hours * 60 min/hr * 4 readings/min (at 15s interval) = 960 readings
#define UNSENT_BUFFER_SIZE (4 * 60 * (60 / 15))
static sensor_reading_t g_unsent_readings_buffer[UNSENT_BUFFER_SIZE];
static int g_unsent_readings_count = 0;

/**
 * @brief Logs the current system time in both UTC and CST formats.
 */
static void log_system_time(void)
{
    char time_str_buf[64];
    struct tm timeinfo;
    time_t now;
    time(&now);

    // Set timezone to UTC and format the time
    setenv("TZ", "UTC", 1);
    tzset();
    localtime_r(&now, &timeinfo);
    strftime(time_str_buf, sizeof(time_str_buf), "%Y-%m-%d %H:%M:%S", &timeinfo);

    // Temporarily set timezone to CST/CDT to format the same time value
    setenv("TZ", "CST6CDT,M3.2.0,M11.1.0", 1);
    tzset();
    localtime_r(&now, &timeinfo);

    ESP_LOGI(TAG, "System time successfully set to %s (UTC) / %d:%02d:%02d (CST)",
             time_str_buf, timeinfo.tm_hour, timeinfo.tm_min, timeinfo.tm_sec);

    // Set back to UTC, this is our default.
    setenv("TZ", "UTC", 1);
}

/**
 * @brief Adds a batch of readings to the unsent buffer, handling overflow by dropping the oldest readings.
 * @param readings Pointer to the array of readings to add.
 * @param count Number of readings to add.
 */
static void add_to_unsent_buffer(const sensor_reading_t* readings, int count) {
    if (count <= 0) {
        return;
    }

    ESP_LOGI(TAG, "Adding %d readings to the unsent buffer.", count);

    // If the new readings won't fit, make space by dropping the oldest ones.
    if (g_unsent_readings_count + count > UNSENT_BUFFER_SIZE) {
        int overflow_count = (g_unsent_readings_count + count) - UNSENT_BUFFER_SIZE;
        ESP_LOGW(TAG, "Unsent buffer overflow. Dropping %d oldest readings to make space.", overflow_count);
        // Shift the existing data to the left
        memmove(&g_unsent_readings_buffer[0],
                &g_unsent_readings_buffer[overflow_count],
                (g_unsent_readings_count - overflow_count) * sizeof(sensor_reading_t));
        g_unsent_readings_count -= overflow_count;
    }

    // Copy the new readings to the end of the buffer
    memcpy(&g_unsent_readings_buffer[g_unsent_readings_count], readings, count * sizeof(sensor_reading_t));
    g_unsent_readings_count += count;

    ESP_LOGI(TAG, "Unsent buffer now contains %d readings.", g_unsent_readings_count);
}

// This function contains the core logic and is exposed for unit testing.
void process_data_send_cycle(app_context_t *context, time_t now, time_t *last_ntp_sync_time) {
    // 1. Get the latest batch of readings from the shared buffer.
    sensor_reading_t current_readings_buffer[context->buffer_size];
    int current_readings_count = 0;
    if (xSemaphoreTake(context->buffer_mutex, portMAX_DELAY) == pdTRUE) {
        if (*(context->reading_idx) > 0) {
            memcpy(current_readings_buffer, context->reading_buffer, *(context->reading_idx) * sizeof(sensor_reading_t));
            current_readings_count = *(context->reading_idx);
            *(context->reading_idx) = 0; // Clear the buffer index
        }
        xSemaphoreGive(context->buffer_mutex);
    }

    // 2. Attempt to connect to Wi-Fi.
    ESP_LOGI(TAG, "Data send interval reached. Checking Wi-Fi...");
    wifi_manager_init(); // Re-init in case it timed out or was stopped
    int connect_retries = 15;
    while (!wifi_is_connected() && connect_retries-- > 0) {
        vTaskDelay(pdMS_TO_TICKS(2000));
    }

    if (wifi_is_connected()) {
        ESP_LOGI(TAG, "Wi-Fi is connected. Processing data queue.");
        // Optional: NTP sync
        if ((now - *last_ntp_sync_time) >= NTP_SYNC_INTERVAL_S) {
            ESP_LOGI(TAG, "NTP sync interval reached. Synchronizing time.");
            initialize_sntp();
            log_system_time();
            *last_ntp_sync_time = time(NULL); // Update sync time
        }

        // 3a. Try to send any previously unsent data.
        if (g_unsent_readings_count > 0) {
            ESP_LOGI(TAG, "Attempting to send %d stored readings.", g_unsent_readings_count);
            if (send_sensor_data(g_unsent_readings_buffer, g_unsent_readings_count, CONFIG_SENSOR_ID, CONFIG_BEARER_TOKEN) == ESP_OK) {
                g_unsent_readings_count = 0;
                ESP_LOGI(TAG, "Stored readings sent successfully. Buffer cleared.");
            } else {
                ESP_LOGE(TAG, "Failed to send stored readings. They will be retried later.");
            }
        }

        // 3b. Send the current batch of data.
        if (current_readings_count > 0) {
            ESP_LOGI(TAG, "Sending %d new readings.", current_readings_count);
            if (send_sensor_data(current_readings_buffer, current_readings_count, CONFIG_SENSOR_ID, CONFIG_BEARER_TOKEN) != ESP_OK) {
                ESP_LOGE(TAG, "Failed to send new readings. Storing them for later.");
                add_to_unsent_buffer(current_readings_buffer, current_readings_count);
            }
        } else {
            ESP_LOGI(TAG, "No new readings to send in this cycle.");
        }

        // 4. Conditionally disconnect Wi-Fi.
        if (strcmp(CONFIG_SENSOR_POWER_DRAIN, "high") != 0) {
            esp_wifi_disconnect();
            esp_wifi_stop();
            ESP_LOGI(TAG, "Wi-Fi disconnected to save power (power_drain: %s).", CONFIG_SENSOR_POWER_DRAIN);
        } else {
            ESP_LOGI(TAG, "Keeping Wi-Fi connected (power_drain: high).");
        }
    } else {
        ESP_LOGE(TAG, "Failed to connect to Wi-Fi. Storing readings for later.");
        // If we couldn't connect, we must store the current readings.
        if (current_readings_count > 0) {
            add_to_unsent_buffer(current_readings_buffer, current_readings_count);
        }
    }
}

void task_send_data(void *arg) {
    app_context_t *context = (app_context_t *)arg;

    ESP_LOGI(TAG, "Data sending task started. Performing initial NTP sync...");

    // Perform an initial connection and time sync immediately on startup.
    wifi_manager_init();
    int retries = 15;
    while (!wifi_is_connected() && retries-- > 0) {
        vTaskDelay(pdMS_TO_TICKS(2000));
    }

    if (wifi_is_connected()) {
        ESP_LOGI(TAG, "Wi-Fi connected, performing initial time sync.");
        wifi_config_t wifi_config;
        if (esp_wifi_get_config(WIFI_IF_STA, &wifi_config) == ESP_OK) {
            char status_msg[64];
            snprintf(status_msg, sizeof(status_msg), "wifi connected to %s", (char *)wifi_config.sta.ssid);
            send_status_update(status_msg, CONFIG_SENSOR_ID, CONFIG_BEARER_TOKEN);
        } else {
            ESP_LOGE(TAG, "Failed to get Wi-Fi config, sending generic status.");
            send_status_update("wifi connected", CONFIG_SENSOR_ID, CONFIG_BEARER_TOKEN);
        }
        initialize_sntp();
        log_system_time();
        send_status_update("ntp set", CONFIG_SENSOR_ID, CONFIG_BEARER_TOKEN);
    } else {
        ESP_LOGE(TAG, "Failed to connect to Wi-Fi for initial NTP sync. Timestamps may be incorrect until next cycle.");
    }

    time_t last_send_time;
    time(&last_send_time); // Initialize with current time to start a full 5-minute cycle
    time_t last_ntp_sync_time;
    time(&last_ntp_sync_time); // Set initial sync time

    while (1) {
        time_t now;
        time(&now);

        // Check if it's time to send data
        if ((now - last_send_time) >= DATA_SEND_INTERVAL_S) {
            process_data_send_cycle(context, now, &last_ntp_sync_time);
            last_send_time = time(NULL); // Update send time regardless of success
        }
        vTaskDelay(pdMS_TO_TICKS(TASK_LOOP_CHECK_INTERVAL_S * 1000)); // Check periodically
    }
}