/**
 * @file light_sensor.h
 *
 * Functions for reading the BH1750 sensor.
 *
 * Copyright (c) 2025 Caden Howell (cadenhowell@gmail.com)
 *
 * Developed with assistance from ChatGPT 4o (2025) and Google Gemini 2.5 Pro (2025).
 *
 * For drivers and example code
 * Credit to Eric Gionet<gionet.c.eric@gmail.com>
 * https://github.com/K0I05/ESP32-S3_ESP-IDF_COMPONENTS/tree/main/components/peripherals/i2c/esp_bh1750
 *
 * Apache 2.0 Licensed as described in the file LICENSE
 */

#include <esp_log.h>
#include "app_config.h"
#include "bh1750.h"
#include "light_sensor.h"

#define TAG "LIGHT_SENSOR"
esp_err_t init_light_sensor(i2c_master_bus_handle_t master_handle, bh1750_handle_t *handle)
{
    bh1750_config_t dev_cfg = I2C_BH1750_CONFIG_DEFAULT;
    esp_err_t err = bh1750_init(master_handle, &dev_cfg, handle);
    if (err != ESP_OK || *handle == NULL)
    {
        ESP_LOGE(TAG, "bh1750 handle init failed");
        return err;
    }
    return ESP_OK;
}

esp_err_t get_ambient_light(bh1750_handle_t handle, float *lux)
{
    esp_err_t result = bh1750_get_ambient_light(handle, lux);
    if (result != ESP_OK)
    {
        ESP_LOGE(TAG, "bh1750 device read failed (%s)", esp_err_to_name(result));
    }
    else
    {
        ESP_LOGI(TAG, "ambient light:     %.2f lux", *lux);
    }
    return result;
}