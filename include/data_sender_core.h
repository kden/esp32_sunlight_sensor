/**
* @file data_sender_core.h
 *
 * Pure business logic for data sending with no ESP-IDF dependencies
 * Keep it this way for unit testing
 *
 * Copyright (c) 2025 Caden Howell (cadenhowell@gmail.com)
 *
 * Developed with assistance from ChatGPT 4o (2025), Google Gemini 2.5 Pro (2025), and Claude Sonnet 4 (2025).
 *
 * Apache 2.0 Licensed as described in the file LICENSE
 */
#pragma once

#include <time.h>
#include <stdbool.h>

// Pure data structures - no ESP-IDF types
typedef struct {
    time_t timestamp;
    float lux;
} reading_t;

typedef struct {
    reading_t *buffer;
    int count;
    int capacity;
} reading_buffer_t;

typedef enum {
    SEND_RESULT_SUCCESS,
    SEND_RESULT_NO_NETWORK,
    SEND_RESULT_SEND_FAILED,
    SEND_RESULT_NO_DATA
} send_result_t;

typedef enum {
    POWER_MODE_LOW,
    POWER_MODE_HIGH
} power_mode_t;

// Pure function interfaces - easily mockable
typedef struct {
    bool (*is_network_connected)(void);
    void (*connect_network)(void);
    void (*disconnect_network)(void);
    bool (*send_data)(const reading_t* readings, int count);
    bool (*should_sync_time)(time_t last_sync, time_t now);
    void (*sync_time)(void);
    power_mode_t (*get_power_mode)(void);
    void (*log_message)(const char* level, const char* message);
} network_interface_t;

// Core business logic - pure functions, easy to test
send_result_t process_data_send_cycle(
    reading_buffer_t* current_readings,
    reading_buffer_t* unsent_buffer,
    time_t now,
    time_t* last_ntp_sync,
    const network_interface_t* network
);

void add_readings_to_buffer(
    reading_buffer_t* buffer,
    const reading_t* new_readings,
    int count
);

bool should_send_data(time_t last_send, time_t now, int interval_seconds);

// Buffer management - pure C, no ESP-IDF
void init_reading_buffer(reading_buffer_t* buffer, reading_t* storage, int capacity);
void clear_reading_buffer(reading_buffer_t* buffer);
int get_reading_count(const reading_buffer_t* buffer);