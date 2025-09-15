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
#include "app_config.h"
#include "http.h"
#include "app_context.h"
#include "ntp.h"
#include "wifi.h"
#include "esp_wifi.h"
#include "esp_log.h"
#include "sdkconfig.h"
#include "persistent_storage.h"
#include "battery_monitor.h"
#include "time_utils.h"
#include "power_management.h"
#include "wifi_monitor.h"
#include <stdlib.h>
#include <time.h>
#include <string.h>

#define TAG "SEND_DATA_TASK"

#define DATA_SEND_INTERVAL_MINUTES 5
#define DATA_SEND_INTERVAL_S (DATA_SEND_INTERVAL_MINUTES * 60)
#define NTP_SYNC_INTERVAL_S (60 * 60) // 1 hour
#define NTP_RETRY_INTERVAL_S (5 * 60)  // 5 minutes for failed syncs
#define TASK_LOOP_CHECK_INTERVAL_S 30
#define MAX_HTTP_RETRY_ATTEMPTS 3
#define HTTP_RETRY_DELAY_MS 5000

// Forward declarations
static void format_time_status_message(char* buffer, size_t buffer_size, const char* prefix);
static bool send_status_update_with_retry(const char* status_message, const char* sensor_id, const char* bearer_token);

// Global time tracking variables
static bool g_time_is_valid = false;
static bool g_initial_boot_complete = false; // Track if initial boot sync is complete

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
 *
 * Used by log_system_time() to display comprehensive time information
 * for debugging NTP synchronization and timezone handling.
 *
 * @param local_time Current time in local timezone
 * @param now Current timestamp
 * @param user_data Unused (pass NULL)
 *
 * Example output: "System time: 2025-01-16 04:45:30 (UTC) / 22:45:30 (local) [valid: yes]"
 * Example output: "System time: 1970-01-01 00:00:15 (UTC) / 18:00:15 (local) [valid: no]"
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
 *
 * Creates a formatted string containing both UTC and local time information
 * suitable for sending as status updates to the remote API.
 *
 * @param local_time Current time in local timezone
 * @param now Current timestamp
 * @param user_data Pointer to time_format_data_t structure containing buffer and prefix
 *
 * Example output: "ntp set 2025-01-16 04:45:30 (UTC) / 22:45:30 (local) [valid: yes]"
 * Example output: "boot time 2025-01-16 04:45:30 (UTC) / 22:45:30 (local) [valid: no]"
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

/**
 * @brief Send WiFi connection status message with IP address and SSID
 *
 * Creates and sends a standardized WiFi connection status message containing
 * the connected SSID and IP address (if available).
 */
static void send_wifi_connection_status(void) {
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
        ESP_LOGE(TAG, "Failed to get Wi-Fi config, sending generic status.");
        if (ip_err == ESP_OK && rssi_err == ESP_OK) {
            snprintf(status_msg, sizeof(status_msg), "wifi connected IP %s %ddBm", ip_address, rssi);
        } else if (ip_err == ESP_OK) {
            snprintf(status_msg, sizeof(status_msg), "wifi connected IP %s", ip_address);
        } else {
            strcpy(status_msg, "wifi connected");
        }
    }

    send_status_update_with_retry(status_msg, CONFIG_SENSOR_ID, CONFIG_BEARER_TOKEN);
}

/**
 * @brief Process buffered sensor readings with automatic buffer management
 *
 * Safely allocates a temporary buffer, copies readings from the shared buffer,
 * clears the shared buffer, and calls the processor function.
 *
 * @param context Application context containing the shared buffer
 * @param processor Function to process the readings (send or save)
 * @return true if processing was successful, false on error
 */
static bool process_buffered_readings(app_context_t *context,
                                    bool (*processor)(sensor_reading_t*, int)) {
    sensor_reading_t *temp_buffer = malloc(context->buffer_size * sizeof(sensor_reading_t));
    if (temp_buffer == NULL) {
        ESP_LOGE(TAG, "Failed to allocate temporary buffer for readings");
        return false;
    }

    int temp_count = 0;
    bool success = true;

    if (xSemaphoreTake(context->buffer_mutex, portMAX_DELAY) == pdTRUE) {
        if (*(context->reading_idx) > 0) {
            memcpy(temp_buffer, context->reading_buffer,
                   *(context->reading_idx) * sizeof(sensor_reading_t));
            temp_count = *(context->reading_idx);
            *(context->reading_idx) = 0;
        }
        xSemaphoreGive(context->buffer_mutex);
    }

    if (temp_count > 0) {
        success = processor(temp_buffer, temp_count);
    }

    free(temp_buffer);
    return success;
}

