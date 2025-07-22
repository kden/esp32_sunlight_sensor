/**
* @file crash_handler.c
 *
 * Reports the device's reset reason on boot.
 * This implementation is based on checking esp_reset_reason() to detect
 * and report crashes without a custom panic handler.
 *
 * Copyright (c) 2025 Caden Howell (cadenhowell@gmail.com)
 *
 * Developed with assistance from ChatGPT 4o (2025) and Google Gemini 2.5 Pro (2025).
 *
 * Apache 2.0 Licensed as described in the file LICENSE
 */

#include "crash_handler.h"
#include "esp_log.h"
#include "esp_system.h"
#include "http.h"
#include "wifi.h"
#include "sdkconfig.h"
#include "freertos/FreeRTOS.h"
#include "freertos/task.h"
#include "esp_wifi.h"
#include <string.h> // Required for strcmp

#define TAG "CRASH_REPORTER"

/**
 * @brief Converts the esp_reset_reason_t enum to a human-readable string.
 *
 * @param reason The reset reason enum from the ESP-IDF.
 * @return A constant string describing the reason.
 */
static const char* reset_reason_to_str(esp_reset_reason_t reason) {
    switch (reason) {
        case ESP_RST_UNKNOWN:    return "Unknown";
        case ESP_RST_POWERON:    return "Power-on";
        case ESP_RST_EXT:        return "External pin";
        case ESP_RST_SW:         return "Software restart";
        case ESP_RST_PANIC:      return "Panic/Exception";
        case ESP_RST_INT_WDT:    return "Interrupt watchdog";
        case ESP_RST_TASK_WDT:   return "Task watchdog";
        case ESP_RST_WDT:        return "Other watchdog";
        case ESP_RST_DEEPSLEEP:  return "Exiting deep sleep";
        case ESP_RST_BROWNOUT:   return "Brownout";
        case ESP_RST_SDIO:       return "SDIO";
        case ESP_RST_USB:        return "USB";
        case ESP_RST_JTAG:       return "JTAG";
        case ESP_RST_EFUSE:      return "eFuse";
        case ESP_RST_PWR_GLITCH: return "Power glitch";
        case ESP_RST_CPU_LOCKUP: return "CPU Lock-up";
        default:                 return "Undefined";
    }
}

void check_and_report_crash(void) {
    esp_reset_reason_t reason = esp_reset_reason();
    ESP_LOGI(TAG, "Device reset reason: %s (%d)", reset_reason_to_str(reason), reason);

    // We only want to report on unexpected, crash-related resets.
    // Normal events like power-on or software restart are ignored.
    if (reason == ESP_RST_PANIC || reason == ESP_RST_TASK_WDT || reason == ESP_RST_INT_WDT || reason == ESP_RST_CPU_LOCKUP) {
        ESP_LOGW(TAG, "Device rebooted from a crash. Attempting to report...");

        char status_message[128];
        snprintf(status_message, sizeof(status_message), "CRASH detected. Reset reason: %s", reset_reason_to_str(reason));

        // Attempt to connect to Wi-Fi to send the report.
        // This block is self-contained and manages its own connection lifecycle.
        wifi_manager_init();
        int retries = 15;
        while (!wifi_is_connected() && retries-- > 0) {
            vTaskDelay(pdMS_TO_TICKS(2000));
        }

        if (wifi_is_connected()) {
            ESP_LOGI(TAG, "Wi-Fi connected. Sending crash report...");
            send_status_update(status_message, CONFIG_SENSOR_ID, CONFIG_BEARER_TOKEN);
            ESP_LOGI(TAG, "Crash report sent successfully.");

            // Conditionally disconnect based on power drain setting
            if (strcmp(CONFIG_SENSOR_POWER_DRAIN, "high") != 0) {
                // Disconnect Wi-Fi to allow the main application logic to manage power.
                esp_wifi_disconnect();
                esp_wifi_stop();
                ESP_LOGI(TAG, "Wi-Fi disconnected after sending report (power_drain: %s).", CONFIG_SENSOR_POWER_DRAIN);
            } else {
                ESP_LOGI(TAG, "Keeping Wi-Fi connected after sending report (power_drain: high).");
            }
        } else {
            ESP_LOGE(TAG, "Failed to connect to Wi-Fi to send crash report.");
        }
    } else {
        ESP_LOGI(TAG, "Normal boot reason. No crash report needed.");
    }
}