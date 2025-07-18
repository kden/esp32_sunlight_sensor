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

#define TAG "SEND_DATA_TASK"

#define DATA_SEND_INTERVAL_MINUTES 5
#define DATA_SEND_INTERVAL_S (DATA_SEND_INTERVAL_MINUTES * 60)
#define NTP_SYNC_INTERVAL_S (60 * 60) // 1 hour
#define TASK_LOOP_CHECK_INTERVAL_S 30

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

void task_send_data(void *arg) {
    app_context_t *context = (app_context_t *)arg;

    ESP_LOGI(TAG, "Data sending task started. Performing initial NTP sync...");

    // Perform an initial connection and time sync immediately on startup.
    // This ensures the clock is accurate before the sensor task starts taking readings.
    wifi_manager_init();
    int retries = 15;
    while (!wifi_is_connected() && retries-- > 0) {
        vTaskDelay(pdMS_TO_TICKS(2000));
    }

    if (wifi_is_connected()) {
        ESP_LOGI(TAG, "Wi-Fi connected, performing initial time sync.");
        send_status_update("wifi connected", CONFIG_SENSOR_ID, CONFIG_BEARER_TOKEN);
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

                // Create a temporary buffer to hold data for sending
                sensor_reading_t temp_buffer[context->buffer_size];
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

                if (temp_count > 0) {
                    ESP_LOGI(TAG, "Sending %d batched readings.", temp_count);
                    send_sensor_data(temp_buffer, temp_count, CONFIG_SENSOR_ID, CONFIG_BEARER_TOKEN);
                } else {
                    ESP_LOGI(TAG, "No new readings to send.");
                }

                esp_wifi_disconnect();
                esp_wifi_stop();
                ESP_LOGI(TAG, "Wi-Fi disconnected to save power.");
            } else {
                ESP_LOGE(TAG, "Failed to connect to Wi-Fi. Will retry in %d minutes.", DATA_SEND_INTERVAL_MINUTES);
            }
            last_send_time = time(NULL); // Update send time regardless of success
        }
        vTaskDelay(pdMS_TO_TICKS(TASK_LOOP_CHECK_INTERVAL_S * 1000)); // Check periodically
    }
}