/**
 * @brief Send combined device status (battery + wifi) if appropriate
 *
 * Gets the current device status and sends it to the API if a battery is present
 * or WiFi is connected to avoid spam.
 */
static void send_device_status_if_appropriate(void) {
    static bool initial_no_battery_sent = false;

    // Try to send detailed battery status first
    esp_err_t battery_result = send_battery_status_update(CONFIG_SENSOR_ID, CONFIG_BEARER_TOKEN);

    if (battery_result == ESP_OK) {
        ESP_LOGI(TAG, "Battery status sent successfully");
    } else if (battery_result == ESP_ERR_NOT_FOUND) {
        // No battery detected - only send this once after boot
        if (!initial_no_battery_sent) {
            send_status_update_with_retry("no battery detected", CONFIG_SENSOR_ID, CONFIG_BEARER_TOKEN);
            initial_no_battery_sent = true;
            ESP_LOGI(TAG, "Initial 'no battery' status sent");
        } else {
            ESP_LOGD(TAG, "Skipping repeated 'no battery' status");
        }
    } else {
        ESP_LOGW(TAG, "Failed to get battery status: %s", esp_err_to_name(battery_result));
    }
}

/**
 * @brief Handle NTP synchronization based on current conditions
 *
 * Performs NTP sync if time is invalid or regular interval has passed.
 * Manages sync timing and status reporting.
 *
 * @param last_ntp_sync_time Pointer to last sync timestamp (updated on successful sync)
 * @param is_initial_boot Whether this is the initial boot sync
 */
static void handle_ntp_sync(time_t *last_ntp_sync_time, bool is_initial_boot) {
    time_t now = time(NULL);
    bool need_sync = false;
    const char* sync_reason = "";

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
                    format_time_status_message(ntp_status_msg, sizeof(ntp_status_msg), "ntp set");
                    send_status_update_with_retry(ntp_status_msg, CONFIG_SENSOR_ID, CONFIG_BEARER_TOKEN);
                } else {
                    ESP_LOGI(TAG, "NTP sync successful (status not sent - post-boot sync)");
                }
                *last_ntp_sync_time = time(NULL);
            } else {
                ESP_LOGE(TAG, "NTP sync failed despite internet connection");
                if (is_initial_boot) {
                    send_status_update_with_retry("ntp sync failed despite connection", CONFIG_SENSOR_ID, CONFIG_BEARER_TOKEN);
                }
            }
        } else {
            // Regular interval sync
            bool ntp_success = initialize_sntp();
            if (ntp_success) {
                with_local_timezone(log_system_time_callback, NULL);
                *last_ntp_sync_time = time(NULL);

                char ntp_status_msg[128];
                format_time_status_message(ntp_status_msg, sizeof(ntp_status_msg), "ntp set");
                ESP_LOGI(TAG, "Regular NTP sync completed: %s", ntp_status_msg);
            } else {
                ESP_LOGE(TAG, "Regular NTP sync failed");
            }
        }
    }
}

/**
 * @brief Create a filtered copy of sensor readings with valid timestamps only
 */
static sensor_reading_t* create_filtered_readings(const sensor_reading_t* original_readings, int count, int* filtered_count) {
    if (original_readings == NULL) {
        *filtered_count = 0;
        return NULL;
    }

    if (count <= 0) {
        *filtered_count = 0;
        return NULL;
    }

    sensor_reading_t* filtered = malloc(count * sizeof(sensor_reading_t));
    if (filtered == NULL) {
        ESP_LOGE(TAG, "Failed to allocate memory for filtered readings");
        *filtered_count = 0;
        return NULL;
    }

    *filtered_count = 0;
    time_t now;
    time(&now);

    for (int i = 0; i < count; i++) {
        // Only include readings with reasonable timestamps
        time_t timestamp = original_readings[i].timestamp;

        // Skip readings that are unreasonable
        if (timestamp < 1704067200) { // Before 2024
            ESP_LOGW(TAG, "Skipping reading with invalid timestamp: %lld", (long long)timestamp);
            continue;
        }

        // Don't send readings from the future (more than 1 hour ahead)
        if (timestamp > now + 3600) {
            ESP_LOGW(TAG, "Skipping reading with future timestamp: %lld", (long long)timestamp);
            continue;
        }

        filtered[*filtered_count] = original_readings[i];
        (*filtered_count)++;
    }

    if (*filtered_count == 0) {
        free(filtered);
        return NULL;
    }

    ESP_LOGI(TAG, "Filtered %d/%d readings", *filtered_count, count);
    return filtered;
}

