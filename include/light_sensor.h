/**
 * @file light_sensor.h
 *
 * Functions for reading the BH1750 sensor.
 *
 * Copyright (c) 2025 Caden Howell (cadenhowell@gmail.com)
 *
 * Developed with assistance from ChatGPT and Google Gemini.
 *
 * Apache 2.0 Licensed as described in the file LICENSE
 */

#pragma once

esp_err_t init_light_sensor(i2c_master_bus_handle_t master_handle, bh1750_handle_t *handle);
esp_err_t get_ambient_light(bh1750_handle_t handle, float *lux);


