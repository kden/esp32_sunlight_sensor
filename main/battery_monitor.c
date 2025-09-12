/**
* @file battery_monitor.c
 *
 * Battery voltage monitoring functions for ESP32-C3.
 *
 * Copyright (c) 2025 Caden Howell (cadenhowell@gmail.com)
 *
 * Developed with assistance from Claude Sonnet 4 (2025).
 *
 * Apache 2.0 Licensed as described in the file LICENSE
 */

#include "battery_monitor.h"
#include "app_config.h"
#include "esp_log.h"
#include "esp_adc/adc_oneshot.h"
#include "esp_adc/adc_cali.h"
#include "esp_adc/adc_cali_scheme.h"
#include <math.h>

#define TAG "BATTERY_MONITOR"

static adc_oneshot_unit_handle_t adc1_handle = NULL;
static adc_cali_handle_t adc1_cali_handle = NULL;
static bool adc_calibrated = false;
static adc_channel_t battery_adc_channel;

/**
 * @brief Map GPIO number to ADC channel for ESP32-C3
 */
static esp_err_t gpio_to_adc_channel(int gpio_num, adc_channel_t *channel) {
    switch (gpio_num) {
        case 0: *channel = ADC_CHANNEL_0; break;
        case 1: *channel = ADC_CHANNEL_1; break;
        case 2: *channel = ADC_CHANNEL_2; break;
        case 3: *channel = ADC_CHANNEL_3; break;
        case 4: *channel = ADC_CHANNEL_4; break;
        case 5: *channel = ADC_CHANNEL_5; break;
        default:
            ESP_LOGE(TAG, "GPIO %d is not a valid ADC pin for ESP32-C3", gpio_num);
            return ESP_ERR_INVALID_ARG;
    }
    return ESP_OK;
}

esp_err_t battery_monitor_init(void) {
    esp_err_t ret = ESP_OK;

    // Map the configured GPIO to ADC channel
    ret = gpio_to_adc_channel(CONFIG_BATTERY_ADC_GPIO, &battery_adc_channel);
    if (ret != ESP_OK) {
        ESP_LOGE(TAG, "Invalid battery ADC GPIO: %d", CONFIG_BATTERY_ADC_GPIO);
        return ret;
    }

    ESP_LOGI(TAG, "Using GPIO %d (ADC channel %d) for battery monitoring",
             CONFIG_BATTERY_ADC_GPIO, battery_adc_channel);

    // Initialize ADC1
    adc_oneshot_unit_init_cfg_t init_config1 = {
        .unit_id = ADC_UNIT_1,
    };
    ret = adc_oneshot_new_unit(&init_config1, &adc1_handle);
    if (ret != ESP_OK) {
        ESP_LOGE(TAG, "Failed to initialize ADC1 unit: %s", esp_err_to_name(ret));
        return ret;
    }

    // Configure the ADC channel
    adc_oneshot_chan_cfg_t config = {
        .bitwidth = ADC_BITWIDTH_DEFAULT,
        .atten = ADC_ATTEN_DB_12,  // For 0-3.3V range
    };
    ret = adc_oneshot_config_channel(adc1_handle, battery_adc_channel, &config);
    if (ret != ESP_OK) {
        ESP_LOGE(TAG, "Failed to configure ADC channel: %s", esp_err_to_name(ret));
        return ret;
    }

    // Initialize ADC calibration
    adc_cali_curve_fitting_config_t cali_config = {
        .unit_id = ADC_UNIT_1,
        .chan = battery_adc_channel,
        .atten = ADC_ATTEN_DB_12,
        .bitwidth = ADC_BITWIDTH_DEFAULT,
    };
    ret = adc_cali_create_scheme_curve_fitting(&cali_config, &adc1_cali_handle);
    if (ret == ESP_OK) {
        adc_calibrated = true;
        ESP_LOGI(TAG, "ADC calibration scheme curve fitting initialized");
    } else {
        ESP_LOGW(TAG, "ADC calibration failed, using raw values: %s", esp_err_to_name(ret));
        adc_calibrated = false;
    }

    ESP_LOGI(TAG, "Battery monitoring initialized");
    return ESP_OK;
}

