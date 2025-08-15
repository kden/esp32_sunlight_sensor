/**
 * @file task_keepalive_transistor.c
 *
 * Smart power draw control using transistor-switched resistor for USB power bank keep-alive.
 *
 * Copyright (c) 2025 Caden Howell (cadenhowell@gmail.com)
 *
 * Developed with assistance from ChatGPT 4o (2025), Google Gemini 2.5 Pro (2025) and Claude Sonnet 4 (2025).
 *
 * Apache 2.0 Licensed as described in the file LICENSE
 */

#include "task_keepalive_transistor.h"
#include "esp_log.h"
#include "esp_err.h"
#include "driver/gpio.h"
#include "freertos/FreeRTOS.h"
#include "freertos/task.h"
#include "freertos/semphr.h"
#include <time.h>
#include <sys/time.h>

#define TAG "SMART_KEEPALIVE"
#define TASK_STACK_SIZE 4096
#define TASK_PRIORITY 3

// Global state structure
static struct {
    keepalive_config_t config;
    TaskHandle_t task_handle;
    SemaphoreHandle_t config_mutex;
    uint32_t total_activations;
    time_t last_activation_time;
    bool initialized;
} g_keepalive_state = {0};

/**
 * @brief Configure GPIO pin for transistor control
 */
static esp_err_t configure_control_gpio(uint8_t gpio_pin) {
    gpio_config_t gpio_conf = {
        .pin_bit_mask = (1ULL << gpio_pin),
        .mode = GPIO_MODE_OUTPUT,
        .pull_up_en = GPIO_PULLUP_DISABLE,
        .pull_down_en = GPIO_PULLDOWN_ENABLE, // Pull down to ensure transistor is off by default
        .intr_type = GPIO_INTR_DISABLE
    };

    esp_err_t ret = gpio_config(&gpio_conf);
    if (ret != ESP_OK) {
        ESP_LOGE(TAG, "Failed to configure GPIO %d: %s", gpio_pin, esp_err_to_name(ret));
        return ret;
    }

    // Start with transistor OFF (GPIO LOW)
    ret = gpio_set_level(gpio_pin, 0);
    if (ret != ESP_OK) {
        ESP_LOGE(TAG, "Failed to set initial GPIO level: %s", esp_err_to_name(ret));
        return ret;
    }

    ESP_LOGI(TAG, "Configured GPIO %d for transistor control (initial state: OFF)", gpio_pin);
    return ESP_OK;
}

/**
 * @brief Activate the keep-alive resistor for the specified duration
 */
static void activate_keepalive_resistor(uint32_t duration_seconds, uint8_t gpio_pin) {
    time_t activation_start;
    time(&activation_start);

    ESP_LOGI(TAG, "Activating keep-alive resistor for %lu seconds", (unsigned long)duration_seconds);

    // Turn ON transistor (GPIO HIGH)
    esp_err_t ret = gpio_set_level(gpio_pin, 1);
    if (ret != ESP_OK) {
        ESP_LOGE(TAG, "Failed to turn ON transistor: %s", esp_err_to_name(ret));
        return;
    }

    // Wait for the specified duration
    vTaskDelay(pdMS_TO_TICKS(duration_seconds * 1000));

    // Turn OFF transistor (GPIO LOW)
    ret = gpio_set_level(gpio_pin, 0);
    if (ret != ESP_OK) {
        ESP_LOGE(TAG, "Failed to turn OFF transistor: %s", esp_err_to_name(ret));
    }

    // Update statistics
    g_keepalive_state.total_activations++;
    g_keepalive_state.last_activation_time = activation_start;

    ESP_LOGI(TAG, "Keep-alive resistor deactivated. Total activations: %lu",
             (unsigned long)g_keepalive_state.total_activations);
}

/**
 * @brief Main keep-alive task function
 */
static void keepalive_task(void *pvParameters) {
    ESP_LOGI(TAG, "Smart keep-alive task started");

    // Initial configuration
    keepalive_config_t current_config;
    if (xSemaphoreTake(g_keepalive_state.config_mutex, portMAX_DELAY) == pdTRUE) {
        current_config = g_keepalive_state.config;
        xSemaphoreGive(g_keepalive_state.config_mutex);
    } else {
        ESP_LOGE(TAG, "Failed to acquire config mutex");
        vTaskDelete(NULL);
        return;
    }

    ESP_LOGI(TAG, "Keep-alive configuration: %lu seconds ON every %lu minutes on GPIO %d",
             (unsigned long)current_config.on_duration_seconds,
             (unsigned long)current_config.interval_minutes,
             current_config.control_gpio);

    while (1) {
        // Get current configuration (in case it was updated)
        if (xSemaphoreTake(g_keepalive_state.config_mutex, pdMS_TO_TICKS(100)) == pdTRUE) {
            current_config = g_keepalive_state.config;
            xSemaphoreGive(g_keepalive_state.config_mutex);
        }

        // Activate the keep-alive resistor
        activate_keepalive_resistor(current_config.on_duration_seconds, current_config.control_gpio);

        // Calculate sleep time (interval minus the ON duration)
        uint32_t interval_seconds = current_config.interval_minutes * 60;
        uint32_t sleep_seconds = interval_seconds - current_config.on_duration_seconds;

        if (sleep_seconds > 0) {
            ESP_LOGI(TAG, "Sleeping for %lu seconds until next activation", (unsigned long)sleep_seconds);
            vTaskDelay(pdMS_TO_TICKS(sleep_seconds * 1000));
        } else {
            ESP_LOGW(TAG, "ON duration (%lu s) is >= interval (%lu s). Using minimum 1 second sleep.",
                     (unsigned long)current_config.on_duration_seconds, (unsigned long)interval_seconds);
            vTaskDelay(pdMS_TO_TICKS(1000));
        }
    }
}

