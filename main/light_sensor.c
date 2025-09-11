/**
 * @file light_sensor.c
 *
 * Functions for reading the BH1750 sensor, adapted for the i2cdev driver.
 *
 * Copyright (c) 2025 Caden Howell (cadenhowell@gmail.com)
 *
 * Developed with assistance from ChatGPT 4o (2025) and Google Gemini 2.5 Pro (2025).
 *
 * Apache 2.0 Licensed as described in the file LICENSE
 */

#include <esp_log.h>
#include "app_config.h"
#include "bh1750.h"
#include "light_sensor.h"
#include "i2cdev.h"
#include <esp_check.h> // Include for ESP_RETURN_ON_ERROR and other check macros

#define TAG "LIGHT_SENSOR"

// Define the sensor descriptor as a static variable in this file
static i2c_dev_t light_sensor_dev;

esp_err_t init_light_sensor(i2c_dev_t **dev)
{
    // Initialize the I2Cdev library. This should be called once per application.
    // It's safe to call multiple times.
    ESP_RETURN_ON_ERROR(i2cdev_init(), TAG, "i2cdev_init failed");

    // Initialize the sensor descriptor
    ESP_RETURN_ON_ERROR(
        bh1750_init_desc(&light_sensor_dev, BH1750_ADDR_LO, I2C0_MASTER_PORT, CONFIG_SENSOR_SDA_GPIO, CONFIG_SENSOR_SCL_GPIO),
        TAG,
        "bh1750_init_desc failed"
    );

    // Setup the sensor with continuous high-resolution mode
    ESP_RETURN_ON_ERROR(
        bh1750_setup(&light_sensor_dev, BH1750_MODE_CONTINUOUS, BH1750_RES_HIGH),
        TAG,
        "bh1750_setup failed"
    );

    // Pass the address of the static device descriptor back to the caller
    *dev = &light_sensor_dev;

    ESP_LOGI(TAG, "BH1750 light sensor initialized successfully");

    return ESP_OK;
}

esp_err_t get_ambient_light(i2c_dev_t *dev, float *lux)
{
    if (dev == NULL || lux == NULL) {
        return ESP_ERR_INVALID_ARG;
    }

    uint16_t raw_lux = 0;
    // The new driver reads an unsigned 16-bit integer
    esp_err_t result = bh1750_read(dev, &raw_lux);

    if (result != ESP_OK)
    {
        ESP_LOGE(TAG, "bh1750 device read failed (%s)", esp_err_to_name(result));
    }
    else
    {
        // The driver now returns the value in Lux directly
        *lux = (float)raw_lux;
        ESP_LOGI(TAG, "Ambient light: %.2f lux", *lux);
    }
    return result;
}

