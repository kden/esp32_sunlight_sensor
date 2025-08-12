/**
 * @file test_sensor_buffer_core.c
 *
 * Unit tests for pure sensor buffering logic
 *
 * Copyright (c) 2025 Caden Howell (cadenhowell@gmail.com)
 *
 * Developed with assistance from ChatGPT 4o (2025), Google Gemini 2.5 Pro (2025) and Claude Sonnet 4 (2025).
 *
 * Apache 2.0 Licensed as described in the file LICENSE
 */
#include <stdio.h>
#include <assert.h>
#include <string.h>
#include <time.h>
#include <sys/time.h>
#include <stdbool.h>
#include "sensor_buffer_core.h"

// NOTE: Do NOT include app_context.h or any ESP-IDF headers in pure tests!

// Test result tracking (reuse from data sender tests)
typedef struct {
    const char* name;
    bool passed;
    double duration_ms;
    char error_message[512];
} test_result_t;

#define MAX_TESTS 15
static test_result_t test_results[MAX_TESTS];
static int test_count = 0;
static struct timeval test_start_time;

// Mock buffer interface
typedef struct {
    bool lock_success;
    int lock_call_count;
    int unlock_call_count;
    char last_log_level[16];
    char last_log_message[256];
} mock_buffer_interface_t;

static mock_buffer_interface_t g_mock_interface;

// Mock functions
static bool mock_acquire_lock(void) {
    g_mock_interface.lock_call_count++;
    return g_mock_interface.lock_success;
}

static void mock_release_lock(void) {
    g_mock_interface.unlock_call_count++;
}

static void mock_log(const char* level, const char* message) {
    strncpy(g_mock_interface.last_log_level, level, sizeof(g_mock_interface.last_log_level) - 1);
    strncpy(g_mock_interface.last_log_message, message, sizeof(g_mock_interface.last_log_message) - 1);
}

static const buffer_interface_t mock_interface = {
    .acquire_buffer_lock = mock_acquire_lock,
    .release_buffer_lock = mock_release_lock,
    .log_message = mock_log
};

// Test framework functions (same as data sender tests)
static void start_test(const char* test_name) {
    gettimeofday(&test_start_time, NULL);
    printf("Running %s...\n", test_name);

    if (test_count < MAX_TESTS) {
        test_results[test_count].name = test_name;
        test_results[test_count].passed = false;
        test_results[test_count].error_message[0] = '\0';
    }
}

static void end_test(bool passed, const char* error_msg) {
    struct timeval end_time;
    gettimeofday(&end_time, NULL);

    if (test_count < MAX_TESTS) {
        test_results[test_count].passed = passed;
        test_results[test_count].duration_ms =
            (end_time.tv_sec - test_start_time.tv_sec) * 1000.0 +
            (end_time.tv_usec - test_start_time.tv_usec) / 1000.0;

        if (!passed && error_msg) {
            strncpy(test_results[test_count].error_message, error_msg,
                   sizeof(test_results[test_count].error_message) - 1);
        }

        printf("✓ %s %s (%.2fms)\n",
               test_results[test_count].name,
               passed ? "PASSED" : "FAILED",
               test_results[test_count].duration_ms);

        test_count++;
    }
}

#define TEST_ASSERT(condition, message) do { \
    if (!(condition)) { \
        end_test(false, message); \
        return; \
    } \
} while(0)

// Test functions
void test_successful_sensor_reading_add(void) {
    start_test("test_successful_sensor_reading_add");

    // Setup
    memset(&g_mock_interface, 0, sizeof(g_mock_interface));
    g_mock_interface.lock_success = true;

    sensor_data_t storage[5];
    int count = 0;
    sensor_buffer_t buffer;
    init_sensor_buffer(&buffer, storage, &count, 5);

    // Test
    buffer_result_t result = add_sensor_reading(&buffer, 1000, 25.5f, &mock_interface);

    // Verify
    TEST_ASSERT(result == BUFFER_RESULT_SUCCESS, "Expected BUFFER_RESULT_SUCCESS");
    TEST_ASSERT(count == 1, "Buffer count should be 1");
    TEST_ASSERT(storage[0].timestamp == 1000, "Timestamp should be 1000");
    TEST_ASSERT(storage[0].lux == 25.5f, "Lux should be 25.5");
    TEST_ASSERT(g_mock_interface.lock_call_count == 1, "Should acquire lock once");
    TEST_ASSERT(g_mock_interface.unlock_call_count == 1, "Should release lock once");

    end_test(true, NULL);
}

