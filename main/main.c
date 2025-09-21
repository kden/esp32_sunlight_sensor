/**
* @file main.c
 *
 * ESP-IDF application to read and send ambient light levels using the BH1750 sensor.
 *
 * Copyright (c) 2025 Caden Howell (cadenhowell@gmail.com)
 *
 * Developed with assistance from ChatGPT 4o (2025), Google Gemini 2.5 Pro (2025) and Claude Sonnet 4 (2025).
 *
 * Apache 2.0 Licensed as described in the file LICENSE
 */

#include <stdio.h>
#include <esp_log.h>
#include <string.h>
#include "freertos/FreeRTOS.h"
#include "freertos/task.h"
#include "freertos/semphr.h"
#include "esp_netif.h"
#include "esp_wifi.h"
#include "app_config.h"
#include "sdkconfig.h"
#include "nvs_flash.h"
#include "task_send_data.h"
#include "task_get_sensor_data.h"
#include "git_version.h" // For build version info

#include "light_sensor.h"
#include "sensor_data.h"
#include "app_context.h"
#include "crash_handler.h"
#include "adc_battery.h"
#include "log_capture.h"
#include "time_utils.h"
#include "power_management.h"
#include "status_reporter.h"

#define TAG "MAIN"

#define BATCH_POST_INTERVAL_S (5 * 60) // 5 minutes
// We read every 15 seconds
#define READING_INTERVAL_S 15
#define READING_BUFFER_SIZE (BATCH_POST_INTERVAL_S / READING_INTERVAL_S)

// Shared data buffer and its current index
static sensor_reading_t g_reading_buffer[READING_BUFFER_SIZE];
static int g_reading_idx = 0;

void app_main(void)
{
    // Initialize NVS first
    esp_err_t err = nvs_flash_init();
    if (err == ESP_ERR_NVS_NO_FREE_PAGES || err == ESP_ERR_NVS_NEW_VERSION_FOUND) {
        ESP_ERROR_CHECK(nvs_flash_erase());
        err = nvs_flash_init();
    }
    ESP_ERROR_CHECK(err);

    // Initialize log capture EARLY - before other logging happens
    log_capture_init();

    // Filter out noisy WiFi system logs - ADD THESE LINES
    esp_log_level_set("wifi", ESP_LOG_WARN);
    esp_log_level_set("wifi_init", ESP_LOG_WARN);

    // Check wakeup reason first
    esp_sleep_wakeup_cause_t wakeup_reason = check_wakeup_reason();

    // Print the firmware version and build date
    ESP_LOGI(TAG, "Firmware Version: %s", GIT_COMMIT_SHA);
    ESP_LOGI(TAG, "Build Timestamp:  %s", GIT_COMMIT_TIMESTAMP);

    // Note: The i2cdev library handles I2C bus initialization implicitly
    // when we initialize the sensor, so we no longer need to do it here.

    // Create the application context to share resources with tasks
    app_context_t *app_context = malloc(sizeof(app_context_t));
    if (app_context == NULL) {
        ESP_LOGE(TAG, "Failed to allocate memory for app context!");
        return;
    }
    // Zero out the context
    memset(app_context, 0, sizeof(app_context_t));

    // Initialize the light sensor using the updated API
    ESP_ERROR_CHECK(init_light_sensor(&app_context->light_sensor_dev));

    // Initialize battery monitoring (safe to call on both ESP32-C3 and ESP32-S3)
    esp_err_t battery_err = adc_battery_init();  // Changed from battery_monitor_init()
    if (battery_err != ESP_OK) {
        ESP_LOGW(TAG, "Failed to initialize battery monitor: %s", esp_err_to_name(battery_err));
        ESP_LOGW(TAG, "Continuing without battery monitoring (this is normal for USB-powered devices)");
    } else {
        ESP_LOGI(TAG, "Battery monitoring initialized successfully");

        // Log initial battery status
        char battery_status[128];
        if (get_battery_status_string(battery_status, sizeof(battery_status))) {  // Changed function call
            ESP_LOGI(TAG, "Initial %s", battery_status);
        }
    }

    // Check if the last reset was due to a crash and report it.
    check_and_report_crash();

    // Log initial time status for debugging
    log_local_time_status();

    // Check if we should immediately go back to sleep (timer wakeup during night)
    if (wakeup_reason == ESP_SLEEP_WAKEUP_TIMER && is_nighttime_local()) {
        ESP_LOGI(TAG, "Timer wakeup during nighttime - checking if it's time to resume normal operation");

        // Try to enter sleep again - function will check conditions
        enter_night_sleep();
        // If we reach here, conditions weren't met for sleep (e.g., no battery, time changed)
        ESP_LOGI(TAG, "Sleep conditions no longer met - continuing with normal operation");
    }

    // Populate the rest of the application context
    app_context->reading_buffer = g_reading_buffer;
    app_context->reading_idx = &g_reading_idx;
    app_context->buffer_size = READING_BUFFER_SIZE;
    app_context->buffer_mutex = xSemaphoreCreateMutex();
    app_context->wifi_send_failed = false;  // Initialize failure flag

    if (app_context->buffer_mutex == NULL) {
        ESP_LOGE(TAG, "Failed to create mutex!");
        free(app_context);
        return;
    }

    // Create and launch the tasks with increased stack sizes to prevent stack overflow
    // Run send data task first to set the local clock correctly
    xTaskCreate(task_send_data, "send_data_task", 8192, app_context, 5, NULL);  // Increased from 4096
    // Give the send_data_task time to connect and perform the initial NTP sync
    vTaskDelay(pdMS_TO_TICKS(10000));
    xTaskCreate(task_get_sensor_data, "sensor_task", 6144, app_context, 5, NULL);  // Increased from 4096

    ESP_LOGI(TAG, "Initialization complete. Tasks are running.");
}