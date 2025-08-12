/**
 * @file test_data_sender_core.c
 *
 * Simple test for pure business logic with JUnit XML output for CI/CD
 */

#include <stdio.h>
#include <assert.h>
#include <string.h>
#include <time.h>
#include <sys/time.h>
#include "data_sender_core.h"

// Test result tracking for JUnit XML
typedef struct {
    const char* name;
    bool passed;
    double duration_ms;
    char error_message[512];
} test_result_t;

#define MAX_TESTS 10
static test_result_t test_results[MAX_TESTS];
static int test_count = 0;
static struct timeval test_start_time;

// Simple mock network interface
typedef struct {
    bool connected;
    bool send_success;
    int send_call_count;
    int connect_call_count;
    char last_log[256];
} mock_network_t;

static mock_network_t g_mock;

// Mock functions
static bool mock_is_connected(void) {
    return g_mock.connected;
}

static void mock_connect(void) {
    g_mock.connect_call_count++;
}

static void mock_disconnect(void) {
    g_mock.connected = false;
}

static bool mock_send(const reading_t* readings, int count) {
    g_mock.send_call_count++;
    return g_mock.send_success;
}

static bool mock_should_sync(time_t last, time_t now) {
    return false;
}

static void mock_sync_time(void) {
    // No-op
}

static power_mode_t mock_power_mode(void) {
    return POWER_MODE_LOW;
}

static void mock_log(const char* level, const char* msg) {
    snprintf(g_mock.last_log, sizeof(g_mock.last_log), "[%s] %s", level, msg);
}

static const network_interface_t mock_interface = {
    .is_network_connected = mock_is_connected,
    .connect_network = mock_connect,
    .disconnect_network = mock_disconnect,
    .send_data = mock_send,
    .should_sync_time = mock_should_sync,
    .sync_time = mock_sync_time,
    .get_power_mode = mock_power_mode,
    .log_message = mock_log
};

// Test framework functions
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

// Enhanced assert that captures error messages
#define TEST_ASSERT(condition, message) do { \
    if (!(condition)) { \
        end_test(false, message); \
        return; \
    } \
} while(0)

// Test functions
void test_no_data_to_send(void) {
    start_test("test_no_data_to_send");

    // Setup
    memset(&g_mock, 0, sizeof(g_mock));
    reading_t current_storage[10], unsent_storage[10];
    reading_buffer_t current, unsent;
    init_reading_buffer(&current, current_storage, 10);
    init_reading_buffer(&unsent, unsent_storage, 10);
    time_t last_sync = 0;

    // Test
    send_result_t result = process_data_send_cycle(&current, &unsent, 1000, &last_sync, &mock_interface);

    // Verify
    TEST_ASSERT(result == SEND_RESULT_NO_DATA, "Expected SEND_RESULT_NO_DATA");
    TEST_ASSERT(g_mock.send_call_count == 0, "Expected no send calls");

    end_test(true, NULL);
}

void test_network_unavailable(void) {
    start_test("test_network_unavailable");

    // Setup
    memset(&g_mock, 0, sizeof(g_mock));
    g_mock.connected = false;

    reading_t current_storage[10], unsent_storage[10];
    reading_buffer_t current, unsent;
    init_reading_buffer(&current, current_storage, 10);
    init_reading_buffer(&unsent, unsent_storage, 10);

    reading_t test_reading = {.timestamp = 1000, .lux = 100.0f};
    add_readings_to_buffer(&current, &test_reading, 1);

    time_t last_sync = 0;

    // Test
    send_result_t result = process_data_send_cycle(&current, &unsent, 1000, &last_sync, &mock_interface);

    // Verify
    TEST_ASSERT(result == SEND_RESULT_NO_NETWORK, "Expected SEND_RESULT_NO_NETWORK");
    TEST_ASSERT(current.count == 0, "Current buffer should be empty");
    TEST_ASSERT(unsent.count == 1, "Unsent buffer should contain 1 reading");

    end_test(true, NULL);
}

