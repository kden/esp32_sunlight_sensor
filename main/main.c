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

#define SDA_GPIO 21
#define SCL_GPIO 22
#define TAG "MAIN"

#define MAX_WIFI_NETWORKS 5
#define MAX_SSID_LEN 32
#define MAX_PASS_LEN 64

void app_main(void)
{
    // Initialize I2C
    i2c_master_bus_handle_t i2c0_bus_hdl;
    const i2c_master_bus_config_t i2c0_bus_cfg = I2C0_MASTER_CONFIG_DEFAULT;
    ESP_ERROR_CHECK(i2c_new_master_bus(&i2c0_bus_cfg, &i2c0_bus_hdl));

    // Initialize the light sensor
    bh1750_handle_t light_sensor_hdl = NULL;
    ESP_ERROR_CHECK(init_light_sensor(i2c0_bus_hdl, &light_sensor_hdl));

    // Initialize the OLED display
    ssd1306_handle_t oled_hdl = NULL;
    ESP_ERROR_CHECK(oled_init(i2c0_bus_hdl, &oled_hdl));

    // Initialize non-volatile storage for WiFi credentials
    esp_err_t err = nvs_flash_init();
    if (err == ESP_ERR_NVS_NO_FREE_PAGES || err == ESP_ERR_NVS_NEW_VERSION_FOUND) {
        ESP_ERROR_CHECK(nvs_flash_erase());
        err = nvs_flash_init();
    }
    ESP_ERROR_CHECK(err);

    // Initialize and start the Wi-Fi manager
    wifi_manager_init();

    // Get and print the MAC address
    char mac_address_str[18];
    wifi_get_mac_address(mac_address_str);

    char lux_str[32];
    time_t last_post_time = 0;
    bool sntp_initialized = false;

    // Main loop: read and print light levels
    while (1)
    {
        if (!wifi_is_connected()) {
            ESP_LOGW(TAG, "Wi-Fi is disconnected. Waiting for automatic reconnect...");
            vTaskDelay(5000 / portTICK_PERIOD_MS);
            continue;
        }

        // Initialize SNTP only once after a valid Wi-Fi connection is made.
        if (!sntp_initialized) {
            initialize_sntp();
            sntp_initialized = true;
        }

        ESP_LOGI(APP_TAG, "Sensor ID (%s)", CONFIG_SENSOR_ID);
        //ESP_LOGI(APP_TAG, "Bearer Token (%s)", CONFIG_BEARER_TOKEN);
        //ESP_LOGI(APP_TAG, "Wifi Credentials (%s)", CONFIG_WIFI_CREDENTIALS);
        //ESP_LOGI(TAG, "Wi-Fi MAC Address: %s", mac_address_str);

        float lux = 0;
        get_ambient_light(light_sensor_hdl, &lux);

        // Update OLED monitor
        display_info(oled_hdl, lux_str, lux);

        // Check if it's time to send data
        time_t now;
        time(&now);
        if (now - last_post_time >= 5) {
            ESP_LOGI(TAG, "One minute has passed. Sending data to web service.");
            send_sensor_data(lux, CONFIG_SENSOR_ID, CONFIG_BEARER_TOKEN);
            last_post_time = now;
        }

        vTaskDelay(1000 / portTICK_PERIOD_MS);
    }
}



