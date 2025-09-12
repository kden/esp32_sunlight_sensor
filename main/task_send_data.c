/**
* @file task_send_data.c
 *
 * Send buffered data to sensor API with improved time handling.
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

#include "app_config.h"
#include "battery_monitor.h"
#include "time_utils.h"
#include "power_management.h"

#define TAG "SEND_DATA_TASK"

#define DATA_SEND_INTERVAL_MINUTES 5
#define DATA_SEND_INTERVAL_S (DATA_SEND_INTERVAL_MINUTES * 60)
#define NTP_SYNC_INTERVAL_S (60 * 60) // 1 hour
#define NTP_RETRY_INTERVAL_S (5 * 60)  // 5 minutes for failed syncs
#define TASK_LOOP_CHECK_INTERVAL_S 30
#define MAX_HTTP_RETRY_ATTEMPTS 3
#define HTTP_RETRY_DELAY_MS 5000

// Forward declarations for functions used by sync_time_if_needed()
static void log_system_time(void);
static void calculate_time_correction(void);
static void format_time_status_message(char* buffer, size_t buffer_size, const char* prefix);
static bool send_status_update_with_retry(const char* status_message, const char* sensor_id, const char* bearer_token);

// Global time correction variables
static time_t g_time_correction_offset = 0;
static bool g_time_is_valid = false;
static bool g_initial_boot_complete = false; // Track if initial boot sync is complete

/**
 * @brief Attempt NTP sync if needed when internet connection is available
 * Call this whenever you successfully connect to the internet
 * @param send_status_update If true, send status update to API; if false, only log
 */
static bool sync_time_if_needed(bool send_status_update) {
    if (!is_system_time_valid() || !g_time_is_valid) {
        ESP_LOGI(TAG, "Time invalid, attempting NTP sync on connection");
        bool ntp_success = initialize_sntp();
        if (ntp_success) {
            log_system_time();
            calculate_time_correction();

            if (send_status_update) {
                char ntp_status_msg[128];
                format_time_status_message(ntp_status_msg, sizeof(ntp_status_msg), "ntp set");
                send_status_update_with_retry(ntp_status_msg, CONFIG_SENSOR_ID, CONFIG_BEARER_TOKEN);
            } else {
                ESP_LOGI(TAG, "NTP sync successful (status not sent - post-boot sync)");
            }
            return true;
        } else {
            ESP_LOGE(TAG, "NTP sync failed despite internet connection");
            if (send_status_update) {
                send_status_update_with_retry("ntp sync failed despite connection", CONFIG_SENSOR_ID, CONFIG_BEARER_TOKEN);
            }
            return false;
        }
    }
    return true; // Time was already valid
}

/**
 * @brief Apply time correction to a timestamp
 */
static time_t apply_time_correction(time_t original_timestamp) {
    if (g_time_is_valid && g_time_correction_offset != 0) {
        return original_timestamp + g_time_correction_offset;
    }
    return original_timestamp;
}

/**
 * @brief Calculate and store time correction offset when NTP sync succeeds
 */
static void calculate_time_correction(void) {
    if (!g_time_is_valid) {
        time_t now_corrected;
        time(&now_corrected);

        // If we had an invalid time before and now have valid time,
        // we can't really calculate a meaningful correction for past readings
        // since the old timestamps were essentially random
        ESP_LOGI(TAG, "Time correction established. Future readings will use correct time.");
        g_time_is_valid = true;
        g_time_correction_offset = 0; // No correction needed for future readings
    }
}

/**
 * @brief Create a corrected copy of sensor readings with fixed timestamps
 */
static sensor_reading_t* create_corrected_readings(const sensor_reading_t* original_readings, int count, int* corrected_count) {
    if (original_readings == NULL) {
        *corrected_count = 0;
        return NULL;
    }

    if (count <= 0) {
        *corrected_count = 0;
        return NULL;
    }

    sensor_reading_t* corrected = malloc(count * sizeof(sensor_reading_t));
    if (corrected == NULL) {
        ESP_LOGE(TAG, "Failed to allocate memory for corrected readings");
        *corrected_count = 0;
        return NULL;
    }

    *corrected_count = 0;
    time_t now;
    time(&now);

    for (int i = 0; i < count; i++) {
        corrected[*corrected_count] = original_readings[i];

        // Only include readings with reasonable timestamps
        time_t corrected_timestamp = apply_time_correction(original_readings[i].timestamp);

        // Skip readings that are still unreasonable even after correction
        if (corrected_timestamp < 1704067200) { // Before 2024
            ESP_LOGW(TAG, "Skipping reading with invalid timestamp: %lld", (long long)corrected_timestamp);
            continue;
        }

        // Don't send readings from the future (more than 1 hour ahead)
        if (corrected_timestamp > now + 3600) {
            ESP_LOGW(TAG, "Skipping reading with future timestamp: %lld", (long long)corrected_timestamp);
            continue;
        }

        corrected[*corrected_count].timestamp = corrected_timestamp;
        (*corrected_count)++;
    }

    if (*corrected_count == 0) {
        free(corrected);
        return NULL;
    }

    ESP_LOGI(TAG, "Corrected %d/%d readings", *corrected_count, count);
    return corrected;
}