void test_successful_send(void) {
    start_test("test_successful_send");

    // Setup
    memset(&g_mock, 0, sizeof(g_mock));
    g_mock.connected = true;
    g_mock.send_success = true;

    reading_t current_storage[10], unsent_storage[10];
    reading_buffer_t current, unsent;
    init_reading_buffer(&current, current_storage, 10);
    init_reading_buffer(&unsent, unsent_storage, 10);

    reading_t test_reading = {.timestamp = 1000, .lux = 100.0f};
    add_readings_to_buffer(&current, &test_reading, 1);

    time_t last_sync = 0;

    // Test
    send_result_t result = process_data_send_cycle(&current, &unsent, 1000, &last_sync, &mock_interface);

    // Verify
    TEST_ASSERT(result == SEND_RESULT_SUCCESS, "Expected SEND_RESULT_SUCCESS");
    TEST_ASSERT(g_mock.send_call_count == 1, "Expected 1 send call");
    TEST_ASSERT(current.count == 0, "Current buffer should be empty");
    TEST_ASSERT(unsent.count == 0, "Unsent buffer should be empty");

    end_test(true, NULL);
}

void test_send_failure_retries(void) {
    start_test("test_send_failure_retries");

    // Setup
    memset(&g_mock, 0, sizeof(g_mock));
    g_mock.connected = true;
    g_mock.send_success = false;

    reading_t current_storage[10], unsent_storage[10];
    reading_buffer_t current, unsent;
    init_reading_buffer(&current, current_storage, 10);
    init_reading_buffer(&unsent, unsent_storage, 10);

    reading_t test_reading = {.timestamp = 1000, .lux = 100.0f};
    add_readings_to_buffer(&current, &test_reading, 1);

    time_t last_sync = 0;

    // Test
    send_result_t result = process_data_send_cycle(&current, &unsent, 1000, &last_sync, &mock_interface);

    // Verify
    TEST_ASSERT(result == SEND_RESULT_SEND_FAILED, "Expected SEND_RESULT_SEND_FAILED");
    TEST_ASSERT(g_mock.send_call_count == 1, "Expected 1 send attempt");
    TEST_ASSERT(current.count == 0, "Current buffer should be cleared");
    TEST_ASSERT(unsent.count == 1, "Failed reading should be stored");

    end_test(true, NULL);
}

void test_buffer_overflow(void) {
    start_test("test_buffer_overflow");

    reading_t storage[3] = {0};
    reading_buffer_t buffer;
    init_reading_buffer(&buffer, storage, 3);

    reading_t readings[4] = {
        {.timestamp = 1000, .lux = 10.0f},
        {.timestamp = 1001, .lux = 11.0f},
        {.timestamp = 1002, .lux = 12.0f},
        {.timestamp = 1003, .lux = 13.0f}
    };

    // Add them individually
    for (int i = 0; i < 4; i++) {
        add_readings_to_buffer(&buffer, &readings[i], 1);
    }

    // Verify
    TEST_ASSERT(buffer.count == 3, "Buffer should contain exactly 3 readings");
    TEST_ASSERT(buffer.buffer[0].timestamp == 1001, "Oldest kept reading should be 1001");
    TEST_ASSERT(buffer.buffer[1].timestamp == 1002, "Middle reading should be 1002");
    TEST_ASSERT(buffer.buffer[2].timestamp == 1003, "Newest reading should be 1003");

    end_test(true, NULL);
}

// JUnit XML output
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

    // XML header and testsuite opening
    fprintf(file, "<?xml version=\"1.0\" encoding=\"UTF-8\"?>\n");
    fprintf(file, "<testsuites>\n");
    fprintf(file, "  <testsuite name=\"DataSenderCoreTests\" tests=\"%d\" failures=\"%d\" time=\"%.3f\">\n",
            test_count, failed, total_time / 1000.0);

    // Individual test cases
    for (int i = 0; i < test_count; i++) {
        fprintf(file, "    <testcase name=\"%s\" classname=\"DataSenderCoreTests\" time=\"%.3f\"",
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

    // Close tags
    fprintf(file, "  </testsuite>\n");
    fprintf(file, "</testsuites>\n");

    fclose(file);
    printf("JUnit XML report written to %s\n", filename);
}

int main(int argc, char* argv[]) {
    printf("Running data sender core tests...\n");
    printf("================================\n");

    test_no_data_to_send();
    test_network_unavailable();
    test_successful_send();
    test_send_failure_retries();
    test_buffer_overflow();

    printf("================================\n");

    int failed = 0;
    for (int i = 0; i < test_count; i++) {
        if (!test_results[i].passed) {
            failed++;
        }
    }

    printf("Tests: %d, Passed: %d, Failed: %d\n", test_count, test_count - failed, failed);

    // Write JUnit XML
    const char* xml_file = (argc > 1) ? argv[1] : "test-results.xml";
    write_junit_xml(xml_file);

    if (failed == 0) {
        printf("✅ All tests passed!\n");
        return 0;
    } else {
        printf("❌ %d test(s) failed!\n", failed);
        return 1;
    }
}