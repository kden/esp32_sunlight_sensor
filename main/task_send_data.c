/**
* @file task_send_data.c
 *
 * Main data sending task orchestration.
 *
 * Copyright (c) 2025 Caden Howell (cadenhowell@gmail.com)
 *
 * Developed with assistance from ChatGPT 4o (2025), Google Gemini 2.5 Pro (2025) and Claude Sonnet 4 (2025).
 *
 * Apache 2.0 Licensed as described in the file LICENSE
 */

#include "task_send_data.h"
#include "app_config.h"
#include "app_context.h"
#include "network_manager.h"
#include "data_processor.h"
#include "status_reporter.h"
#include "time_utils.h"
#include "power_management.h"
#include "esp_log.h"
#include <time.h>

#define TAG "SEND_DATA_TASK"

#define DATA_SEND_INTERVAL_MINUTES 5
#define DATA_SEND_INTERVAL_S (DATA_SEND_INTERVAL_MINUTES * 60)
#define TASK_LOOP_CHECK_INTERVAL_S 30

void task_send_data(void *arg) {
    app_context_t *context = (app_context_t *)arg;

    ESP_LOGI(TAG, "Data sending task started. Performing initial setup... (heap: %zu bytes)",
             esp_get_free_heap_size());

    // Initialize data processing and storage systems
    bool data_processor_available = data_processor_init();
    if (!data_processor_available) {
        ESP_LOGW(TAG, "Continuing without persistent storage (degraded mode)");
    }

    // Perform initial connection and time sync on startup
    time_t last_ntp_sync_time = time(NULL);

    ESP_LOGI(TAG, "Starting initial network connection (attempt 1 of 15)");
    if (initialize_network_connection(15)) {
        ESP_LOGI(TAG, "WiFi connected successfully, performing initial setup");

        // Send WiFi connection status (initial connection = true)
        ESP_LOGI(TAG, "Sending WiFi connection status");
        send_wifi_connection_status(true);

        // Send device status
        ESP_LOGI(TAG, "Sending device status");
        send_device_status_if_appropriate();

        // Perform initial NTP sync
        ESP_LOGI(TAG, "Performing initial NTP sync");
        handle_ntp_sync(&last_ntp_sync_time, true);

        // Send any stored readings from previous sessions
        ESP_LOGI(TAG, "Checking for stored readings from previous sessions");
        if (send_all_stored_readings()) {
            ESP_LOGI(TAG, "Successfully processed stored readings");
        } else {
            ESP_LOGW(TAG, "Failed to process stored readings, will retry later");
        }

        context->wifi_send_failed = false;
        ESP_LOGI(TAG, "Initial setup completed successfully");
    } else {
        ESP_LOGE(TAG, "Failed to connect to WiFi for initial setup. Will retry in next cycle.");
        context->wifi_send_failed = true;
    }

    time_t last_send_time = time(NULL);
    int cycle_count = 0;

    ESP_LOGI(TAG, "Entering main send loop (interval: %d minutes)", DATA_SEND_INTERVAL_MINUTES);

    while (1) {
        time_t now = time(NULL);
        cycle_count++;

        ESP_LOGD(TAG, "Send loop cycle %d - checking if send time reached (last: %ld, now: %ld, diff: %ld)",
                 cycle_count, last_send_time, now, now - last_send_time);

        // Check if it's time to send data
        if ((now - last_send_time) >= DATA_SEND_INTERVAL_S) {
            ESP_LOGI(TAG, "=== DATA SEND CYCLE %d START === (heap: %zu bytes)",
                     cycle_count, esp_get_free_heap_size());
            log_local_time_status();

            if (is_nighttime_local()) {
                ESP_LOGI(TAG, "Nighttime detected - evaluating power management options");

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

            ESP_LOGI(TAG, "Data send interval reached. Connecting to WiFi...");

            if (initialize_network_connection(15)) {
                ESP_LOGI(TAG, "Network connection established - proceeding with data operations");

                // Handle NTP synchronization
                ESP_LOGI(TAG, "Checking NTP synchronization requirements");
                handle_ntp_sync(&last_ntp_sync_time, false);

                // Send device status update
                ESP_LOGI(TAG, "Sending device status update");
                send_device_status_if_appropriate();

                // Send any stored readings first if previous send failed
                if (context->wifi_send_failed) {
                    ESP_LOGI(TAG, "Previous send failed, attempting to send stored readings first");
                    if (send_all_stored_readings()) {
                        ESP_LOGI(TAG, "Successfully sent stored readings");
                    } else {
                        ESP_LOGW(TAG, "Failed to send stored readings");
                    }
                }

                // Send current buffered readings
                bool send_success = true;
                if (*(context->reading_idx) > 0) {
                    ESP_LOGI(TAG, "Processing %d buffered readings", *(context->reading_idx));
                    send_success = process_buffered_readings(context, send_readings_processor);
                    if (send_success) {
                        ESP_LOGI(TAG, "Successfully processed buffered readings");
                    } else {
                        ESP_LOGE(TAG, "Failed to process buffered readings");
                    }
                } else {
                    ESP_LOGI(TAG, "No new readings to send.");
                }

                context->wifi_send_failed = !send_success;

                ESP_LOGI(TAG, "Disconnecting WiFi for power savings");
                disconnect_wifi_for_power_saving();

            } else {
                ESP_LOGE(TAG, "Failed to connect to WiFi. Will retry in %d minutes.", DATA_SEND_INTERVAL_MINUTES);
                context->wifi_send_failed = true;

                // Save current readings to persistent storage
                if (*(context->reading_idx) > 0) {
                    ESP_LOGI(TAG, "Saving %d readings to persistent storage due to WiFi failure",
                             *(context->reading_idx));
                    process_buffered_readings(context, save_readings_processor);
                }
            }
            last_send_time = time(NULL);
            ESP_LOGI(TAG, "=== DATA SEND CYCLE %d END ===", cycle_count);
        }

        ESP_LOGD(TAG, "Sleeping for %d seconds (cycle %d)", TASK_LOOP_CHECK_INTERVAL_S, cycle_count);
        vTaskDelay(pdMS_TO_TICKS(TASK_LOOP_CHECK_INTERVAL_S * 1000));
    }
}