/**
 * @brief Logs the current system time in both UTC and local timezone formats.
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

    // Set timezone to configured local timezone to format the same time value
    setenv("TZ", CONFIG_LOCAL_TIMEZONE, 1);
    tzset();
    localtime_r(&now, &timeinfo);

    ESP_LOGI(TAG, "System time: %s (UTC) / %d:%02d:%02d (local) [valid: %s]",
             time_str_buf, timeinfo.tm_hour, timeinfo.tm_min, timeinfo.tm_sec,
             is_system_time_valid() ? "yes" : "no");

    // Set back to UTC, this is our default.
    setenv("TZ", "UTC", 1);
    tzset();
}

/**
 * @brief Create a formatted time string with both UTC and local timestamps
 */
static void format_time_status_message(char* buffer, size_t buffer_size, const char* prefix)
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

    // Set timezone to configured local timezone to format the same time value
    setenv("TZ", CONFIG_LOCAL_TIMEZONE, 1);
    tzset();
    localtime_r(&now, &timeinfo);

    snprintf(buffer, buffer_size, "%s %s (UTC) / %d:%02d:%02d (local) [valid: %s]",
             prefix, time_str_buf, timeinfo.tm_hour, timeinfo.tm_min, timeinfo.tm_sec,
             is_system_time_valid() ? "yes" : "no");

    // Set back to UTC, this is our default.
    setenv("TZ", "UTC", 1);
    tzset();
}

/**
 * @brief Send sensor data with retry mechanism and timestamp correction
 */
static bool send_sensor_data_with_retry(const sensor_reading_t* readings, int count,
                                       const char* sensor_id, const char* bearer_token) {
    if (readings == NULL || count <= 0) {
        return true; // Nothing to send is considered success
    }

    // Create corrected readings
    int corrected_count;
    sensor_reading_t* corrected_readings = create_corrected_readings(readings, count, &corrected_count);

    if (corrected_readings == NULL || corrected_count == 0) {
        ESP_LOGW(TAG, "No valid readings to send after timestamp correction");
        if (corrected_readings) free(corrected_readings);
        return true; // No valid readings is considered success
    }

    bool success = false;
    for (int attempt = 1; attempt <= MAX_HTTP_RETRY_ATTEMPTS; attempt++) {
        ESP_LOGI(TAG, "Sensor data send attempt %d/%d (%d corrected readings)",
                 attempt, MAX_HTTP_RETRY_ATTEMPTS, corrected_count);

        // Use the version that returns proper error codes
        esp_err_t result = send_sensor_data_with_status(corrected_readings, corrected_count, sensor_id, bearer_token);

        if (result == ESP_OK) {
            ESP_LOGI(TAG, "Sensor data sent successfully on attempt %d", attempt);
            success = true;
            break;
        }

        ESP_LOGE(TAG, "Sensor data attempt %d failed: %s", attempt, esp_err_to_name(result));

        // Don't retry on certain errors (bad credentials, invalid data, etc.)
        if (result == ESP_ERR_INVALID_ARG || result == ESP_ERR_NOT_ALLOWED) {
            ESP_LOGE(TAG, "Non-retryable error, aborting retry attempts");
            break;
        }

        // Wait before retry
        if (attempt < MAX_HTTP_RETRY_ATTEMPTS) {
            ESP_LOGI(TAG, "Waiting %d ms before retry...", HTTP_RETRY_DELAY_MS);
            vTaskDelay(pdMS_TO_TICKS(HTTP_RETRY_DELAY_MS));
        }
    }

    free(corrected_readings);

    if (!success) {
        ESP_LOGE(TAG, "Sensor data send failed after %d attempts", MAX_HTTP_RETRY_ATTEMPTS);
    }

    return success;
}

/**
 * @brief Send status update with retry mechanism - NOW WITH REAL ERROR DETECTION
 */
