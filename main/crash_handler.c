/**
* @file crash_handler.c
 *
 * Reports the device's reset reason on boot and sends logs for debugging.
 * Now handles both crashes and manual resets consistently.
 *
 * Copyright (c) 2025 Caden Howell (cadenhowell@gmail.com)
 *
 * Developed with assistance from ChatGPT 4o (2025) and Google Gemini 2.5 Pro (2025).
 *
 * Apache 2.0 Licensed as described in the file LICENSE
 */

#include "crash_handler.h"
#include "app_config.h"
#include "esp_log.h"
#include "esp_system.h"
#include "api_client.h"
#include "wifi_manager.h"
#include "log_capture.h"
#include "sdkconfig.h"
#include "freertos/FreeRTOS.h"
#include "freertos/task.h"
#include "esp_wifi.h"
#include <string.h>
#include <stdlib.h>

#define TAG "CRASH_REPORTER"

/**
 * @brief Converts the esp_reset_reason_t enum to a human-readable string.
 */
static const char* reset_reason_to_str(esp_reset_reason_t reason)
{
    switch (reason)
    {
    case ESP_RST_UNKNOWN: return "Unknown";
    case ESP_RST_POWERON: return "Power-on";
    case ESP_RST_EXT: return "External pin";
    case ESP_RST_SW: return "Software restart";
    case ESP_RST_PANIC: return "Panic/Exception";
    case ESP_RST_INT_WDT: return "Interrupt watchdog";
    case ESP_RST_TASK_WDT: return "Task watchdog";
    case ESP_RST_WDT: return "Other watchdog";
    case ESP_RST_DEEPSLEEP: return "Exiting deep sleep";
    case ESP_RST_BROWNOUT: return "Brownout";
    case ESP_RST_SDIO: return "SDIO";
    case ESP_RST_USB: return "USB";
    case ESP_RST_JTAG: return "JTAG";
    case ESP_RST_EFUSE: return "eFuse";
    case ESP_RST_PWR_GLITCH: return "Power glitch";
    case ESP_RST_CPU_LOCKUP: return "CPU Lock-up";
    default: return "Undefined";
    }
}

/**
 * @brief Check if this reset type should trigger a log report
 */
static bool should_send_log_report(esp_reset_reason_t reason)
{
    switch (reason) {
        // Crash-related resets (always send)
        case ESP_RST_PANIC:
        case ESP_RST_TASK_WDT:
        case ESP_RST_INT_WDT:
        case ESP_RST_CPU_LOCKUP:
        case ESP_RST_BROWNOUT:
        case ESP_RST_PWR_GLITCH:
            return true;

        // Manual reset (send for debugging) - INCLUDES USB RESET
        case ESP_RST_EXT:      // External pin reset
        case ESP_RST_USB:      // USB reset (when you press reset button)
            return true;

        // Normal resets (don't send to avoid spam)
        case ESP_RST_POWERON:
        case ESP_RST_SW:
        case ESP_RST_DEEPSLEEP:
        default:
            return false;
    }
}

/**
 * @brief Get appropriate message prefix for reset type
 */
static const char* get_reset_prefix(esp_reset_reason_t reason)
{
    switch (reason) {
        case ESP_RST_PANIC:
        case ESP_RST_TASK_WDT:
        case ESP_RST_INT_WDT:
        case ESP_RST_CPU_LOCKUP:
        case ESP_RST_BROWNOUT:
        case ESP_RST_PWR_GLITCH:
            return "CRASH";

        case ESP_RST_EXT:
        case ESP_RST_USB:      // USB RESET ALSO GETS MANUAL RESET PREFIX
            return "MANUAL RESET";

        default:
            return "RESET";
    }
}

void check_and_report_crash(void)
{
    esp_reset_reason_t reason = esp_reset_reason();
    ESP_LOGI(TAG, "Device reset reason: %s (%d)", reset_reason_to_str(reason), reason);

    // Check if we should send a log report for this reset type
    if (!should_send_log_report(reason)) {
        ESP_LOGI(TAG, "Normal boot reason. No log report needed.");
        return;
    }

    // Get the message prefix based on reset type
    const char* prefix = get_reset_prefix(reason);

    ESP_LOGW(TAG, "Device rebooted from: %s. Attempting to send log report...",
             reset_reason_to_str(reason));

    // Allocate larger buffers for comprehensive log capture
    char *log_dump = malloc(8192);      // 8KB for logs (was 1536)
    char *status_message = malloc(10240); // 10KB for final message (was 2048)

    if (!log_dump || !status_message) {
        ESP_LOGE(TAG, "Failed to allocate memory for crash report buffers");
        if (log_dump) free(log_dump);
        if (status_message) free(status_message);
        return;
    }

    ESP_LOGI(TAG, "Allocated buffers: log_dump=%zu bytes, status_message=%zu bytes",
             log_dump ? 8192 : 0, status_message ? 10240 : 0);

    // Get captured logs from previous session
    if (log_capture_dump(log_dump, 8192) == ESP_OK && strlen(log_dump) > 0) {
        ESP_LOGI(TAG, "Retrieved %zu bytes of log data from previous session", strlen(log_dump));
        snprintf(status_message, 10240,
                 "%s: %s\nPrevious session logs:\n%s",
                 prefix, reset_reason_to_str(reason), log_dump);
    } else {
        ESP_LOGW(TAG, "No logs available from previous session");
        snprintf(status_message, 10240,
                 "%s: %s (no logs available from previous session)",
                 prefix, reset_reason_to_str(reason));
    }

    ESP_LOGI(TAG, "Crash report message length: %zu bytes", strlen(status_message));

    // Clear the old logs since we're about to send them
    log_capture_clear();

    // Attempt to connect to Wi-Fi to send the report
    ESP_LOGI(TAG, "Initializing WiFi for crash report transmission");
    wifi_manager_init();
    int retries = 30;
    while (!wifi_is_connected() && retries-- > 0) {
        ESP_LOGI(TAG, "WiFi connection attempt %d/15", 15 - retries);
        vTaskDelay(pdMS_TO_TICKS(3000));
    }

    if (wifi_is_connected()) {
        ESP_LOGI(TAG, "Wi-Fi connected. Sending %s log report...", prefix);
        esp_err_t send_result = api_send_status_update(status_message);

        if (send_result == ESP_OK) {
            ESP_LOGI(TAG, "%s log report sent successfully.", prefix);
        } else {
            ESP_LOGE(TAG, "Failed to send %s log report: %s", prefix, esp_err_to_name(send_result));
        }

        // Disconnect Wi-Fi to allow the main application logic to manage power
        esp_wifi_disconnect();
        esp_wifi_stop();
        ESP_LOGI(TAG, "Wi-Fi disconnected after sending report.");
    } else {
        ESP_LOGE(TAG, "Failed to connect to Wi-Fi to send %s log report.", prefix);
    }

    // Free allocated memory
    free(log_dump);
    free(status_message);
    ESP_LOGI(TAG, "Crash handler cleanup completed");
}