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
#include "bh1750.h"
#include "nvs_flash.h"
#include "http.h"
#include "task_send_data.h"
#include "task_get_sensor_data.h"

#include "light_sensor.h"
#include "sensor_data.h"
#include "app_context.h"
#include "crash_handler.h"
#include "task_keepalive_blink.h"

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

    // Check if the last reset was due to a crash and report it.
    check_and_report_crash();

    // Initialize the keep-alive blink task only if a valid GPIO is configured.
    if (CONFIG_KEEPALIVE_LED_GPIO >= 0) {
        init_keepalive_blink_task();
    }

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

    if (app_context->buffer_mutex == NULL) {
        ESP_LOGE(TAG, "Failed to create mutex!");
        free(app_context);
        return;
    }

    // Create and launch the tasks
    // Run send data task first to set the local clock correctly
    // Increased stack size from 4096 to 8192
    xTaskCreate(task_send_data, "send_data_task", 8192, app_context, 5, NULL);
    // Give the send_data_task time to connect and perform the initial NTP sync
    vTaskDelay(pdMS_TO_TICKS(10000));
    // Increased stack size from 4096 to 8192
    xTaskCreate(task_get_sensor_data, "sensor_task", 8192, app_context, 5, NULL);

    ESP_LOGI(TAG, "Initialization complete. Tasks are running.");
    // The main task has nothing else to do, so it can be deleted.
    // vTaskDelete(NULL); // Or just let it exit.
}