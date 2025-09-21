/**
* @file api_client.c
 *
 * High-level API client with JSON construction and chunked sending.
 *
 * Copyright (c) 2025 Caden Howell (cadenhowell@gmail.com)
 *
 * Developed with assistance from ChatGPT 4o (2025), Google Gemini 2.5 Pro (2025) and Claude Sonnet 4 (2025).
 *
 * Apache 2.0 Licensed as described in the file LICENSE
 */

#include "api_client.h"
#include "app_config.h"
#include "http_client.h"
#include "adc_battery.h"
#include "wifi_manager.h"
#include "status_reporter.h"
#include "git_version.h"
#include "cJSON.h"
#include "esp_log.h"
#include <time.h>
#include <string.h>

#define TAG "API_CLIENT"
#define MAX_READINGS_PER_CHUNK 50  // Send at most 50 readings per HTTP request

/**
 * @brief Internal function to send a single chunk of sensor data
 */
static esp_err_t send_sensor_data_chunk(const sensor_reading_t* readings, int count) {
    if (readings == NULL || count <= 0) {
        ESP_LOGE(TAG, "Invalid parameters for sensor data chunk send");
        return ESP_ERR_INVALID_ARG;
    }

    cJSON *root_array = NULL;
    char *json_payload = NULL;
    esp_err_t result = ESP_FAIL;

    // Log available heap before creating JSON
    size_t free_heap_before = esp_get_free_heap_size();
    ESP_LOGD(TAG, "Free heap before JSON creation: %zu bytes", free_heap_before);

    root_array = cJSON_CreateArray();
    if (root_array == NULL) {
        ESP_LOGE(TAG, "Failed to create JSON array");
        return ESP_ERR_NO_MEM;
    }

    int valid_readings = 0;
    for (int i = 0; i < count; i++) {
        cJSON *sensor_object = cJSON_CreateObject();
        if (sensor_object == NULL) {
            ESP_LOGE(TAG, "Failed to create sensor object for reading #%d (may be out of memory)", i);
            break;
        }

        char timestamp_str[32];
        strftime(timestamp_str, sizeof(timestamp_str), "%Y-%m-%dT%H:%M:%SZ", gmtime(&readings[i].timestamp));

        if (cJSON_AddNumberToObject(sensor_object, "light_intensity", readings[i].lux) == NULL ||
            cJSON_AddStringToObject(sensor_object, "sensor_id", CONFIG_SENSOR_ID) == NULL ||
            cJSON_AddStringToObject(sensor_object, "timestamp", timestamp_str) == NULL ||
            cJSON_AddStringToObject(sensor_object, "sensor_set_id", CONFIG_SENSOR_SET) == NULL ||
            cJSON_AddNumberToObject(sensor_object, "chip_temp_c", readings[i].chip_temp_c) == NULL ||
            cJSON_AddNumberToObject(sensor_object, "chip_temp_f", readings[i].chip_temp_f) == NULL) {
            ESP_LOGE(TAG, "Failed to add fields to sensor object for reading #%d", i);
            cJSON_Delete(sensor_object);
            break;
        }

        if (cJSON_AddItemToArray(root_array, sensor_object) == 0) {
            ESP_LOGE(TAG, "Failed to add sensor object to array for reading #%d", i);
            cJSON_Delete(sensor_object);
            break;
        }

        valid_readings++;
    }

    if (valid_readings == 0) {
        ESP_LOGW(TAG, "No valid readings to send in this chunk. Aborting HTTP POST.");
        result = ESP_ERR_INVALID_ARG;
        goto cleanup;
    }

    json_payload = cJSON_Print(root_array);
    if (json_payload == NULL) {
        ESP_LOGE(TAG, "Failed to print JSON payload (likely out of memory)");
        result = ESP_ERR_NO_MEM;
        goto cleanup;
    }

    ESP_LOGD(TAG, "JSON Payload for chunk: %s", json_payload);
    ESP_LOGI(TAG, "Sending JSON chunk with %d records.", valid_readings);

    result = http_send_json_payload(json_payload, CONFIG_BEARER_TOKEN);

cleanup:
    if (root_array) cJSON_Delete(root_array);
    if (json_payload) free(json_payload);

    // Log final heap status
    size_t free_heap_final = esp_get_free_heap_size();
    ESP_LOGD(TAG, "Free heap after cleanup: %zu bytes", free_heap_final);

    return result;
}

