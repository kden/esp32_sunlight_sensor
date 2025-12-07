/**
* @file main.c
 *
 * ESP-IDF application to read and send ambient light levels using the BH1750 sensor.
 *
 * Copyright (c) 2025 Caden Howell (cadenhowell@gmail.com)
 *
 * Developed with assistance from ChatGPT 4o (2025), Google Gemini 2.5 Pro (2025) and Claude Sonnet 4 (2025).
 *
 * Apache 2.0 Licensed as described in the file LICENSE
 */

#include "app_config.h"
#include "network_manager.h"
#include "light_sensor.h"
#include "data_processor.h"
#include "api_client.h"
#include "crash_handler.h"
#include "status_reporter.h"
#include "power_management.h"
#include "adc_battery.h"
#include "internal_temp.h"
#include "time_utils.h"
#include "persistent_storage.h"
#include "sensor_data.h"
#include <nvs_flash.h>
#include <time.h>
#include <string.h>

#define TAG "MAIN"
#define MAX_READINGS_BUFFER 20
#define MIN_VALID_TIMESTAMP 1704067200  // January 1, 2024 - readings before this are invalid

// Application state that needs to persist
static i2c_dev_t *light_sensor_dev = NULL;
static sensor_reading_t reading_buffer[MAX_READINGS_BUFFER];

// NTP sync tracking in RTC memory
static RTC_DATA_ATTR uint64_t last_ntp_sync_timestamp = 0;

// Forward declarations
static void take_sensor_reading(void);
static void send_data_to_server(void);
static bool init_peripherals(void);
static void handle_initial_boot(void);
static void clear_reading_buffer(void);
static bool is_time_valid(void);