bool battery_is_present(void) {
    float voltage;
    esp_err_t ret = battery_get_voltage(&voltage);
    if (ret != ESP_OK) {
        return false;
    }

    return voltage > BATTERY_PRESENT_THRESHOLD_V;
}

esp_err_t battery_get_voltage(float *voltage) {
    if (voltage == NULL) {
        return ESP_ERR_INVALID_ARG;
    }

    if (adc1_handle == NULL) {
        ESP_LOGE(TAG, "ADC not initialized");
        return ESP_ERR_INVALID_STATE;
    }

    // Take multiple readings and average them for better accuracy
    const int num_samples = 10;
    int total_raw = 0;
    int min_raw = 4095;
    int max_raw = 0;

    for (int i = 0; i < num_samples; i++) {
        int raw_reading;
        esp_err_t ret = adc_oneshot_read(adc1_handle, battery_adc_channel, &raw_reading);
        if (ret != ESP_OK) {
            ESP_LOGE(TAG, "Failed to read ADC: %s", esp_err_to_name(ret));
            return ret;
        }
        total_raw += raw_reading;
        if (raw_reading < min_raw) min_raw = raw_reading;
        if (raw_reading > max_raw) max_raw = raw_reading;
        vTaskDelay(pdMS_TO_TICKS(10)); // Small delay between readings
    }

    int avg_raw = total_raw / num_samples;

    ESP_LOGI(TAG, "ADC readings - avg: %d, min: %d, max: %d, range: %d",
             avg_raw, min_raw, max_raw, max_raw - min_raw);

    if (adc_calibrated) {
        // Convert to voltage using calibration
        int voltage_mv;
        esp_err_t ret = adc_cali_raw_to_voltage(adc1_cali_handle, avg_raw, &voltage_mv);
        if (ret != ESP_OK) {
            ESP_LOGE(TAG, "Failed to convert raw to voltage: %s", esp_err_to_name(ret));
            return ret;
        }

        float adc_voltage = voltage_mv / 1000.0;
        *voltage = adc_voltage * BATTERY_VOLTAGE_DIVIDER_RATIO;

        ESP_LOGI(TAG, "ADC voltage: %.3fV, calculated battery voltage: %.3fV", adc_voltage, *voltage);
    } else {
        // Fallback: use raw reading with approximate conversion
        // For ESP32-C3 with 12-bit ADC and 3.3V reference
        float adc_voltage = (avg_raw / 4095.0) * 3.3;
        *voltage = adc_voltage * BATTERY_VOLTAGE_DIVIDER_RATIO;

        ESP_LOGI(TAG, "Raw ADC voltage: %.3fV, calculated battery voltage: %.3fV", adc_voltage, *voltage);
    }

    ESP_LOGI(TAG, "Final battery voltage: %.3fV (threshold: %.1fV)", *voltage, BATTERY_PRESENT_THRESHOLD_V);
    return ESP_OK;
}

esp_err_t battery_get_status_string(char *buffer, size_t buffer_size) {
    if (buffer == NULL || buffer_size == 0) {
        return ESP_ERR_INVALID_ARG;
    }

    if (!battery_is_present()) {
        snprintf(buffer, buffer_size, "no battery detected");
        return ESP_OK;
    }

    float voltage;
    esp_err_t ret = battery_get_voltage(&voltage);
    if (ret != ESP_OK) {
        snprintf(buffer, buffer_size, "battery read error");
        return ret;
    }

    // Estimate percentage (rough approximation for Li-ion)
    float percentage = 0.0;
    if (voltage >= 4.0) {
        percentage = 100.0;
    } else if (voltage >= 3.7) {
        percentage = 50.0 + (voltage - 3.7) * (50.0 / 0.3);
    } else if (voltage >= 3.3) {
        percentage = (voltage - 3.3) * (50.0 / 0.4);
    } else {
        percentage = 0.0;
    }

    const char* status;
    if (voltage <= BATTERY_CRITICAL_THRESHOLD_V) {
        status = "critical";
    } else if (voltage <= BATTERY_LOW_THRESHOLD_V) {
        status = "low";
    } else {
        status = "ok";
    }

    snprintf(buffer, buffer_size, "battery %.2fV %.0f%% %s", voltage, percentage, status);
    return ESP_OK;
}
