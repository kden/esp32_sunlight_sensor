/**
* @file task_send_data.c
 *
 * Send buffered data to sensor API.
 *
 * Copyright (c) 2025 Caden Howell (cadenhowell@gmail.com)
 *
 * Developed with assistance from ChatGPT 4o (2025), Google Gemini 2.5 Pro (2025) and Claude Sonnet 4 (2025).
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
#include "persistent_storage.h"
#include <stdlib.h>
#include <time.h>
#include <string.h> // Required for strcmp

#define TAG "SEND_DATA_TASK"

#define DATA_SEND_INTERVAL_MINUTES 5
#define DATA_SEND_INTERVAL_S (DATA_SEND_INTERVAL_MINUTES * 60)
#define NTP_SYNC_INTERVAL_S (60 * 60) // 1 hour
#define TASK_LOOP_CHECK_INTERVAL_S 30
#define MAX_HTTP_RETRY_ATTEMPTS 3
#define HTTP_RETRY_DELAY_MS 5000

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
 * @brief Send sensor data with retry mechanism
 *
 * @param readings Sensor readings array
 * @param count Number of readings
 * @param sensor_id Sensor ID string
 * @param bearer_token Bearer token string
 * @return true if send was successful, false otherwise
 */
static bool send_sensor_data_with_retry(const sensor_reading_t* readings, int count,
                                       const char* sensor_id, const char* bearer_token) {
    for (int attempt = 1; attempt <= MAX_HTTP_RETRY_ATTEMPTS; attempt++) {
        ESP_LOGI(TAG, "Sensor data send attempt %d/%d", attempt, MAX_HTTP_RETRY_ATTEMPTS);

        // Attempt to send
        send_sensor_data(readings, count, sensor_id, bearer_token);

        // For now, we assume success since the send functions don't return status
        // In a future improvement, these functions should return esp_err_t
        ESP_LOGI(TAG, "Sensor data send attempt %d completed", attempt);

        // Consider successful after first attempt for now
        if (attempt == 1) {
            return true;
        }

        // Wait before retry
        if (attempt < MAX_HTTP_RETRY_ATTEMPTS) {
            ESP_LOGI(TAG, "Waiting %d ms before retry...", HTTP_RETRY_DELAY_MS);
            vTaskDelay(pdMS_TO_TICKS(HTTP_RETRY_DELAY_MS));
        }
    }

    ESP_LOGE(TAG, "Sensor data send failed after %d attempts", MAX_HTTP_RETRY_ATTEMPTS);
    return false;
}

/**
 * @brief Send status update with retry mechanism
 *
 * @param status_message Status message string
 * @param sensor_id Sensor ID string
 * @param bearer_token Bearer token string
 * @return true if send was successful, false otherwise
 */
static bool send_status_update_with_retry(const char* status_message, const char* sensor_id,
                                         const char* bearer_token) {
    for (int attempt = 1; attempt <= MAX_HTTP_RETRY_ATTEMPTS; attempt++) {
        ESP_LOGI(TAG, "Status update send attempt %d/%d", attempt, MAX_HTTP_RETRY_ATTEMPTS);

        // Attempt to send
        send_status_update(status_message, sensor_id, bearer_token);

        // For now, we assume success since the send functions don't return status
        // In a future improvement, these functions should return esp_err_t
        ESP_LOGI(TAG, "Status update send attempt %d completed", attempt);

        // Consider successful after first attempt for now
        if (attempt == 1) {
            return true;
        }

        // Wait before retry
        if (attempt < MAX_HTTP_RETRY_ATTEMPTS) {
            ESP_LOGI(TAG, "Waiting %d ms before retry...", HTTP_RETRY_DELAY_MS);
            vTaskDelay(pdMS_TO_TICKS(HTTP_RETRY_DELAY_MS));
        }
    }

    ESP_LOGE(TAG, "Status update send failed after %d attempts", MAX_HTTP_RETRY_ATTEMPTS);
    return false;
}

/**
 * @brief Attempt to send stored readings from persistent storage
 *
 * @return true if readings were sent successfully, false otherwise
 */