esp_err_t api_send_sensor_data(const sensor_reading_t* readings, int count) {
    if (readings == NULL || count <= 0) {
        ESP_LOGE(TAG, "Invalid parameters for sensor data send");
        return ESP_ERR_INVALID_ARG;
    }

    ESP_LOGI(TAG, "Sending %d readings in chunks of %d", count, MAX_READINGS_PER_CHUNK);

    // Send data in chunks to avoid memory issues
    int sent_count = 0;
    esp_err_t final_result = ESP_OK;

    while (sent_count < count) {
        int chunk_size = (count - sent_count > MAX_READINGS_PER_CHUNK) ? MAX_READINGS_PER_CHUNK : (count - sent_count);

        ESP_LOGI(TAG, "Sending chunk %d-%d of %d total readings",
                 sent_count + 1, sent_count + chunk_size, count);

        esp_err_t result = send_sensor_data_chunk(&readings[sent_count], chunk_size);

        if (result != ESP_OK) {
            ESP_LOGE(TAG, "Failed to send chunk %d-%d: %s",
                     sent_count + 1, sent_count + chunk_size, esp_err_to_name(result));
            final_result = result;
            break; // Stop sending if a chunk fails
        }

        sent_count += chunk_size;
        ESP_LOGI(TAG, "Successfully sent chunk. Progress: %d/%d readings", sent_count, count);

        // Small delay between chunks to let the system recover
        if (sent_count < count) {
            vTaskDelay(pdMS_TO_TICKS(1000)); // 1 second delay between chunks
        }
    }

    if (final_result == ESP_OK) {
        ESP_LOGI(TAG, "Successfully sent all %d readings in %d chunks",
                 count, (count + MAX_READINGS_PER_CHUNK - 1) / MAX_READINGS_PER_CHUNK);
    } else {
        ESP_LOGE(TAG, "Failed to send all readings. Sent %d/%d successfully", sent_count, count);
    }

    return final_result;
}

esp_err_t api_send_status_update(const char* status_message) {
    if (status_message == NULL) {
        ESP_LOGE(TAG, "Invalid parameters for status update");
        return ESP_ERR_INVALID_ARG;
    }

    ESP_LOGI(TAG, "Input status message length: %zu bytes", strlen(status_message));

    // Check available heap before JSON operations
    size_t heap_before = esp_get_free_heap_size();
    ESP_LOGI(TAG, "Heap before JSON creation: %zu bytes", heap_before);

    // Create enhanced status message with boot reason prefix
    char enhanced_status[256];
    create_enhanced_status_message(status_message, enhanced_status, sizeof(enhanced_status));

    cJSON *root_array = NULL;
    char *json_payload = NULL;
    esp_err_t result = ESP_FAIL;

    root_array = cJSON_CreateArray();
    if (root_array == NULL) {
        ESP_LOGE(TAG, "Failed to create JSON array - heap: %zu", esp_get_free_heap_size());
        return ESP_ERR_NO_MEM;
    }

    cJSON *status_object = cJSON_CreateObject();
    if (status_object == NULL) {
        ESP_LOGE(TAG, "Failed to create status object - heap: %zu", esp_get_free_heap_size());
        result = ESP_ERR_NO_MEM;
        goto cleanup;
    }

    time_t now;
    time(&now);
    char timestamp_str[32];
    strftime(timestamp_str, sizeof(timestamp_str), "%Y-%m-%dT%H:%M:%SZ", gmtime(&now));

    cJSON_AddStringToObject(status_object, "sensor_id", CONFIG_SENSOR_ID);
    cJSON_AddStringToObject(status_object, "timestamp", timestamp_str);
    cJSON_AddStringToObject(status_object, "sensor_set_id", CONFIG_SENSOR_SET);
    cJSON_AddStringToObject(status_object, "status", enhanced_status);

    // Add the Git commit info to the status payload
    cJSON_AddStringToObject(status_object, "commit_sha", GIT_COMMIT_SHA);
    cJSON_AddStringToObject(status_object, "commit_timestamp", GIT_COMMIT_TIMESTAMP);

    cJSON_AddItemToArray(root_array, status_object);

    size_t heap_before_print = esp_get_free_heap_size();
    ESP_LOGI(TAG, "Heap before cJSON_Print: %zu bytes", heap_before_print);

    json_payload = cJSON_PrintUnformatted(root_array);

    size_t json_length = strlen(json_payload);
    ESP_LOGI(TAG, "cJSON_PrintUnformatted returned %zu bytes (expected ~%zu)", json_length, strlen(enhanced_status) + 200);

    // Check if the JSON contains the full status message
    if (!strstr(json_payload, enhanced_status + strlen(enhanced_status) - 50)) {
        ESP_LOGE(TAG, "JSON appears truncated - status message was cut off by cJSON");
        ESP_LOGE(TAG, "Input message size: %zu bytes, JSON size: %zu bytes", strlen(enhanced_status), json_length);

        // Try to create a truncated version that will fit
        char truncated_status[2000];
        snprintf(truncated_status, sizeof(truncated_status), "%.1950s... [TRUNCATED]", enhanced_status);

        // Replace the status field with truncated version
        cJSON_SetValuestring(cJSON_GetObjectItem(status_object, "status"), truncated_status);

        // Re-generate JSON
        free(json_payload);
        json_payload = cJSON_Print(root_array);
        if (json_payload) {
            ESP_LOGI(TAG, "Regenerated JSON with truncated message: %zu bytes", strlen(json_payload));
        }
    }

    if (!strstr(json_payload, "}]")) {
        ESP_LOGE(TAG, "JSON appears truncated - missing closing brackets");
    }

    ESP_LOGI(TAG, "Sending status update: '%s'", enhanced_status);
    ESP_LOGD(TAG, "Status JSON Payload: %s", json_payload);

    result = http_send_json_payload(json_payload, CONFIG_BEARER_TOKEN);

cleanup:
    if (root_array) cJSON_Delete(root_array);
    if (json_payload) free(json_payload);
    return result;
}