esp_err_t init_smart_keepalive_task(const keepalive_config_t *config) {
    if (g_keepalive_state.initialized) {
        ESP_LOGW(TAG, "Keep-alive task already initialized");
        return ESP_OK;
    }

    // Use provided config or defaults
    if (config != NULL) {
        g_keepalive_state.config = *config;
    } else {
        keepalive_config_t default_config = KEEPALIVE_CONFIG_DEFAULT();
        g_keepalive_state.config = default_config;
    }

    // Validate configuration
    if (g_keepalive_state.config.on_duration_seconds == 0) {
        ESP_LOGE(TAG, "Invalid on_duration_seconds: must be > 0");
        return ESP_ERR_INVALID_ARG;
    }
    if (g_keepalive_state.config.interval_minutes == 0) {
        ESP_LOGE(TAG, "Invalid interval_minutes: must be > 0");
        return ESP_ERR_INVALID_ARG;
    }

    // Create mutex for configuration updates
    g_keepalive_state.config_mutex = xSemaphoreCreateMutex();
    if (g_keepalive_state.config_mutex == NULL) {
        ESP_LOGE(TAG, "Failed to create config mutex");
        return ESP_ERR_NO_MEM;
    }

    // Configure the control GPIO
    esp_err_t ret = configure_control_gpio(g_keepalive_state.config.control_gpio);
    if (ret != ESP_OK) {
        vSemaphoreDelete(g_keepalive_state.config_mutex);
        return ret;
    }

    // Create the keep-alive task
    BaseType_t task_ret = xTaskCreate(
        keepalive_task,
        "smart_keepalive",
        TASK_STACK_SIZE,
        NULL,
        TASK_PRIORITY,
        &g_keepalive_state.task_handle
    );

    if (task_ret != pdPASS) {
        ESP_LOGE(TAG, "Failed to create keep-alive task");
        vSemaphoreDelete(g_keepalive_state.config_mutex);
        return ESP_FAIL;
    }

    g_keepalive_state.initialized = true;
    g_keepalive_state.total_activations = 0;
    g_keepalive_state.last_activation_time = 0;

    ESP_LOGI(TAG, "Smart keep-alive system initialized successfully");
    return ESP_OK;
}

esp_err_t update_keepalive_timing(uint32_t on_duration_seconds, uint32_t interval_minutes) {
    if (!g_keepalive_state.initialized) {
        ESP_LOGE(TAG, "Keep-alive system not initialized");
        return ESP_ERR_INVALID_STATE;
    }

    if (on_duration_seconds == 0 || interval_minutes == 0) {
        ESP_LOGE(TAG, "Invalid timing parameters: on_duration=%lu, interval=%lu",
                 (unsigned long)on_duration_seconds, (unsigned long)interval_minutes);
        return ESP_ERR_INVALID_ARG;
    }

    if (xSemaphoreTake(g_keepalive_state.config_mutex, pdMS_TO_TICKS(1000)) == pdTRUE) {
        g_keepalive_state.config.on_duration_seconds = on_duration_seconds;
        g_keepalive_state.config.interval_minutes = interval_minutes;
        xSemaphoreGive(g_keepalive_state.config_mutex);

        ESP_LOGI(TAG, "Updated keep-alive timing: %lu seconds ON every %lu minutes",
                 (unsigned long)on_duration_seconds, (unsigned long)interval_minutes);
        return ESP_OK;
    } else {
        ESP_LOGE(TAG, "Failed to acquire config mutex for timing update");
        return ESP_ERR_TIMEOUT;
    }
}

esp_err_t get_keepalive_stats(uint32_t *total_activations, time_t *last_activation_time) {
    if (!g_keepalive_state.initialized) {
        return ESP_ERR_INVALID_STATE;
    }

    if (total_activations != NULL) {
        *total_activations = g_keepalive_state.total_activations;
    }

    if (last_activation_time != NULL) {
        *last_activation_time = g_keepalive_state.last_activation_time;
    }

    return ESP_OK;
}