static bool send_status_update_with_retry(const char* status_message, const char* sensor_id,
                                         const char* bearer_token) {
    for (int attempt = 1; attempt <= MAX_HTTP_RETRY_ATTEMPTS; attempt++) {
        ESP_LOGI(TAG, "Status update send attempt %d/%d", attempt, MAX_HTTP_RETRY_ATTEMPTS);

        // Use the version that returns proper error codes
        esp_err_t result = send_status_update_with_status(status_message, sensor_id, bearer_token);

        if (result == ESP_OK) {
            ESP_LOGI(TAG, "Status update sent successfully on attempt %d", attempt);
            return true;
        }

        ESP_LOGE(TAG, "Status update attempt %d failed: %s", attempt, esp_err_to_name(result));

        // Don't retry on certain errors (bad credentials, invalid data, etc.)
        if (result == ESP_ERR_INVALID_ARG || result == ESP_ERR_NOT_ALLOWED) {
            ESP_LOGE(TAG, "Non-retryable error, aborting retry attempts");
            break;
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
 * @brief Attempt to send stored readings from persistent storage with timestamp correction
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

    // Send the stored readings with retry mechanism and timestamp correction
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
    wifi_manager_init();
    int wifi_retries = 15;
    while (!wifi_is_connected() && wifi_retries-- > 0) {
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

        // Send battery status if available
        char battery_status[128];
        esp_err_t battery_err = battery_get_status_string(battery_status, sizeof(battery_status));
        if (battery_err == ESP_OK) {
            ESP_LOGI(TAG, "Battery status: %s", battery_status);
            send_status_update_with_retry(battery_status, CONFIG_SENSOR_ID, CONFIG_BEARER_TOKEN);
        } else {
            ESP_LOGW(TAG, "Failed to get battery status: %s", esp_err_to_name(battery_err));
        }

        // Always attempt NTP sync on successful connection if time is invalid
        // This is the initial boot sync, so send status update
        sync_time_if_needed(true);
        g_initial_boot_complete = true; // Mark initial boot as complete

        // Attempt to send any stored readings from previous sessions
        if (send_stored_readings()) {
            ESP_LOGI(TAG, "Successfully processed stored readings");
        } else {
            ESP_LOGW(TAG, "Failed to process stored readings, will retry later");
        }

        // Reset the failure flag since we're connected
        context->wifi_send_failed = false;
    } else {
        ESP_LOGE(TAG, "Failed to connect to Wi-Fi for initial NTP sync. Timestamps will be incorrect until next cycle.");
        context->wifi_send_failed = true;
        g_initial_boot_complete = true; // Mark initial boot as complete even if failed
    }

    time_t last_send_time;
    time(&last_send_time);
    time_t last_ntp_sync_time;
    time(&last_ntp_sync_time);

    while (1) {
        time_t now;
        time(&now);

        // Check if it's time to send data
        if ((now - last_send_time) >= DATA_SEND_INTERVAL_S) {
            // Log current time status
            log_local_time_status();

            // Check if it's nighttime - handle accordingly based on device
            if (is_nighttime_local()) {
                ESP_LOGI(TAG, "Nighttime detected");

                // Try deep sleep for ESP32-C3 with battery
                if (should_enter_deep_sleep()) {
                    ESP_LOGI(TAG, "Entering deep sleep mode");

                    // Give other tasks a moment to finish
                    vTaskDelay(pdMS_TO_TICKS(2000));

                    // Enter deep sleep (this will restart the system when it wakes up)
                    enter_night_sleep();
                    // This function does not return if sleep conditions are met
                } else {
                    // Skip transmission but stay awake (USB-powered or ESP32-S3)
                    ESP_LOGI(TAG, "Skipping data transmission for power savings (staying awake)");
                    last_send_time = time(NULL); // Update time to prevent immediate retry
                    continue;
                }
            }

            ESP_LOGI(TAG, "Data send interval reached. Connecting to Wi-Fi...");
            wifi_manager_init();

            // Wait for connection
            int connection_retries = 15;
            while (!wifi_is_connected() && connection_retries-- > 0) {
                vTaskDelay(pdMS_TO_TICKS(2000));
            }

            if (wifi_is_connected()) {
                // ALWAYS attempt NTP sync immediately if time is invalid, regardless of intervals
                // But only send status update if it's the initial boot (which should be complete by now)
                bool need_additional_ntp_sync = false;

                // First, sync time if needed (this handles the critical case)
                // Don't send status update since initial boot is complete
                sync_time_if_needed(false);

                // Then check if we need a regular interval-based sync
                if (is_system_time_valid() && g_time_is_valid && (now - last_ntp_sync_time) >= NTP_SYNC_INTERVAL_S) {
                    ESP_LOGI(TAG, "Regular NTP sync interval reached");
                    need_additional_ntp_sync = true;
                }

                if (need_additional_ntp_sync) {
                    bool ntp_success = initialize_sntp();
                    if (ntp_success) {
                        log_system_time();
                        calculate_time_correction();
                        last_ntp_sync_time = time(NULL);

                        // Only log the time status, don't send to API
                        char ntp_status_msg[128];
                        format_time_status_message(ntp_status_msg, sizeof(ntp_status_msg), "ntp set");
                        ESP_LOGI(TAG, "Regular NTP sync completed: %s", ntp_status_msg);
                    } else {
                        ESP_LOGE(TAG, "Regular NTP sync failed");
                    }
                }

                char battery_status[128];
                esp_err_t battery_err = battery_get_status_string(battery_status, sizeof(battery_status));
                if (battery_err == ESP_OK) {
                    ESP_LOGI(TAG, "Periodic battery status: %s", battery_status);
                    // Only send status update if battery is actually present to avoid spam
                    if (battery_is_present()) {
                        send_status_update_with_retry(battery_status, CONFIG_SENSOR_ID, CONFIG_BEARER_TOKEN);
                    }
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

