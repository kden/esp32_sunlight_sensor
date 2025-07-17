/**
* @file main.c
 *
 * ESP-IDF application to read and display ambient light levels using the BH1750 sensor.
 *
 * Copyright (c) 2025 Caden Howell (cadenhowell@gmail.com)
 *
 * Developed with assistance from ChatGPT 4o (2025) and Google Gemini 2.5 Pro (2025).
 *
 * Apache 2.0 Licensed as described in the file LICENSE
 */

#include <stdio.h>
#include <esp_log.h>
#include <string.h>

#include "freertos/FreeRTOS.h"
#include "freertos/task.h"
#include "esp_netif.h"
#include "esp_wifi.h"
#include "app_config.h"
#include "sdkconfig.h"

#include "bh1750.h"
#include "ssd1306.h"
#include "oled.h"
#include "nvs_flash.h"
#include "wifi.h"
#include "http.h"
#include "ntp.h"
#include "light_sensor.h"
#include "sensor_data.h"
#include "esp_sleep.h"

#define TAG "MAIN"

#define MAX_WIFI_NETWORKS 5
#define MAX_SSID_LEN 32
#define MAX_PASS_LEN 64

// Define the sleep interval in seconds for clarity
#define DEEP_SLEEP_INTERVAL_S 15

void app_main(void)
{
    ESP_LOGI(TAG, "Waking up from sleep...");

    // Initialize I2C
    i2c_master_bus_handle_t i2c0_bus_hdl;
    const i2c_master_bus_config_t i2c0_bus_cfg = I2C0_MASTER_CONFIG_DEFAULT;
    ESP_ERROR_CHECK(i2c_new_master_bus(&i2c0_bus_cfg, &i2c0_bus_hdl));

    // Initialize the light sensor for a single reading
    bh1750_handle_t light_sensor_hdl = NULL;
    ESP_ERROR_CHECK(init_light_sensor(i2c0_bus_hdl, &light_sensor_hdl));

    // --- Perform one reading ---
    float lux = 0;
    get_ambient_light(light_sensor_hdl, &lux);
    ESP_LOGI(TAG, "Ambient light: %.2f lux", lux);

    // --- Enter Deep Sleep ---
    // Note: We don't de-initialize peripherals. The deep sleep cycle resets them automatically.
    ESP_LOGI(TAG, "Entering deep sleep for %d seconds.", DEEP_SLEEP_INTERVAL_S);

    // Configure the timer wakeup
    esp_sleep_enable_timer_wakeup(DEEP_SLEEP_INTERVAL_S * 1000000ULL);
    
    fflush(stdout); // Ensure all log messages are printed before sleep
    esp_deep_sleep_start();
}