void app_main(void) {
    // CRITICAL: Delay to allow USB serial to reconnect after deep sleep wake
    // Without this, early log messages will be lost
    vTaskDelay(pdMS_TO_TICKS(100));

    // Get current local time for boot logging
    time_t now;
    time(&now);
    struct tm timeinfo;
    localtime_r(&now, &timeinfo);
    char time_str[32];
    strftime(time_str, sizeof(time_str), "%Y-%m-%d %H:%M:%S", &timeinfo);

    // Print to both stdout and ESP log system for reliability
    printf("\n\n");
    printf("╔════════════════════════════════════════╗\n");
    printf("║  ESP32 BOOT                            ║\n");
    printf("╠════════════════════════════════════════╣\n");
    printf("║  Local time:     %-21s ║\n", time_str);
    printf("║  Reset reason:   %-21d ║\n", esp_reset_reason());
    printf("║  Wakeup cause:   %-21d ║\n", esp_sleep_get_wakeup_cause());
    printf("║  Free heap:      %-21lu ║\n", (unsigned long)esp_get_free_heap_size());
    printf("╚════════════════════════════════════════╝\n");
    printf("\n");

    ESP_LOGI(TAG, "=== ESP32 Sunlight Sensor Starting ===");
    ESP_LOGI(TAG, "Free heap: %lu bytes", (unsigned long)esp_get_free_heap_size());

    // Check wakeup reason (used for logging in check_wakeup_reason)
    check_wakeup_reason();

    // Initialize NVS
    esp_err_t ret = nvs_flash_init();
    if (ret == ESP_ERR_NVS_NO_FREE_PAGES || ret == ESP_ERR_NVS_NEW_VERSION_FOUND) {
        ESP_LOGI(TAG, "Erasing NVS flash...");
        ESP_ERROR_CHECK(nvs_flash_erase());
        ret = nvs_flash_init();
    }
    ESP_ERROR_CHECK(ret);

    // Initialize RTC state
    bool rtc_restored = rtc_state_init();
    rtc_sleep_state_t* state = rtc_state_get();

    // Clear reading buffer on fresh boot
    if (!rtc_restored) {
        clear_reading_buffer();
        state->readings_taken = 0;
    }

    // Check for crash and report if needed (only on true first boot)
    if (!rtc_restored) {
        check_and_report_crash();
    }

    // Initialize peripherals
    if (!init_peripherals()) {
        ESP_LOGE(TAG, "Failed to initialize peripherals");
        vTaskDelay(pdMS_TO_TICKS(5000));
        esp_restart();
    }

    // Initialize time keeping
    setenv("TZ", CONFIG_LOCAL_TIMEZONE, 1);
    tzset();

    // Handle initial boot sequence (first boot only)
    if (!rtc_restored) {
        handle_initial_boot();
    }

    // Check if it's nighttime (unless testing flag is set)
    bool is_night = false;
    if (!IGNORE_NIGHTTIME_FOR_TESTING) {
        is_night = is_nighttime_local();
    } else {
        ESP_LOGW(TAG, "⚠️  NIGHTTIME DETECTION DISABLED FOR TESTING ⚠️");
    }

    if (is_night) {
        ESP_LOGI(TAG, "Nighttime detected");
        log_local_time_status();

        if (should_enter_deep_sleep()) {
            uint64_t sleep_time = calculate_sleep_duration_us(true);
            ESP_LOGI(TAG, "Entering nighttime deep sleep for %llu minutes",
                     sleep_time / (60 * 1000000ULL));
            enter_deep_sleep(sleep_time);
            // Does not return
        } else {
            ESP_LOGI(TAG, "Skipping nighttime deep sleep (USB powered)");
            vTaskDelay(pdMS_TO_TICKS(30000)); // Wait 30 seconds
            esp_restart();
        }
    }

    // Daytime operation
    ESP_LOGI(TAG, "Daytime operation mode");
    log_local_time_status();

    // Take a sensor reading (only if time is valid)
    if (is_time_valid()) {
        take_sensor_reading();
    } else {
        ESP_LOGW(TAG, "Skipping reading - system time not valid yet");
    }

    // Check if it's time to send data
    if (is_time_to_send_data() || state->wifi_send_failed) {
        send_data_to_server();
    } else {
        ESP_LOGI(TAG, "Not time to send yet. Next send in %lu seconds",
                 seconds_until_next_send());
    }

    // Calculate next sleep duration (pass false since we're treating it as daytime)
    uint64_t sleep_time = calculate_sleep_duration_us(false);

    ESP_LOGI(TAG, "Readings taken: %lu/%d", state->readings_taken, MAX_READINGS_BUFFER);
    ESP_LOGI(TAG, "Next wake in %llu seconds", sleep_time / 1000000ULL);

    // Enter deep sleep if conditions are met
    if (should_enter_deep_sleep()) {
        // Get current local time
        time(&now);
        localtime_r(&now, &timeinfo);
        strftime(time_str, sizeof(time_str), "%Y-%m-%d %H:%M:%S", &timeinfo);

        ESP_LOGI(TAG, "╔════════════════════════════════════════╗");
        ESP_LOGI(TAG, "║  SLEEP SUMMARY                         ║");
        ESP_LOGI(TAG, "╠════════════════════════════════════════╣");
        ESP_LOGI(TAG, "║  Local time:     %-21s ║", time_str);
        ESP_LOGI(TAG, "║  Boot count:     %5lu                 ║", state->boot_count);
        ESP_LOGI(TAG, "║  Readings taken: %5lu                 ║", state->readings_taken);
        ESP_LOGI(TAG, "║  Sleep duration: %5llu seconds        ║", sleep_time / 1000000ULL);
        ESP_LOGI(TAG, "╚════════════════════════════════════════╝");
        enter_deep_sleep(sleep_time);
        // Does not return
    } else {
        // USB powered - use delay instead of sleep
        ESP_LOGI(TAG, "USB powered - using delay instead of sleep");
        vTaskDelay(pdMS_TO_TICKS(sleep_time / 1000));
        esp_restart();
    }
}