esp_err_t api_send_battery_status(void) {
    float voltage;
    int percentage;
    int8_t rssi;

    // Get battery data - return error if no battery
    esp_err_t battery_err = adc_battery_get_api_data(&voltage, &percentage);
    if (battery_err != ESP_OK) {
        return battery_err; // No battery or read error
    }

    // Get WiFi data
    esp_err_t wifi_err = wifi_get_rssi(&rssi);

    cJSON *root_array = NULL;
    char *json_payload = NULL;
    esp_err_t result = ESP_FAIL;

    root_array = cJSON_CreateArray();
    if (root_array == NULL) {
        ESP_LOGE(TAG, "Failed to create JSON array for battery status update");
        return ESP_ERR_NO_MEM;
    }

    cJSON *status_object = cJSON_CreateObject();
    if (status_object == NULL) {
        ESP_LOGE(TAG, "Failed to create battery status object");
        result = ESP_ERR_NO_MEM;
        goto cleanup;
    }

    time_t now;
    time(&now);
    char timestamp_str[32];
    strftime(timestamp_str, sizeof(timestamp_str), "%Y-%m-%dT%H:%M:%SZ", gmtime(&now));

    // Create base status message and add boot reason
    char base_status[] = "battery";
    char enhanced_status[64];
    create_enhanced_status_message(base_status, enhanced_status, sizeof(enhanced_status));

    cJSON_AddStringToObject(status_object, "sensor_id", CONFIG_SENSOR_ID);
    cJSON_AddStringToObject(status_object, "timestamp", timestamp_str);
    cJSON_AddStringToObject(status_object, "sensor_set_id", CONFIG_SENSOR_SET);
    cJSON_AddStringToObject(status_object, "status", enhanced_status);

    // Add numeric battery fields
    cJSON_AddNumberToObject(status_object, "battery_voltage", voltage);
    cJSON_AddNumberToObject(status_object, "battery_percent", percentage);

    // Add WiFi strength if available
    if (wifi_err == ESP_OK) {
        cJSON_AddNumberToObject(status_object, "wifi_dbm", rssi);
    }

    // Add the Git commit info
    cJSON_AddStringToObject(status_object, "commit_sha", GIT_COMMIT_SHA);
    cJSON_AddStringToObject(status_object, "commit_timestamp", GIT_COMMIT_TIMESTAMP);

    cJSON_AddItemToArray(root_array, status_object);

    json_payload = cJSON_Print(root_array);
    if (json_payload == NULL) {
        ESP_LOGE(TAG, "Failed to print JSON payload for battery status update");
        result = ESP_ERR_NO_MEM;
        goto cleanup;
    }

    ESP_LOGI(TAG, "Sending battery status update");
    ESP_LOGD(TAG, "Battery Status JSON Payload: %s", json_payload);

    result = http_send_json_payload(json_payload, CONFIG_BEARER_TOKEN);

cleanup:
    if (root_array) cJSON_Delete(root_array);
    if (json_payload) free(json_payload);
    return result;
}