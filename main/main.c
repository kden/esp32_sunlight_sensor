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
#include "esp_pm.h"     // NEW: For power management control
#include "esp_timer.h"  // NEW: For esp_timer_get_time()
#include "app_config.h"
#include "sdkconfig.h"
#include "bh1750.h"
#include "nvs_flash.h"
#include "http.h"
#include "task_send_data.h"
#include "task_get_sensor_data.h"
#include "task_keepalive_transistor.h"  // NEW: Include the smart keep-alive

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
    // NEW: Disable power management to prevent sleep
    ESP_LOGI(TAG, "Disabling power management to prevent sleep modes");
    esp_pm_config_t pm_config = {
        .max_freq_mhz = 240,
        .min_freq_mhz = 240,         // Keep at max frequency - no frequency scaling
        .light_sleep_enable = false  // Explicitly disable light sleep
    };
    esp_err_t pm_err = esp_pm_configure(&pm_config);
    if (pm_err == ESP_OK) {
        ESP_LOGI(TAG, "Power management configured: max CPU, no sleep");
    } else {
        ESP_LOGE(TAG, "Failed to configure power management: %s", esp_err_to_name(pm_err));
        ESP_LOGW(TAG, "Continuing anyway, but ESP32 may still enter sleep modes");
    }

    // Initialize I2C
    i2c_master_bus_handle_t i2c0_bus_hdl;
    const i2c_master_bus_config_t i2c0_bus_cfg = I2C0_MASTER_CONFIG_DEFAULT;
    ESP_ERROR_CHECK(i2c_new_master_bus(&i2c0_bus_cfg, &i2c0_bus_hdl));

    // Initialize the light sensor
    bh1750_handle_t light_sensor_hdl = NULL;
    ESP_ERROR_CHECK(init_light_sensor(i2c0_bus_hdl, &light_sensor_hdl));

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

    // NEW: Initialize smart keep-alive system if configured
    #ifdef CONFIG_KEEPALIVE_GPIO
    keepalive_config_t keepalive_cfg = {
        #ifdef CONFIG_KEEPALIVE_DURATION_SECONDS
        .on_duration_seconds = CONFIG_KEEPALIVE_DURATION_SECONDS,
        #else
        .on_duration_seconds = 10,    // Default: 10 seconds ON
        #endif

        #ifdef CONFIG_KEEPALIVE_INTERVAL_MINUTES
        .interval_minutes = CONFIG_KEEPALIVE_INTERVAL_MINUTES,
        #else
        .interval_minutes = 4,        // Default: Every 4 minutes
        #endif

        .control_gpio = CONFIG_KEEPALIVE_GPIO
    };

    err = init_smart_keepalive_task(&keepalive_cfg);
    if (err != ESP_OK) {
        ESP_LOGE(TAG, "Failed to initialize smart keep-alive: %s", esp_err_to_name(err));
        ESP_LOGW(TAG, "Continuing without smart keep-alive (may have power issues)");
    } else {
        ESP_LOGI(TAG, "Smart keep-alive system initialized successfully");
    }
    #else
    ESP_LOGI(TAG, "Keep-alive system disabled (no keepalive_gpio configured in credentials.ini)");
    #endif

    // Check if the last reset was due to a crash and report it.
    check_and_report_crash();

    // Create the application context to share resources with tasks
    app_context_t *app_context = malloc(sizeof(app_context_t));
    if (app_context == NULL) {
        ESP_LOGE(TAG, "Failed to allocate memory for app context!");
        // In a real scenario, you might want to restart here.
        return;
    }

    app_context->light_sensor_hdl = light_sensor_hdl;
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

    // NEW: Main task monitoring loop to detect if system goes to sleep
    ESP_LOGI(TAG, "Starting main task monitoring loop to detect sleep issues");
    int loop_counter = 0;
    while(1) {
        loop_counter++;
        ESP_LOGI(TAG, "Main task alive - loop #%d, uptime: %d seconds",
                 loop_counter, (int)(esp_timer_get_time() / 1000000));

        // Log every 30 seconds to monitor for sleep
        vTaskDelay(pdMS_TO_TICKS(30000));

        // If we stop seeing these logs, the ESP32 went to sleep
        if (loop_counter % 8 == 0) {  // Every 4 minutes (8 * 30 seconds)
            ESP_LOGI(TAG, "=== 4-MINUTE MARK - Keep-alive should activate soon ===");
        }
    }

    // The main task should never exit this loop
    // If it does, something went wrong
    ESP_LOGE(TAG, "Main task exited unexpectedly!");
}