static bool init_peripherals(void) {
    // REMOVED: New I2C driver initialization that conflicted with i2cdev
    // The i2cdev library (used by light_sensor) will initialize the old I2C driver

    // Initialize light sensor (this will call i2cdev_init internally)
    if (init_light_sensor(&light_sensor_dev) != ESP_OK) {
        ESP_LOGE(TAG, "Failed to initialize light sensor");
        return false;
    }

    // Initialize ADC for battery monitoring
    if (adc_battery_init() != ESP_OK) {
        ESP_LOGW(TAG, "Battery monitoring init failed (may not have battery)");
    }

    // Initialize internal temperature sensor
    internal_temp_init();

    // Initialize persistent storage
    persistent_storage_init();

    // Initialize data processor (for sending stored readings)
    data_processor_init();

    return true;
}

static void handle_initial_boot(void) {
    ESP_LOGI(TAG, "=== INITIAL BOOT SEQUENCE ===");
    ESP_LOGI(TAG, "Performing first-time setup...");

    // Connect to WiFi for initial setup
    if (!initialize_network_connection(15)) {
        ESP_LOGE(TAG, "Failed to connect to WiFi on initial boot");
        return;
    }

    // Handle NTP synchronization
    time_t temp_ntp_time = last_ntp_sync_timestamp;
    handle_ntp_sync(&temp_ntp_time, true);
    if (temp_ntp_time != last_ntp_sync_timestamp) {
        last_ntp_sync_timestamp = (uint64_t)time(NULL);
        ESP_LOGI(TAG, "Initial NTP sync successful");
    }

    // Send initial status
    send_device_status_if_appropriate();

    // Disconnect WiFi to save power
    disconnect_wifi_for_power_saving();

    ESP_LOGI(TAG, "=== INITIAL BOOT COMPLETE ===");
}

static bool is_time_valid(void) {
    time_t now;
    time(&now);
    // Consider time valid if it's after January 1, 2024
    return (now >= MIN_VALID_TIMESTAMP);
}

static void clear_reading_buffer(void) {
    memset(reading_buffer, 0, sizeof(reading_buffer));
    ESP_LOGI(TAG, "Reading buffer cleared");
}

static void take_sensor_reading(void) {
    rtc_sleep_state_t* state = rtc_state_get();

    // Double-check time is valid before taking reading
    if (!is_time_valid()) {
        ESP_LOGW(TAG, "Cannot take reading - system time invalid");
        return;
    }

    float lux = 0;
    esp_err_t result = get_ambient_light(light_sensor_dev, &lux);

    if (result != ESP_OK) {
        ESP_LOGE(TAG, "Failed to read light sensor: %s", esp_err_to_name(result));
        return;
    }

    ESP_LOGI(TAG, "Light level: %.2f lux", lux);

    // Read internal temperature
    float chip_temp_c = 0;
    esp_err_t temp_err = internal_temp_read(&chip_temp_c);

    // Get current time for timestamp
    time_t now;
    time(&now);

    // Store reading in buffer (using circular buffer index)
    int idx = state->readings_taken % MAX_READINGS_BUFFER;
    reading_buffer[idx].timestamp = now;
    reading_buffer[idx].lux = lux;

    if (temp_err == ESP_OK) {
        reading_buffer[idx].chip_temp_c = chip_temp_c;
        reading_buffer[idx].chip_temp_f = (chip_temp_c * 9.0f / 5.0f) + 32.0f;
        ESP_LOGI(TAG, "Reading #%lu: Lux=%.2f, Temp=%.1f°C/%.1f°F",
                 state->readings_taken + 1, lux, chip_temp_c, reading_buffer[idx].chip_temp_f);
    } else {
        reading_buffer[idx].chip_temp_c = -999.0f;
        reading_buffer[idx].chip_temp_f = -999.0f;
        ESP_LOGI(TAG, "Reading #%lu: Lux=%.2f, Temp=error",
                 state->readings_taken + 1, lux);
    }

    state->readings_taken++;

    // Save reading to persistent storage if buffer is full or send failed
    if ((state->readings_taken % MAX_READINGS_BUFFER == 0) || state->wifi_send_failed) {
        int count = (state->readings_taken % MAX_READINGS_BUFFER == 0) ?
                     MAX_READINGS_BUFFER : (state->readings_taken % MAX_READINGS_BUFFER);
        ESP_LOGI(TAG, "Saving %d readings to persistent storage", count);
        persistent_storage_save_readings(reading_buffer, count);
    }
}

