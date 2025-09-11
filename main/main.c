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
#include "http.h"
#include "task_send_data.h"
#include "task_get_sensor_data.h"
#include "git_version.h" // For build version info

#include "light_sensor.h"
#include "sensor_data.h"
#include "app_context.h"
#include "crash_handler.h"
#include "persistent_storage.h"

#define TAG "MAIN"

#define MAX_WIFI_NETWORKS 5
#define MAX_SSID_LEN 32
#define MAX_PASS_LEN 64
#define BATCH_POST_INTERVAL_S (5 * 60) // 5 minutes
// We read every 15 seconds
#define READING_INTERVAL_S 15
#define READING_BUFFER_SIZE (BATCH_POST_INTERVAL_S / READING_INTERVAL_S)

// Shared data buffer and its current index
static sensor_reading_t g_reading_buffer[READING_BUFFER_SIZE];
static int g_reading_idx = 0;

void app_main(void)
{
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

    // Initialize non-volatile storage.
    esp_err_t err = nvs_flash_init();
    if (err == ESP_ERR_NVS_NO_FREE_PAGES || err == ESP_ERR_NVS_NEW_VERSION_FOUND) {
        ESP_ERROR_CHECK(nvs_flash_erase());
        err = nvs_flash_init();
    }
    ESP_ERROR_CHECK(err);

    // Initialize persistent storage for sensor readings
    err = persistent_storage_init();
    if (err != ESP_OK) {
        ESP_LOGE(TAG, "Failed to initialize persistent storage: %s", esp_err_to_name(err));
        ESP_LOGW(TAG, "Continuing without persistent storage (readings will be lost on Wi-Fi failure)");
    } else {
        ESP_LOGI(TAG, "Persistent storage initialized successfully");

        // Log how many readings are already stored
        int stored_count = 0;
        err = persistent_storage_get_count(&stored_count);
        if (err == ESP_OK && stored_count > 0) {
            ESP_LOGI(TAG, "Found %d stored readings from previous session", stored_count);
        }
    }

    // Check if the last reset was due to a crash and report it.
    check_and_report_crash();

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

