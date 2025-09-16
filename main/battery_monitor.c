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
#include "wifi_monitor.h"

#define TAG "BATTERY_MONITOR"

static adc_oneshot_unit_handle_t adc1_handle = NULL;
static adc_cali_handle_t adc1_cali_handle = NULL;
static bool adc_calibrated = false;
static adc_channel_t battery_adc_channel;
static bool battery_circuit_present = false;

/**
 * @brief Map GPIO number to ADC channel for different ESP32 variants
 * @param gpio_num GPIO pin number
 * @param channel Output parameter for ADC channel
 * @return ESP_OK on success, error code on failure
 */
static esp_err_t gpio_to_adc_channel(int gpio_num, adc_channel_t *channel) {
    if (channel == NULL) {
        return ESP_ERR_INVALID_ARG;
    }

#if CONFIG_IDF_TARGET_ESP32C3
    if (gpio_num < 0 || gpio_num > 5) {
        ESP_LOGE(TAG, "GPIO %d is not a valid ADC pin for ESP32-C3 (valid: 0-5)", gpio_num);
        return ESP_ERR_INVALID_ARG;
    }
    *channel = (adc_channel_t)gpio_num;  // Direct mapping for ESP32-C3
#elif CONFIG_IDF_TARGET_ESP32S3
    if (gpio_num < 1 || gpio_num > 10) {
        ESP_LOGE(TAG, "GPIO %d is not a valid ADC1 pin for ESP32-S3 (valid: 1-10)", gpio_num);
        return ESP_ERR_INVALID_ARG;
    }
    *channel = (adc_channel_t)(gpio_num - 1);  // GPIO 1-10 map to ADC1 channels 0-9
#else
    ESP_LOGE(TAG, "Unsupported target for battery monitoring");
    return ESP_ERR_NOT_SUPPORTED;
#endif

    return ESP_OK;
}

/**
 * @brief Calculate battery percentage from voltage
 * @param voltage Battery voltage
 * @return Battery percentage (0-100)
 */
static int calculate_battery_percentage(float voltage) {
    if (voltage >= 4.0) {
        return 100;
    } else if (voltage >= 3.7) {
        return (int)(50.0 + (voltage - 3.7) * (50.0 / 0.3));
    } else if (voltage >= 3.3) {
        return (int)((voltage - 3.3) * (50.0 / 0.4));
    } else {
        return 0;
    }
}

/**
 * @brief Get battery status description from voltage
 * @param voltage Battery voltage
 * @return Status string ("ok", "low", "critical")
 */
static const char* get_battery_status(float voltage) {
    if (voltage <= BATTERY_CRITICAL_THRESHOLD_V) {
        return "critical";
    } else if (voltage <= BATTERY_LOW_THRESHOLD_V) {
        return "low";
    } else {
        return "ok";
    }
}

esp_err_t battery_monitor_init(void) {
    esp_err_t ret = ESP_OK;

    // Check if battery monitoring is disabled in configuration
    if (!CONFIG_HAS_BATTERY_CIRCUIT) {
        ESP_LOGI(TAG, "Battery monitoring disabled in configuration (no battery circuit)");
        battery_circuit_present = false;
        return ESP_OK;
    }

    // Battery circuit is present according to configuration
    battery_circuit_present = true;

    // Map the configured GPIO to ADC channel
    ret = gpio_to_adc_channel(CONFIG_BATTERY_ADC_GPIO, &battery_adc_channel);
    if (ret != ESP_OK) {
        ESP_LOGE(TAG, "Invalid battery ADC GPIO: %d", CONFIG_BATTERY_ADC_GPIO);
        battery_circuit_present = false;
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
        battery_circuit_present = false;
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
        battery_circuit_present = false;
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

    ESP_LOGI(TAG, "Battery monitoring initialized successfully");
    return ESP_OK;
}

bool battery_is_present(void) {
    if (!battery_circuit_present) {
        return false;
    }

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

    ESP_LOGD(TAG, "ADC readings - avg: %d, min: %d, max: %d, range: %d",
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

        ESP_LOGD(TAG, "ADC voltage: %.3fV, calculated battery voltage: %.3fV", adc_voltage, *voltage);
    } else {
        // Fallback: use raw reading with approximate conversion
        // For both ESP32-C3 and ESP32-S3 with 12-bit ADC and 3.3V reference
        float adc_voltage = (avg_raw / 4095.0) * 3.3;
        *voltage = adc_voltage * BATTERY_VOLTAGE_DIVIDER_RATIO;

        ESP_LOGD(TAG, "Raw ADC voltage: %.3fV, calculated battery voltage: %.3fV", adc_voltage, *voltage);
    }

    return ESP_OK;
}

esp_err_t battery_get_status_string(char *buffer, size_t buffer_size) {
    if (buffer == NULL || buffer_size == 0) {
        return ESP_ERR_INVALID_ARG;
    }

    if (!battery_circuit_present) {
        snprintf(buffer, buffer_size, "no battery circuit");
        return ESP_OK;
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

    int percentage = calculate_battery_percentage(voltage);
    const char* status = get_battery_status(voltage);

    snprintf(buffer, buffer_size, "battery %.2fV %d%% %s", voltage, percentage, status);
    return ESP_OK;
}

esp_err_t get_device_status_string(char *buffer, size_t buffer_size) {
    if (buffer == NULL || buffer_size == 0) {
        return ESP_ERR_INVALID_ARG;
    }

    char battery_status[64];
    char wifi_status[64];

    // Get battery status
    esp_err_t battery_err = battery_get_status_string(battery_status, sizeof(battery_status));

    // Get WiFi status
    esp_err_t wifi_err = wifi_get_status_string(wifi_status, sizeof(wifi_status));

    // Combine both statuses
    if (battery_err == ESP_OK && wifi_err == ESP_OK) {
        snprintf(buffer, buffer_size, "%s | %s", battery_status, wifi_status);
    } else if (battery_err == ESP_OK) {
        snprintf(buffer, buffer_size, "%s | wifi error", battery_status);
    } else if (wifi_err == ESP_OK) {
        snprintf(buffer, buffer_size, "battery error | %s", wifi_status);
    } else {
        snprintf(buffer, buffer_size, "battery error | wifi error");
    }

    return ESP_OK;
}

esp_err_t battery_get_api_data(float *voltage, int *percentage) {
    if (voltage == NULL || percentage == NULL) {
        return ESP_ERR_INVALID_ARG;
    }

    if (!battery_circuit_present || !battery_is_present()) {
        return ESP_ERR_NOT_FOUND;
    }

    esp_err_t ret = battery_get_voltage(voltage);
    if (ret != ESP_OK) {
        return ret;
    }

    *percentage = calculate_battery_percentage(*voltage);
    return ESP_OK;
}