static void send_data_to_server(void) {
    rtc_sleep_state_t* state = rtc_state_get();

    ESP_LOGI(TAG, "=== DATA SEND CYCLE START ===");
    ESP_LOGI(TAG, "Free heap: %lu bytes", (unsigned long)esp_get_free_heap_size());

    // Connect to WiFi
    if (!initialize_network_connection(15)) {
        ESP_LOGE(TAG, "Failed to connect to WiFi");
        state->wifi_send_failed = true;

        // Save current readings
        int count = state->readings_taken % MAX_READINGS_BUFFER;
        if (count > 0) {
            persistent_storage_save_readings(reading_buffer, count);
        }
        return;
    }

    ESP_LOGI(TAG, "WiFi connected successfully");

    // Handle NTP synchronization
    time_t current_time = time(NULL);
    bool force_ntp_sync = false;

    // Force sync if we haven't synced yet or if it's been a long time
    if (last_ntp_sync_timestamp == 0 ||
        ((uint64_t)current_time - last_ntp_sync_timestamp) > (24 * 3600)) { // 24 hours
        force_ntp_sync = true;
        ESP_LOGI(TAG, "NTP sync needed (last sync: %llu seconds ago)",
                 last_ntp_sync_timestamp == 0 ? 0 : (uint64_t)current_time - last_ntp_sync_timestamp);
    }

    time_t temp_ntp_time = last_ntp_sync_timestamp;
    handle_ntp_sync(&temp_ntp_time, force_ntp_sync);
    if (temp_ntp_time != last_ntp_sync_timestamp) {
        last_ntp_sync_timestamp = (uint64_t)time(NULL);
    }

    // Send device status (includes battery status)
    ESP_LOGI(TAG, "Sending device status");
    send_device_status_if_appropriate();

    // Send stored readings from previous failed attempts
    if (state->wifi_send_failed) {
        ESP_LOGI(TAG, "Previous send failed, sending stored readings first");
        if (send_all_stored_readings()) {
            ESP_LOGI(TAG, "Successfully sent stored readings");
        } else {
            ESP_LOGW(TAG, "Failed to send stored readings");
        }
    }

    // Send current buffered readings
    int count = state->readings_taken % MAX_READINGS_BUFFER;
    if (count > 0 || state->readings_taken >= MAX_READINGS_BUFFER) {
        if (count == 0) count = MAX_READINGS_BUFFER;

        ESP_LOGI(TAG, "Sending %d current readings", count);
        esp_err_t result = api_send_sensor_data(reading_buffer, count);

        if (result == ESP_OK) {
            ESP_LOGI(TAG, "Successfully sent readings");
            state->wifi_send_failed = false;

            // Clear the buffer and reset counter after successful send
            clear_reading_buffer();
            state->readings_taken = 0;
        } else {
            ESP_LOGE(TAG, "Failed to send readings: %s", esp_err_to_name(result));
            state->wifi_send_failed = true;
            // Save to persistent storage
            persistent_storage_save_readings(reading_buffer, count);
        }
    } else {
        ESP_LOGI(TAG, "No new readings to send");
    }

    // Update last send timestamp
    state->last_send_timestamp = (uint64_t)time(NULL);

    // Disconnect WiFi for power savings
    disconnect_wifi_for_power_saving();

    ESP_LOGI(TAG, "=== DATA SEND CYCLE END ===");
}