/**
 * @brief Create a formatted time string with both UTC and local timestamps
 *
 * Generates a status message string suitable for transmission to the remote API,
 * containing both UTC and local time information with the specified prefix.
 *
 * @param buffer Output buffer to store the formatted string
 * @param buffer_size Size of the output buffer
 * @param prefix Text to prepend to the time information (e.g., "ntp set", "boot time")
 */
static void format_time_status_message(char* buffer, size_t buffer_size, const char* prefix) {
    time_format_data_t data = {
        .buffer = buffer,
        .buffer_size = buffer_size,
        .prefix = prefix
    };
    with_local_timezone(format_time_status_callback, &data);
}

/**
 * @brief Processor function to send sensor readings
 */
static bool send_readings_processor(sensor_reading_t* readings, int count) {
    ESP_LOGI(TAG, "Sending %d batched readings.", count);

    // Create filtered readings
    int filtered_count;
    sensor_reading_t* filtered_readings = create_filtered_readings(readings, count, &filtered_count);

    if (filtered_readings == NULL || filtered_count == 0) {
        ESP_LOGW(TAG, "No valid readings to send after timestamp filtering");
        if (filtered_readings) free(filtered_readings);
        return true; // No valid readings is considered success
    }

    bool success = false;
    for (int attempt = 1; attempt <= MAX_HTTP_RETRY_ATTEMPTS; attempt++) {
        ESP_LOGI(TAG, "Sensor data send attempt %d/%d (%d filtered readings)",
                 attempt, MAX_HTTP_RETRY_ATTEMPTS, filtered_count);

        esp_err_t result = send_sensor_data_with_status(filtered_readings, filtered_count,
                                                       CONFIG_SENSOR_ID, CONFIG_BEARER_TOKEN);

        if (result == ESP_OK) {
            ESP_LOGI(TAG, "Sensor data sent successfully on attempt %d", attempt);
            success = true;
            break;
        }

        ESP_LOGE(TAG, "Sensor data attempt %d failed: %s", attempt, esp_err_to_name(result));

        if (result == ESP_ERR_INVALID_ARG || result == ESP_ERR_NOT_ALLOWED) {
            ESP_LOGE(TAG, "Non-retryable error, aborting retry attempts");
            break;
        }

        if (attempt < MAX_HTTP_RETRY_ATTEMPTS) {
            ESP_LOGI(TAG, "Waiting %d ms before retry...", HTTP_RETRY_DELAY_MS);
            vTaskDelay(pdMS_TO_TICKS(HTTP_RETRY_DELAY_MS));
        }
    }

    free(filtered_readings);

    if (!success) {
        ESP_LOGE(TAG, "Sensor data send failed after %d attempts", MAX_HTTP_RETRY_ATTEMPTS);
    }

    return success;
}

/**
 * @brief Processor function to save sensor readings to persistent storage
 */
static bool save_readings_processor(sensor_reading_t* readings, int count) {
    ESP_LOGI(TAG, "Saving %d readings to persistent storage due to Wi-Fi failure", count);
    esp_err_t storage_err = persistent_storage_save_readings(readings, count);
    if (storage_err != ESP_OK) {
        ESP_LOGE(TAG, "Failed to save readings to persistent storage: %s", esp_err_to_name(storage_err));
        return false;
    } else {
        ESP_LOGI(TAG, "Successfully saved readings to persistent storage");
        return true;
    }
}

/**
 * @brief Send status update with retry mechanism
 */
static bool send_status_update_with_retry(const char* status_message, const char* sensor_id,
                                         const char* bearer_token) {
    for (int attempt = 1; attempt <= MAX_HTTP_RETRY_ATTEMPTS; attempt++) {
        ESP_LOGI(TAG, "Status update send attempt %d/%d", attempt, MAX_HTTP_RETRY_ATTEMPTS);

        esp_err_t result = send_status_update(status_message, sensor_id, bearer_token);

        if (result == ESP_OK) {
            ESP_LOGI(TAG, "Status update sent successfully on attempt %d", attempt);
            return true;
        }

        ESP_LOGE(TAG, "Status update attempt %d failed: %s", attempt, esp_err_to_name(result));

        if (result == ESP_ERR_INVALID_ARG || result == ESP_ERR_NOT_ALLOWED) {
            ESP_LOGE(TAG, "Non-retryable error, aborting retry attempts");
            break;
        }

        if (attempt < MAX_HTTP_RETRY_ATTEMPTS) {
            ESP_LOGI(TAG, "Waiting %d ms before retry...", HTTP_RETRY_DELAY_MS);
            vTaskDelay(pdMS_TO_TICKS(HTTP_RETRY_DELAY_MS));
        }
    }

    ESP_LOGE(TAG, "Status update send failed after %d attempts", MAX_HTTP_RETRY_ATTEMPTS);
    return false;
}