void test_buffer_full_handling(void) {
    start_test("test_buffer_full_handling");

    // Setup
    memset(&g_mock_interface, 0, sizeof(g_mock_interface));
    g_mock_interface.lock_success = true;

    sensor_data_t storage[2];
    int count = 2;  // Start with full buffer
    sensor_buffer_t buffer;
    init_sensor_buffer(&buffer, storage, &count, 2);

    // Test
    buffer_result_t result = add_sensor_reading(&buffer, 1000, 25.5f, &mock_interface);

    // Verify
    TEST_ASSERT(result == BUFFER_RESULT_FULL, "Expected BUFFER_RESULT_FULL");
    TEST_ASSERT(count == 2, "Buffer count should remain 2 (unchanged)");
    TEST_ASSERT(strstr(g_mock_interface.last_log_message, "buffer full") != NULL,
                "Should log buffer full warning");

    end_test(true, NULL);
}

void test_lock_failure_handling(void) {
    start_test("test_lock_failure_handling");

    // Setup
    memset(&g_mock_interface, 0, sizeof(g_mock_interface));
    g_mock_interface.lock_success = false;  // Simulate lock failure

    sensor_data_t storage[5];
    int count = 0;
    sensor_buffer_t buffer;
    init_sensor_buffer(&buffer, storage, &count, 5);

    // Test
    buffer_result_t result = add_sensor_reading(&buffer, 1000, 25.5f, &mock_interface);

    // Verify
    TEST_ASSERT(result == BUFFER_RESULT_ERROR, "Expected BUFFER_RESULT_ERROR");
    TEST_ASSERT(count == 0, "Buffer count should remain 0");
    TEST_ASSERT(g_mock_interface.lock_call_count == 1, "Should attempt to acquire lock");
    TEST_ASSERT(g_mock_interface.unlock_call_count == 0, "Should not release lock on failure");

    end_test(true, NULL);
}

void test_multiple_readings_sequential(void) {
    start_test("test_multiple_readings_sequential");

    // Setup
    memset(&g_mock_interface, 0, sizeof(g_mock_interface));
    g_mock_interface.lock_success = true;

    sensor_data_t storage[5];
    int count = 0;
    sensor_buffer_t buffer;
    init_sensor_buffer(&buffer, storage, &count, 5);

    // Test - add multiple readings
    for (int i = 0; i < 3; i++) {
        buffer_result_t result = add_sensor_reading(&buffer, 1000 + i, 10.0f + i, &mock_interface);
        TEST_ASSERT(result == BUFFER_RESULT_SUCCESS, "Each reading should succeed");
    }

    // Verify
    TEST_ASSERT(count == 3, "Buffer should contain 3 readings");
    TEST_ASSERT(storage[0].timestamp == 1000, "First reading timestamp");
    TEST_ASSERT(storage[1].timestamp == 1001, "Second reading timestamp");
    TEST_ASSERT(storage[2].timestamp == 1002, "Third reading timestamp");
    TEST_ASSERT(storage[0].lux == 10.0f, "First reading lux");
    TEST_ASSERT(storage[1].lux == 11.0f, "Second reading lux");
    TEST_ASSERT(storage[2].lux == 12.0f, "Third reading lux");

    end_test(true, NULL);
}