static bool send_stored_readings(void) {
    // Check if we have any stored readings first
    int stored_count = 0;
    esp_err_t err = persistent_storage_get_count(&stored_count);
    if (err != ESP_OK) {
        ESP_LOGE(TAG, "Failed to get stored reading count: %s", esp_err_to_name(err));
        return false;
    }

    if (stored_count == 0) {
        ESP_LOGI(TAG, "No stored readings to send");
        return true; // No readings to send is considered success
    }

    // Allocate memory for stored readings
    sensor_reading_t *stored_readings = malloc(PERSISTENT_STORAGE_MAX_READINGS * sizeof(sensor_reading_t));
    if (stored_readings == NULL) {
        ESP_LOGE(TAG, "Failed to allocate memory for stored readings");
        return false;
    }

    // Load stored readings
    int loaded_count = 0;
    err = persistent_storage_load_readings(stored_readings, PERSISTENT_STORAGE_MAX_READINGS, &loaded_count);
    if (err != ESP_OK) {
        ESP_LOGE(TAG, "Failed to load stored readings: %s", esp_err_to_name(err));
        free(stored_readings);
        return false;
    }

    if (loaded_count == 0) {
        ESP_LOGI(TAG, "No stored readings loaded");
        free(stored_readings);
        return true;
    }

    ESP_LOGI(TAG, "Attempting to send %d stored readings", loaded_count);

    // Send the stored readings with retry mechanism
    bool send_success = send_sensor_data_with_retry(stored_readings, loaded_count, CONFIG_SENSOR_ID, CONFIG_BEARER_TOKEN);

    if (send_success) {
        // Clear stored readings after successful send
        err = persistent_storage_clear_readings();
        if (err != ESP_OK) {
            ESP_LOGE(TAG, "Failed to clear stored readings after send: %s", esp_err_to_name(err));
            free(stored_readings);
            return false;
        }
        ESP_LOGI(TAG, "Successfully sent and cleared %d stored readings", loaded_count);
    } else {
        ESP_LOGE(TAG, "Failed to send stored readings");
    }

    free(stored_readings);
    return send_success;
}