/**
 * @brief Attempt to send stored readings from persistent storage with timestamp filtering
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
        return true;
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

    // Send the stored readings
    bool send_success = send_readings_processor(stored_readings, loaded_count);

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

/**
 * @brief Initialize persistent storage and log status
 */
static bool initialize_persistent_storage(void) {
    esp_err_t err = persistent_storage_init();
    if (err != ESP_OK) {
        ESP_LOGE(TAG, "Failed to initialize persistent storage: %s", esp_err_to_name(err));
        return false;
    }

    ESP_LOGI(TAG, "Persistent storage initialized successfully");

    int stored_count = 0;
    err = persistent_storage_get_count(&stored_count);
    if (err == ESP_OK && stored_count > 0) {
        ESP_LOGI(TAG, "Found %d stored readings from previous session", stored_count);
    }

    return true;
}

void task_send_data(void *arg) {
    app_context_t *context = (app_context_t *)arg;

    ESP_LOGI(TAG, "Data sending task started. Performing initial NTP sync...");

    // Initialize persistent storage
    bool persistent_storage_available = initialize_persistent_storage();
    if (!persistent_storage_available) {
        ESP_LOGW(TAG, "Continuing without persistent storage (degraded mode)");
    }

    // Perform initial connection and time sync on startup
    wifi_manager_init();
    int wifi_retries = 15;
    while (!wifi_is_connected() && wifi_retries-- > 0) {
        vTaskDelay(pdMS_TO_TICKS(2000));
    }

    time_t last_ntp_sync_time = time(NULL);

    if (wifi_is_connected()) {
        ESP_LOGI(TAG, "Wi-Fi connected, performing initial time sync.");

        // Send WiFi connection status
        send_wifi_connection_status();

        // Send device status
        send_device_status_if_appropriate();

        // Perform initial NTP sync
        handle_ntp_sync(&last_ntp_sync_time, true);
        g_initial_boot_complete = true;

        // Send any stored readings from previous sessions
        if (send_stored_readings()) {
            ESP_LOGI(TAG, "Successfully processed stored readings");
        } else {
            ESP_LOGW(TAG, "Failed to process stored readings, will retry later");
        }

        context->wifi_send_failed = false;
    } else {
        ESP_LOGE(TAG, "Failed to connect to Wi-Fi for initial NTP sync. Timestamps will be incorrect until next cycle.");
        context->wifi_send_failed = true;
        g_initial_boot_complete = true;
    }

    time_t last_send_time = time(NULL);

    while (1) {
        time_t now = time(NULL);

        // Check if it's time to send data
        if ((now - last_send_time) >= DATA_SEND_INTERVAL_S) {
            log_local_time_status();

            if (is_nighttime_local()) {
                ESP_LOGI(TAG, "Nighttime detected");

                if (should_enter_deep_sleep()) {
                    ESP_LOGI(TAG, "Entering deep sleep mode");
                    vTaskDelay(pdMS_TO_TICKS(2000));
                    enter_night_sleep();
                } else {
                    ESP_LOGI(TAG, "Skipping data transmission for power savings (staying awake)");
                    last_send_time = time(NULL);
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
                // Handle NTP synchronization
                handle_ntp_sync(&last_ntp_sync_time, false);

                // Send device status update
                send_device_status_if_appropriate();

                // Send any stored readings first if previous send failed
                if (context->wifi_send_failed) {
                    ESP_LOGI(TAG, "Previous send failed, attempting to send stored readings first");
                    if (send_stored_readings()) {
                        ESP_LOGI(TAG, "Successfully sent stored readings");
                    } else {
                        ESP_LOGW(TAG, "Failed to send stored readings");
                    }
                }

                // Send current buffered readings
                bool send_success = true;
                if (*(context->reading_idx) > 0) {
                    send_success = process_buffered_readings(context, send_readings_processor);
                } else {
                    ESP_LOGI(TAG, "No new readings to send.");
                }

                context->wifi_send_failed = !send_success;

                esp_wifi_disconnect();
                esp_wifi_stop();
                ESP_LOGI(TAG, "Wi-Fi disconnected to save power.");

            } else {
                ESP_LOGE(TAG, "Failed to connect to Wi-Fi. Will retry in %d minutes.", DATA_SEND_INTERVAL_MINUTES);
                context->wifi_send_failed = true;

                // Save current readings to persistent storage
                if (*(context->reading_idx) > 0) {
                    process_buffered_readings(context, save_readings_processor);
                }
            }
            last_send_time = time(NULL);
        }
        vTaskDelay(pdMS_TO_TICKS(TASK_LOOP_CHECK_INTERVAL_S * 1000));
    }
}