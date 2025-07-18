/**
* @file test_http.c
 *
 * Unit tests for the http module.
 *
 * Copyright (c) 2025 Caden Howell (cadenhowell@gmail.com)
 *
 * Developed with assistance from ChatGPT 4o (2025) and Google Gemini 2.5 Pro (2025).
 *
 * Apache 2.0 Licensed as described in the file LICENSE
 */

#include <string.h>
#include <time.h>
#include "unity.h"
#include "cJSON.h"
#include "esp_log.h" // Include for esp_log_level_t
#include "http.h" // The file we are testing
#include "sensor_data.h" // Include for sensor_reading_t

// =========================================================================
// ========================== Mocks & Test State ===========================
// =========================================================================

// --- State variables to record mock behavior ---
static char g_captured_json_payload[256];
static int g_send_payload_call_count;

// --- Mock Implementations ---

// This is our primary mock. It overrides the weak `_send_json_payload` function
// in http.c, allowing us to capture its input without touching the network.
void _send_json_payload(const char* json_payload, const char* bearer_token) {
    g_send_payload_call_count++;
    TEST_ASSERT_NOT_NULL(json_payload);
    strncpy(g_captured_json_payload, json_payload, sizeof(g_captured_json_payload) - 1);
}

// We still need to mock esp_log_write because it's called by the code under test.
void __wrap_esp_log_write(esp_log_level_t level, const char *tag, const char *format, ...) {
    // Ignore logs for this test.
}

// Mock time functions to return a predictable timestamp.
time_t __wrap_time(time_t *timer) {
    return 1672531200; // A fixed time: 2023-01-01 00:00:00 UTC
}
struct tm *__wrap_gmtime(const time_t *timer) {
    static struct tm static_tm;
    static_tm.tm_year = 2023 - 1900;
    static_tm.tm_mon = 0; // January
    static_tm.tm_mday = 1;
    return &static_tm;
}
size_t __wrap_strftime(char *s, size_t max, const char *fmt, const struct tm *tm) {
    // Return a fixed, predictable timestamp string.
    const char* fixed_time = "2023-01-01T00:00:00Z";
    strncpy(s, fixed_time, max);
    return strlen(fixed_time);
}

// =========================================================================
// ============================= Test Cases ================================
// =========================================================================

void setUp(void) {
    memset(g_captured_json_payload, 0, sizeof(g_captured_json_payload));
    g_send_payload_call_count = 0;
}

void tearDown(void) {}

void test_send_status_update_generates_correct_json(void) {
    // --- Arrange ---
    const char *status_msg = "wifi connected";
    const char *sensor_id = "test-sensor-123";
    const char *token = "dummy-token";

    // --- Act ---
    send_status_update(status_msg, sensor_id, token);

    // --- Assert ---
    // Check that our mock was called
    TEST_ASSERT_EQUAL(1, g_send_payload_call_count);

    // Check that the JSON payload passed to our mock contains the correct data.
    TEST_ASSERT_NOT_NULL(strstr(g_captured_json_payload, "\"status\":\"wifi connected\""));
    TEST_ASSERT_NOT_NULL(strstr(g_captured_json_payload, "\"sensor_id\":\"test-sensor-123\""));
    TEST_ASSERT_NOT_NULL(strstr(g_captured_json_payload, "\"timestamp\":\"2023-01-01T00:00:00Z\""));
}

void test_send_sensor_data_generates_correct_json(void) {
    // --- Arrange ---
    sensor_reading_t readings[] = {
        { .timestamp = 1672531200, .lux = 123.45f }
    };
    const char *sensor_id = "test-sensor-123";
    const char *token = "dummy-token";

    // --- Act ---
    send_sensor_data(readings, 1, sensor_id, token);

    // --- Assert ---
    TEST_ASSERT_EQUAL(1, g_send_payload_call_count);
    TEST_ASSERT_NOT_NULL(strstr(g_captured_json_payload, "\"light_intensity\":123.45"));
    TEST_ASSERT_NOT_NULL(strstr(g_captured_json_payload, "\"sensor_id\":\"test-sensor-123\""));
    TEST_ASSERT_NOT_NULL(strstr(g_captured_json_payload, "\"timestamp\":\"2023-01-01T00:00:00Z\""));
}

void app_main(void) {
    UNITY_BEGIN();
    RUN_TEST(test_send_status_update_generates_correct_json);
    RUN_TEST(test_send_sensor_data_generates_correct_json);
    UNITY_END();
}