void test_buffer_utility_functions(void) {
    start_test("test_buffer_utility_functions");

    sensor_data_t storage[5];
    int count = 3;
    sensor_buffer_t buffer;
    init_sensor_buffer(&buffer, storage, &count, 5);

    // Test utility functions
    TEST_ASSERT(get_buffer_usage(&buffer) == 3, "Buffer usage should be 3");
    TEST_ASSERT(!is_buffer_full(&buffer), "Buffer should not be full");

    count = 5;  // Fill buffer
    TEST_ASSERT(is_buffer_full(&buffer), "Buffer should be full");
    TEST_ASSERT(get_buffer_usage(&buffer) == 5, "Buffer usage should be 5");

    clear_sensor_buffer(&buffer);
    TEST_ASSERT(count == 0, "Buffer should be cleared");
    TEST_ASSERT(!is_buffer_full(&buffer), "Buffer should not be full after clear");

    end_test(true, NULL);
}

void test_invalid_parameters(void) {
    start_test("test_invalid_parameters");

    sensor_data_t storage[5];
    int count = 0;
    sensor_buffer_t buffer;

    // Test init with invalid parameters
    init_sensor_buffer(NULL, storage, &count, 5);  // Should handle gracefully
    init_sensor_buffer(&buffer, NULL, &count, 5);  // Should handle gracefully
    init_sensor_buffer(&buffer, storage, NULL, 5); // Should handle gracefully
    init_sensor_buffer(&buffer, storage, &count, 0); // Should handle gracefully

    // Test add_sensor_reading with NULL buffer
    buffer_result_t result = add_sensor_reading(NULL, 1000, 25.5f, &mock_interface);
    TEST_ASSERT(result == BUFFER_RESULT_ERROR, "Should return error for NULL buffer");

    // Test utility functions with NULL
    TEST_ASSERT(get_buffer_usage(NULL) == 0, "Should return 0 for NULL buffer");
    TEST_ASSERT(!is_buffer_full(NULL), "Should return false for NULL buffer");

    end_test(true, NULL);
}

// JUnit XML output (same as data sender tests)
void write_junit_xml(const char* filename) {
    FILE* file = fopen(filename, "w");
    if (!file) {
        printf("Error: Could not create JUnit XML file %s\n", filename);
        return;
    }

    int passed = 0, failed = 0;
    double total_time = 0;

    for (int i = 0; i < test_count; i++) {
        if (test_results[i].passed) {
            passed++;
        } else {
            failed++;
        }
        total_time += test_results[i].duration_ms;
    }

    fprintf(file, "<?xml version=\"1.0\" encoding=\"UTF-8\"?>\n");
    fprintf(file, "<testsuites>\n");
    fprintf(file, "  <testsuite name=\"SensorBufferCoreTests\" tests=\"%d\" failures=\"%d\" time=\"%.3f\">\n",
            test_count, failed, total_time / 1000.0);

    for (int i = 0; i < test_count; i++) {
        fprintf(file, "    <testcase name=\"%s\" classname=\"SensorBufferCoreTests\" time=\"%.3f\"",
                test_results[i].name, test_results[i].duration_ms / 1000.0);

        if (test_results[i].passed) {
            fprintf(file, "/>\n");
        } else {
            fprintf(file, ">\n");
            fprintf(file, "      <failure message=\"%s\">%s</failure>\n",
                    test_results[i].error_message, test_results[i].error_message);
            fprintf(file, "    </testcase>\n");
        }
    }

    fprintf(file, "  </testsuite>\n");
    fprintf(file, "</testsuites>\n");
    fclose(file);
    printf("JUnit XML report written to %s\n", filename);
}

int main(int argc, char* argv[]) {
    printf("Running sensor buffer core tests...\n");
    printf("==================================\n");

    test_successful_sensor_reading_add();
    test_buffer_full_handling();
    test_lock_failure_handling();
    test_multiple_readings_sequential();
    test_buffer_utility_functions();
    test_invalid_parameters();

    printf("==================================\n");

    int failed = 0;
    for (int i = 0; i < test_count; i++) {
        if (!test_results[i].passed) {
            failed++;
        }
    }

    printf("Tests: %d, Passed: %d, Failed: %d\n", test_count, test_count - failed, failed);

    const char* xml_file = (argc > 1) ? argv[1] : "sensor-buffer-test-results.xml";
    write_junit_xml(xml_file);

    if (failed == 0) {
        printf("✅ All sensor buffer tests passed!\n");
        return 0;
    } else {
        printf("❌ %d test(s) failed!\n", failed);
        return 1;
    }
}