void task_send_data(void *arg) {
    app_context_t *context = (app_context_t *)arg;

    ESP_LOGI(TAG, "Data sending task started. Performing initial NTP sync...");

    // Initialize persistent storage
    esp_err_t err = persistent_storage_init();
    if (err != ESP_OK) {
        ESP_LOGE(TAG, "Failed to initialize persistent storage: %s", esp_err_to_name(err));
        // Continue without persistent storage (degraded mode)
    } else {
        ESP_LOGI(TAG, "Persistent storage initialized successfully");

        // Log how many readings are already stored
        int stored_count = 0;
        err = persistent_storage_get_count(&stored_count);
        if (err == ESP_OK && stored_count > 0) {
            ESP_LOGI(TAG, "Found %d stored readings from previous session", stored_count);
        }
    }

    // Perform an initial connection and time sync immediately on startup.
    // This ensures the clock is accurate before the sensor task starts taking readings.
    wifi_manager_init();
    int retries = 15;
    while (!wifi_is_connected() && retries-- > 0) {
        vTaskDelay(pdMS_TO_TICKS(2000));
    }

    if (wifi_is_connected()) {
        ESP_LOGI(TAG, "Wi-Fi connected, performing initial time sync.");

        // Send WiFi connection status
        wifi_config_t wifi_config;
        if (esp_wifi_get_config(WIFI_IF_STA, &wifi_config) == ESP_OK) {
            char status_msg[64];
            snprintf(status_msg, sizeof(status_msg), "wifi connected to %s", (char *)wifi_config.sta.ssid);
            send_status_update_with_retry(status_msg, CONFIG_SENSOR_ID, CONFIG_BEARER_TOKEN);
        } else {
            ESP_LOGE(TAG, "Failed to get Wi-Fi config, sending generic status.");
            send_status_update_with_retry("wifi connected", CONFIG_SENSOR_ID, CONFIG_BEARER_TOKEN);
        }

        // Initialize NTP
        initialize_sntp();
        log_system_time();
        send_status_update_with_retry("ntp set", CONFIG_SENSOR_ID, CONFIG_BEARER_TOKEN);

        // Attempt to send any stored readings from previous sessions
        if (send_stored_readings()) {
            ESP_LOGI(TAG, "Successfully processed stored readings");
        } else {
            ESP_LOGW(TAG, "Failed to process stored readings, will retry later");
        }

        // Reset the failure flag since we're connected
        context->wifi_send_failed = false;
    } else {
        ESP_LOGE(TAG, "Failed to connect to Wi-Fi for initial NTP sync. Timestamps may be incorrect until next cycle.");
        context->wifi_send_failed = true;
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
            ESP_LOGI(TAG, "Data send interval reached. Connecting to Wi-Fi...");
            wifi_manager_init(); // Re-init in case it timed out

            // Wait for connection
            int retries = 15;
            while (!wifi_is_connected() && retries-- > 0) {
                vTaskDelay(pdMS_TO_TICKS(2000));
            }

            if (wifi_is_connected()) {
                // Check if it's time for an NTP sync
                if ((now - last_ntp_sync_time) >= NTP_SYNC_INTERVAL_S) {
                    ESP_LOGI(TAG, "NTP sync interval reached. Synchronizing time.");
                    initialize_sntp();
                    log_system_time();
                    last_ntp_sync_time = time(NULL); // Update sync time
                }

                // If we previously failed to send, try to send stored readings first
                if (context->wifi_send_failed) {
                    ESP_LOGI(TAG, "Previous send failed, attempting to send stored readings first");
                    if (send_stored_readings()) {
                        ESP_LOGI(TAG, "Successfully sent stored readings");
                    } else {
                        ESP_LOGW(TAG, "Failed to send stored readings");
                    }
                }

                // Create a temporary buffer to hold data for sending
                sensor_reading_t *temp_buffer = malloc(context->buffer_size * sizeof(sensor_reading_t));
                if (temp_buffer == NULL) {
                    ESP_LOGE(TAG, "Failed to allocate temporary buffer for readings");
                    context->wifi_send_failed = true;
                    last_send_time = time(NULL);
                    continue;
                }

                int temp_count = 0;

                // Lock the mutex to safely copy and clear the shared buffer
                if (xSemaphoreTake(context->buffer_mutex, portMAX_DELAY) == pdTRUE) {
                    if (*(context->reading_idx) > 0) {
                        memcpy(temp_buffer, context->reading_buffer, *(context->reading_idx) * sizeof(sensor_reading_t));
                        temp_count = *(context->reading_idx);
                        *(context->reading_idx) = 0; // Clear the buffer index
                    }
                    xSemaphoreGive(context->buffer_mutex);
                }

                bool send_success = true;
                if (temp_count > 0) {
                    ESP_LOGI(TAG, "Sending %d batched readings.", temp_count);
                    send_success = send_sensor_data_with_retry(temp_buffer, temp_count, CONFIG_SENSOR_ID, CONFIG_BEARER_TOKEN);
                } else {
                    ESP_LOGI(TAG, "No new readings to send.");
                }

                free(temp_buffer);

                // Mark send result
                context->wifi_send_failed = !send_success;


                    esp_wifi_disconnect();
                    esp_wifi_stop();
                    ESP_LOGI(TAG, "Wi-Fi disconnected to save power.");

            } else {
                ESP_LOGE(TAG, "Failed to connect to Wi-Fi. Will retry in %d minutes.", DATA_SEND_INTERVAL_MINUTES);

                // Mark send as failed
                context->wifi_send_failed = true;

                // Save current readings to persistent storage since we can't send them
                sensor_reading_t *temp_buffer = malloc(context->buffer_size * sizeof(sensor_reading_t));
                if (temp_buffer != NULL) {
                    int temp_count = 0;

                    // Lock the mutex to safely copy the shared buffer
                    if (xSemaphoreTake(context->buffer_mutex, portMAX_DELAY) == pdTRUE) {
                        if (*(context->reading_idx) > 0) {
                            memcpy(temp_buffer, context->reading_buffer, *(context->reading_idx) * sizeof(sensor_reading_t));
                            temp_count = *(context->reading_idx);
                            *(context->reading_idx) = 0; // Clear the buffer index
                        }
                        xSemaphoreGive(context->buffer_mutex);
                    }

                    if (temp_count > 0) {
                        ESP_LOGI(TAG, "Saving %d readings to persistent storage due to Wi-Fi failure", temp_count);
                        esp_err_t storage_err = persistent_storage_save_readings(temp_buffer, temp_count);
                        if (storage_err != ESP_OK) {
                            ESP_LOGE(TAG, "Failed to save readings to persistent storage: %s", esp_err_to_name(storage_err));
                        } else {
                            ESP_LOGI(TAG, "Successfully saved readings to persistent storage");
                        }
                    }

                    free(temp_buffer);
                } else {
                    ESP_LOGE(TAG, "Failed to allocate memory to save readings to persistent storage");
                }
            }
            last_send_time = time(NULL); // Update send time regardless of success
        }
        vTaskDelay(pdMS_TO_TICKS(TASK_LOOP_CHECK_INTERVAL_S * 1000)); // Check periodically
    }
}