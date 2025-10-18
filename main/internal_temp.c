/**
* @file internal_temp.c
 *
 * ESP32 internal temperature sensor interface.
 *
 * Copyright (c) 2025 Caden Howell (cadenhowell@gmail.com)
 *
 * Developed with assistance from Claude Sonnet 4 (2025).
 *
 * Apache 2.0 Licensed as described in the file LICENSE
 */

#include "internal_temp.h"
#include "driver/temperature_sensor.h"
#include "esp_log.h"

#define TAG "INTERNAL_TEMP"

static temperature_sensor_handle_t temp_sensor = NULL;
static bool temp_initialized = false;

esp_err_t internal_temp_init(void) {
    if (temp_initialized) {
        return ESP_OK;
    }

    temperature_sensor_config_t temp_sensor_config = TEMPERATURE_SENSOR_CONFIG_DEFAULT(10, 80);
    esp_err_t err = temperature_sensor_install(&temp_sensor_config, &temp_sensor);

    if (err != ESP_OK) {
        ESP_LOGE(TAG, "Failed to install temperature sensor: %s", esp_err_to_name(err));
        return err;
    }

    err = temperature_sensor_enable(temp_sensor);
    if (err != ESP_OK) {
        ESP_LOGE(TAG, "Failed to enable temperature sensor: %s", esp_err_to_name(err));
        temperature_sensor_uninstall(temp_sensor);
        return err;
    }

    temp_initialized = true;
    ESP_LOGI(TAG, "Internal temperature sensor initialized");
    return ESP_OK;
}

esp_err_t internal_temp_read(float *temp_celsius) {
    if (!temp_initialized || !temp_celsius) {
        return ESP_ERR_INVALID_STATE;
    }

    esp_err_t err = temperature_sensor_get_celsius(temp_sensor, temp_celsius);
    if (err != ESP_OK) {
        ESP_LOGE(TAG, "Failed to read temperature: %s", esp_err_to_name(err));
        return err;
    }

    return ESP_OK;
}