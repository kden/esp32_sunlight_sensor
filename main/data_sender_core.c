/**
 * @file data_sender_core.c
 *
 * Pure business logic implementation - easily testable
 *
 * Copyright (c) 2025 Caden Howell (cadenhowell@gmail.com)
 *
 * Developed with assistance from Claude Sonnet 4 (2025).
 *
 * Apache 2.0 Licensed as described in the file LICENSE
 */
#include "data_sender_core.h"
#include <string.h>
#include <stdio.h>

send_result_t process_data_send_cycle(
    reading_buffer_t* current_readings,
    reading_buffer_t* unsent_buffer,
    time_t now,
    time_t* last_ntp_sync,
    const network_interface_t* network)
{
    // Pure business logic - no ESP-IDF dependencies!

    if (current_readings->count == 0 && unsent_buffer->count == 0) {
        network->log_message("INFO", "No readings to send");
        return SEND_RESULT_NO_DATA;
    }

    // Try to connect
    if (!network->is_network_connected()) {
        network->log_message("INFO", "Connecting to network...");
        network->connect_network();

        if (!network->is_network_connected()) {
            network->log_message("ERROR", "Failed to connect to network");
            // Store current readings for later
            if (current_readings->count > 0) {
                add_readings_to_buffer(unsent_buffer, current_readings->buffer, current_readings->count);
                clear_reading_buffer(current_readings);
            }
            return SEND_RESULT_NO_NETWORK;
        }
    }

    // Sync time if needed
    if (network->should_sync_time(*last_ntp_sync, now)) {
        network->log_message("INFO", "Syncing time");
        network->sync_time();
        *last_ntp_sync = now;
    }

    bool all_sent = true;

    // Send stored readings first
    if (unsent_buffer->count > 0) {
        char msg[64];
        snprintf(msg, sizeof(msg), "Sending %d stored readings", unsent_buffer->count);
        network->log_message("INFO", msg);

        if (network->send_data(unsent_buffer->buffer, unsent_buffer->count)) {
            network->log_message("INFO", "Stored readings sent successfully");
            clear_reading_buffer(unsent_buffer);
        } else {
            network->log_message("ERROR", "Failed to send stored readings");
            all_sent = false;
        }
    }

    // Send current readings
    if (current_readings->count > 0) {
        char msg[64];
        snprintf(msg, sizeof(msg), "Sending %d new readings", current_readings->count);
        network->log_message("INFO", msg);

        if (network->send_data(current_readings->buffer, current_readings->count)) {
            network->log_message("INFO", "New readings sent successfully");
            clear_reading_buffer(current_readings);
        } else {
            network->log_message("ERROR", "Failed to send new readings");
            add_readings_to_buffer(unsent_buffer, current_readings->buffer, current_readings->count);
            clear_reading_buffer(current_readings);
            all_sent = false;
        }
    }

    // Handle power management
    if (network->get_power_mode() == POWER_MODE_LOW) {
        network->log_message("INFO", "Disconnecting network for power saving");
        network->disconnect_network();
    }

    return all_sent ? SEND_RESULT_SUCCESS : SEND_RESULT_SEND_FAILED;
}

void add_readings_to_buffer(reading_buffer_t* buffer, const reading_t* new_readings, int count) {
    if (count <= 0 || !buffer || !new_readings) {
        return;
    }

    printf("  [DEBUG] Adding %d readings to buffer (current count: %d, capacity: %d)\n",
           count, buffer->count, buffer->capacity);

    // Handle overflow by dropping oldest readings
    if (buffer->count + count > buffer->capacity) {
        int overflow = (buffer->count + count) - buffer->capacity;
        printf("  [DEBUG] Overflow detected: need to drop %d oldest readings\n", overflow);

        // Make sure we don't try to move more data than we have
        if (overflow >= buffer->count) {
            // Complete replacement - just clear the buffer
            printf("  [DEBUG] Complete replacement - clearing buffer\n");
            buffer->count = 0;
        } else {
            // Shift existing data left to make room
            int keep_count = buffer->count - overflow;
            printf("  [DEBUG] Keeping %d existing readings, shifting left by %d\n", keep_count, overflow);

            memmove(&buffer->buffer[0],
                   &buffer->buffer[overflow],
                   keep_count * sizeof(reading_t));
            buffer->count = keep_count;
        }
    }

    // Now add new readings (but only as many as will fit)
    int can_add = buffer->capacity - buffer->count;
    if (count > can_add) {
        printf("  [DEBUG] Can only add %d of %d new readings\n", can_add, count);
        count = can_add;
    }

    if (count > 0) {
        printf("  [DEBUG] Copying %d readings to position %d\n", count, buffer->count);
        memcpy(&buffer->buffer[buffer->count], new_readings, count * sizeof(reading_t));
        buffer->count += count;
    }

    printf("  [DEBUG] Final buffer count: %d\n", buffer->count);
}

bool should_send_data(time_t last_send, time_t now, int interval_seconds) {
    return (now - last_send) >= interval_seconds;
}

void init_reading_buffer(reading_buffer_t* buffer, reading_t* storage, int capacity) {
    buffer->buffer = storage;
    buffer->capacity = capacity;
    buffer->count = 0;
}

void clear_reading_buffer(reading_buffer_t* buffer) {
    buffer->count = 0;
}

int get_reading_count(const reading_buffer_t* buffer) {
    return buffer->count;
}