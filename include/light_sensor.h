/**
* @file light_sensor.h
 *
 * Functions for reading the BH1750 sensor.
 *
 * Copyright (c) 2025 Caden Howell (cadenhowell@gmail.com)
 *
 * Developed with assistance from ChatGPT 4o (2025) and Google Gemini 2.5 Pro (2025).
 *
 * Apache 2.0 Licensed as described in the file LICENSE
 */

#pragma once

#include "i2cdev.h"
#include "esp_err.h"

/**
 * @brief Initializes the BH1750 light sensor.
 *
 * @param[out] dev Pointer to a pointer that will receive the address of the sensor's device descriptor.
 * @return esp_err_t ESP_OK on success, error code on failure.
 */
esp_err_t init_light_sensor(i2c_dev_t **dev);

/**
 * @brief Reads the ambient light from the BH1750 sensor.
 *
 * @param dev Pointer to the initialized sensor's device descriptor.
 * @param[out] lux Pointer to a float where the light level in Lux will be stored.
 * @return esp_err_t ESP_OK on success, error code on failure.
 */
esp_err_t get_ambient_light(i2c_dev_t *dev, float *lux);

