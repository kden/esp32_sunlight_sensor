/**
* @file task_send_data.c
 *
 * ESP-IDF task for sending buffered data to sensor API.
 * Clean version with no conditional compilation.
 *
 * Copyright (c) 2025 Caden Howell (cadenhowell@gmail.com)
 *
 * Developed with assistance from ChatGPT 4o (2025), Google Gemini 2.5 Pro (2025) and Claude Sonnet 4 (2025).
 *
 * Apache 2.0 Licensed as described in the file LICENSE
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
#include <inttypes.h>

#define TAG "SEND_DATA_TASK"

#define DATA_SEND_INTERVAL_MINUTES 5
#define DATA_SEND_INTERVAL_S (DATA_SEND_INTERVAL_MINUTES * 60)
#define UNSENT_BUFFER_SIZE (4 * 60 * (60 / 15))

// Internal storage for unsent readings - always static in production
static sensor_reading_t g_unsent_readings_storage[UNSENT_BUFFER_SIZE];
static int g_unsent_readings_count = 0;

// Conversion helpers between ESP-IDF types and pure business logic types
static void convert_to_pure_readings(const sensor_reading_t* esp_readings, int count, reading_t* pure_readings) {
    if (!esp_readings || !pure_readings || count <= 0) {
        ESP_LOGE(TAG, "Invalid parameters for convert_to_pure_readings");
        return;
    }

    for (int i = 0; i < count; i++) {
        pure_readings[i].timestamp = esp_readings[i].timestamp;
        pure_readings[i].lux = esp_readings[i].lux;
    }
}

static void convert_from_pure_readings(const reading_t* pure_readings, int count, sensor_reading_t* esp_readings) {
    if (!pure_readings || !esp_readings || count <= 0) {
        ESP_LOGE(TAG, "Invalid parameters for convert_from_pure_readings");
        return;
    }

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
    if (!readings || count <= 0) {
        ESP_LOGE(TAG, "Invalid parameters for esp_send_data");
        return false;
    }

    // Allocate on heap to avoid stack overflow for large counts
    sensor_reading_t* esp_readings = malloc(count * sizeof(sensor_reading_t));
    if (!esp_readings) {
        ESP_LOGE(TAG, "Failed to allocate memory for ESP readings conversion");
        return false;
    }

    convert_from_pure_readings(readings, count, esp_readings);
    esp_err_t result = send_sensor_data(esp_readings, count, CONFIG_SENSOR_ID, CONFIG_BEARER_TOKEN);

    free(esp_readings);
    return result == ESP_OK;
}

static bool esp_should_sync_time(time_t last_sync, time_t now) {
    return (now - last_sync) >= (60 * 60); // 1 hour
}

static void esp_sync_time(void) {
    // Check if SNTP is already running and stop it first
    if (esp_sntp_enabled()) {
        ESP_LOGI(TAG, "SNTP already running, stopping first");
        esp_sntp_stop();
        vTaskDelay(pdMS_TO_TICKS(100)); // Small delay to ensure cleanup
    }

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
    if (!level || !message) {
        return;
    }

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

// Helper to send status updates - with better error handling and delays
static void send_startup_status_updates(void) {
    if (!wifi_is_connected()) {
        ESP_LOGW(TAG, "Cannot send status updates - wifi not connected");
        return;
    }

    ESP_LOGI(TAG, "Sending startup status updates...");

    // Add longer delay to let network fully stabilize
    vTaskDelay(pdMS_TO_TICKS(5000)); // 5 seconds instead of 2

    // First status update with retry logic
    bool wifi_status_sent = false;
    for (int retry = 0; retry < 3 && !wifi_status_sent; retry++) {
        if (retry > 0) {
            ESP_LOGW(TAG, "Retrying wifi status update (attempt %d/3)", retry + 1);
            vTaskDelay(pdMS_TO_TICKS(2000)); // 2 second delay between retries
        }

        wifi_config_t wifi_config;
        if (esp_wifi_get_config(WIFI_IF_STA, &wifi_config) == ESP_OK) {
            char status_msg[64];
            int written = snprintf(status_msg, sizeof(status_msg), "wifi connected to %s", (char *)wifi_config.sta.ssid);
            if (written >= 0 && written < sizeof(status_msg)) {
                send_status_update(status_msg, CONFIG_SENSOR_ID, CONFIG_BEARER_TOKEN);
                wifi_status_sent = true;
                ESP_LOGI(TAG, "WiFi status update sent successfully");
            } else {
                ESP_LOGW(TAG, "Status message too long, trying generic message");
                send_status_update("wifi connected", CONFIG_SENSOR_ID, CONFIG_BEARER_TOKEN);
                wifi_status_sent = true;
            }
        } else {
            send_status_update("wifi connected", CONFIG_SENSOR_ID, CONFIG_BEARER_TOKEN);
            wifi_status_sent = true;
        }
    }

    if (!wifi_status_sent) {
        ESP_LOGE(TAG, "Failed to send WiFi status after 3 attempts - continuing anyway");
    }

    // Longer delay between status updates
    vTaskDelay(pdMS_TO_TICKS(3000)); // 3 seconds between status updates

    // Second status update with retry logic
    bool ntp_status_sent = false;
    for (int retry = 0; retry < 3 && !ntp_status_sent; retry++) {
        if (retry > 0) {
            ESP_LOGW(TAG, "Retrying NTP status update (attempt %d/3)", retry + 1);
            vTaskDelay(pdMS_TO_TICKS(2000));
        }

        send_status_update("ntp set", CONFIG_SENSOR_ID, CONFIG_BEARER_TOKEN);
        ntp_status_sent = true;
        ESP_LOGI(TAG, "NTP status update sent successfully");
    }

    if (!ntp_status_sent) {
        ESP_LOGE(TAG, "Failed to send NTP status after 3 attempts - continuing anyway");
    }

    ESP_LOGI(TAG, "Startup status updates completed");
}

void task_send_data(void *arg) {
    app_context_t *context = (app_context_t *)arg;

    if (!context) {
        ESP_LOGE(TAG, "Invalid context passed to task_send_data");
        vTaskDelete(NULL);
        return;
    }

    ESP_LOGI(TAG, "Data sending task started. Performing initial setup...");

    // Check available heap before proceeding
    uint32_t free_heap = esp_get_free_heap_size();
    ESP_LOGI(TAG, "Free heap at start: %" PRIu32 " bytes", free_heap);

    if (free_heap < 50000) { // Less than 50KB free
        ESP_LOGW(TAG, "Low heap memory detected: %" PRIu32 " bytes", free_heap);
    }

    // Initial network connection and time sync
    esp_connect_network();
    if (wifi_is_connected()) {
        ESP_LOGI(TAG, "Initial network connection successful");
        esp_sync_time();

        // Only send status updates if we have enough heap
        uint32_t heap_after_sync = esp_get_free_heap_size();
        ESP_LOGI(TAG, "Free heap after sync: %" PRIu32 " bytes", heap_after_sync);

        if (heap_after_sync > 30000) { // Only if we have >30KB free
            send_startup_status_updates();
        } else {
            ESP_LOGW(TAG, "Skipping status updates due to low heap: %" PRIu32 " bytes", heap_after_sync);
        }
    } else {
        ESP_LOGE(TAG, "Failed initial network connection. Will retry on first send cycle.");
    }

    // Pre-allocate buffers to avoid repeated malloc/free
    reading_t* unsent_storage = malloc(UNSENT_BUFFER_SIZE * sizeof(reading_t));
    if (!unsent_storage) {
        ESP_LOGE(TAG, "Failed to allocate unsent storage buffer");
        vTaskDelete(NULL);
        return;
    }

    // Pre-allocate current readings buffer to avoid malloc in loop
    reading_t* current_storage = malloc(context->buffer_size * sizeof(reading_t));
    if (!current_storage) {
        ESP_LOGE(TAG, "Failed to allocate current storage buffer");
        free(unsent_storage);
        vTaskDelete(NULL);
        return;
    }

    ESP_LOGI(TAG, "Pre-allocated buffers. Free heap: %" PRIu32 " bytes", esp_get_free_heap_size());

    reading_buffer_t unsent_buffer;
    init_reading_buffer(&unsent_buffer, unsent_storage, UNSENT_BUFFER_SIZE);

    time_t last_send_time;
    time(&last_send_time);
    time_t last_ntp_sync_time;
    time(&last_ntp_sync_time);

    // Heap monitoring counter
    int heap_check_counter = 0;

    while (1) {
        time_t now;
        time(&now);

        // Check if it's time to send data
        if (should_send_data(last_send_time, now, DATA_SEND_INTERVAL_S)) {
            ESP_LOGI(TAG, "Data send interval reached. Processing send cycle...");

            // Reuse pre-allocated buffer instead of malloc/free each time
            reading_buffer_t current_buffer;
            init_reading_buffer(&current_buffer, current_storage, context->buffer_size);

            // Get current readings from shared buffer (ESP-IDF specific)
            if (xSemaphoreTake(context->buffer_mutex, portMAX_DELAY) == pdTRUE) {
                if (*(context->reading_idx) > 0) {
                    // Bounds check
                    int readings_to_copy = *(context->reading_idx);
                    if (readings_to_copy > context->buffer_size) {
                        ESP_LOGW(TAG, "Reading index exceeds buffer size, clamping");
                        readings_to_copy = context->buffer_size;
                    }

                    // Convert ESP-IDF readings to pure business logic format
                    convert_to_pure_readings(context->reading_buffer, readings_to_copy, current_storage);
                    current_buffer.count = readings_to_copy;
                    *(context->reading_idx) = 0; // Clear the shared buffer
                }
                xSemaphoreGive(context->buffer_mutex);
            } else {
                ESP_LOGE(TAG, "Failed to take buffer mutex");
                continue; // Skip this cycle
            }

            // Convert unsent readings to pure format (with bounds checking)
            reading_t* temp_unsent = malloc(UNSENT_BUFFER_SIZE * sizeof(reading_t));
            if (!temp_unsent) {
                ESP_LOGE(TAG, "Failed to allocate temp unsent buffer");
                continue; // Skip this cycle
            }

            // Bounds check for unsent readings
            if (g_unsent_readings_count > UNSENT_BUFFER_SIZE) {
                ESP_LOGW(TAG, "Unsent readings count exceeds buffer size, clamping");
                g_unsent_readings_count = UNSENT_BUFFER_SIZE;
            }

            if (g_unsent_readings_count > 0) {
                convert_to_pure_readings(g_unsent_readings_storage, g_unsent_readings_count, temp_unsent);
            }

            // Re-initialize unsent buffer with current data
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

            // Convert back and update unsent storage (with bounds checking)
            if (unsent_buffer.count > UNSENT_BUFFER_SIZE) {
                ESP_LOGW(TAG, "Unsent buffer count exceeds storage size, clamping");
                unsent_buffer.count = UNSENT_BUFFER_SIZE;
            }

            if (unsent_buffer.count > 0) {
                convert_from_pure_readings(unsent_buffer.buffer, unsent_buffer.count, g_unsent_readings_storage);
            }
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

            // Clean up temp buffer
            free(temp_unsent);
        }

        // Periodic heap monitoring
        if (++heap_check_counter >= 10) { // Every 10 cycles (5 minutes)
            ESP_LOGI(TAG, "Free heap: %" PRIu32 " bytes", esp_get_free_heap_size());
            heap_check_counter = 0;
        }

        // Sleep until next check
        vTaskDelay(pdMS_TO_TICKS(30000)); // Check every 30 seconds
    }

    // This should never be reached, but clean up if it is
    free(current_storage);
    free(unsent_storage);
}