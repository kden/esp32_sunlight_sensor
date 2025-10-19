/**
* @file crash_handler.c
 *
 * Reports the device's reset reason on boot.
 * Simple version - just logs, doesn't send anything.
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
#include "http.h"
#include "sdkconfig.h"
#include "freertos/FreeRTOS.h"


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

void check_and_report_crash(void)
{
    esp_reset_reason_t reason = esp_reset_reason();
    ESP_LOGI(TAG, "Device reset reason: %s (%d)", reset_reason_to_str(reason), reason);

    // Only report actual crashes, not normal resets
    if (reason == ESP_RST_PANIC || reason == ESP_RST_TASK_WDT ||
        reason == ESP_RST_INT_WDT || reason == ESP_RST_CPU_LOCKUP)
    {
        ESP_LOGW(TAG, "Device rebooted from a crash. Reset reason: %s",
                 reset_reason_to_str(reason));

        // Don't try to send anything during early boot
        // Just log it - we can see it in serial monitor
    }
    else
    {
        ESP_LOGI(TAG, "Normal boot reason. No crash report needed.");
    }
}