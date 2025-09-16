/**
 * @file http.c
 *
 * HTTP client for sending sensor data with proper error handling and chunked sending.
 *
 * Copyright (c) 2025 Caden Howell (cadenhowell@gmail.com)
 *
 * Developed with assistance from ChatGPT 4o (2025), Google Gemini 2.5 Pro (2025) and Claude Sonnet 4 (2025).
 *
 * Apache 2.0 Licensed as described in the file LICENSE
 */
#include <esp_log.h>
#include "app_config.h"
#include "esp_http_client.h"
#include "cJSON.h"
#include "http.h"
#include "sensor_data.h"
#include "git_version.h" // Include the auto-generated header
#include <time.h>
#include <string.h>

#include "battery_monitor.h"
#include "wifi_monitor.h"
#include "esp_sleep.h"

#define TAG "HTTP"
#define MAX_READINGS_PER_CHUNK 50  // Send at most 50 readings per HTTP request

extern const uint8_t _binary_server_cert_pem_start[];
extern const uint8_t _binary_server_cert_pem_end[];

/**
 * @brief Add boot/wake reason prefix to status messages
 */
static void add_boot_reason_to_status(char* status_msg, size_t buffer_size) {
    esp_sleep_wakeup_cause_t wakeup_cause = esp_sleep_get_wakeup_cause();
    char temp_msg[128];
    strncpy(temp_msg, status_msg, sizeof(temp_msg) - 1);
    temp_msg[sizeof(temp_msg) - 1] = '\0';

    switch (wakeup_cause) {
        case ESP_SLEEP_WAKEUP_UNDEFINED:
            snprintf(status_msg, buffer_size, "[boot] %s", temp_msg);
            break;
        case ESP_SLEEP_WAKEUP_TIMER:
            snprintf(status_msg, buffer_size, "[wake] %s", temp_msg);
            break;
        default:
            // Keep original message for other wake reasons
            break;
    }
}

/**
 * @brief HTTP event handler for logging. It is static as it's only used within this file.
 */
static esp_err_t http_event_handler(esp_http_client_event_t *evt)
{
    switch(evt->event_id) {
    case HTTP_EVENT_ERROR:
        ESP_LOGD(TAG, "HTTP_EVENT_ERROR");
        break;
    case HTTP_EVENT_ON_CONNECTED:
        ESP_LOGD(TAG, "HTTP_EVENT_ON_CONNECTED");
        break;
    case HTTP_EVENT_HEADER_SENT:
        ESP_LOGD(TAG, "HTTP_EVENT_HEADER_SENT");
        break;
    case HTTP_EVENT_ON_HEADER:
        ESP_LOGD(TAG, "HTTP_EVENT_ON_HEADER, key=%s, value=%s", evt->header_key, evt->header_value);
        break;
    case HTTP_EVENT_ON_DATA:
        ESP_LOGD(TAG, "HTTP_EVENT_ON_DATA, len=%d", evt->data_len);
        break;
    case HTTP_EVENT_ON_FINISH:
        ESP_LOGD(TAG, "HTTP_EVENT_ON_FINISH");
        break;
    case HTTP_EVENT_DISCONNECTED:
        ESP_LOGD(TAG, "HTTP_EVENT_DISCONNECTED");
        break;
    case HTTP_EVENT_REDIRECT:
        ESP_LOGD(TAG, "HTTP_EVENT_REDIRECT");
        break;
    }
    return ESP_OK;
}

/**
 * @brief Private helper to send a pre-formatted JSON payload with proper error handling.
 * This function handles the entire HTTP client lifecycle and returns meaningful error codes.
 */
static esp_err_t _send_json_payload_with_status(const char* json_payload, const char* bearer_token) {
    esp_http_client_handle_t client = NULL;
    esp_err_t err = ESP_FAIL;

    if (json_payload == NULL || bearer_token == NULL) {
        ESP_LOGE(TAG, "Invalid parameters: json_payload or bearer_token is NULL");
        return ESP_ERR_INVALID_ARG;
    }

    const char *cert_pem = (const char *)_binary_server_cert_pem_start;

    esp_http_client_config_t config = {
        .url = CONFIG_API_URL,
        .event_handler = http_event_handler,
        .cert_pem = cert_pem,
        .timeout_ms = 30000, // 30 second timeout
    };

    client = esp_http_client_init(&config);
    if (client == NULL) {
        ESP_LOGE(TAG, "Failed to initialize HTTP client");
        return ESP_ERR_NO_MEM;
    }

    // Set HTTP method and headers
    err = esp_http_client_set_method(client, HTTP_METHOD_POST);
    if (err != ESP_OK) {
        ESP_LOGE(TAG, "Failed to set HTTP method: %s", esp_err_to_name(err));
        esp_http_client_cleanup(client);
        return err;
    }

    err = esp_http_client_set_header(client, "Content-Type", "application/json");
    if (err != ESP_OK) {
        ESP_LOGE(TAG, "Failed to set Content-Type header: %s", esp_err_to_name(err));
        esp_http_client_cleanup(client);
        return err;
    }

    char auth_header[128];
    snprintf(auth_header, sizeof(auth_header), "Bearer %s", bearer_token);
    err = esp_http_client_set_header(client, "Authorization", auth_header);
    if (err != ESP_OK) {
        ESP_LOGE(TAG, "Failed to set Authorization header: %s", esp_err_to_name(err));
        esp_http_client_cleanup(client);
        return err;
    }

    err = esp_http_client_set_post_field(client, json_payload, strlen(json_payload));
    if (err != ESP_OK) {
        ESP_LOGE(TAG, "Failed to set POST data: %s", esp_err_to_name(err));
        esp_http_client_cleanup(client);
        return err;
    }

    // Perform the HTTP request
    err = esp_http_client_perform(client);
    if (err == ESP_OK) {
        int status_code = esp_http_client_get_status_code(client);
        int content_length = esp_http_client_get_content_length(client);

        ESP_LOGI(TAG, "HTTPS POST request completed, status = %d, content_length = %d",
                 status_code, content_length);

        // Check if HTTP status indicates success (2xx range)
        if (status_code >= 200 && status_code < 300) {
            ESP_LOGI(TAG, "HTTP request successful with status %d", status_code);
            err = ESP_OK;
        } else {
            ESP_LOGE(TAG, "HTTP request failed with status %d", status_code);
            // Map HTTP status codes to ESP error codes
            switch (status_code) {
                case 400:
                    err = ESP_ERR_INVALID_ARG;
                    break;
                case 401:
                case 403:
                    err = ESP_ERR_NOT_ALLOWED;
                    break;
                case 404:
                    err = ESP_ERR_NOT_FOUND;
                    break;
                case 500:
                case 502:
                case 503:
                    err = ESP_ERR_INVALID_RESPONSE;
                    break;
                default:
                    err = ESP_FAIL;
                    break;
            }
        }
    } else {
        ESP_LOGE(TAG, "HTTPS POST request failed: %s", esp_err_to_name(err));
    }

    esp_http_client_cleanup(client);
    return err;
}

/**
 * @brief Internal function to send a single chunk of sensor data
 */
static esp_err_t _send_sensor_data_chunk(const sensor_reading_t* readings, int count, const char* sensor_id, const char* bearer_token) {
    if (readings == NULL || count <= 0 || sensor_id == NULL || bearer_token == NULL) {
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
            // Don't continue adding more objects if we're out of memory
            break;
        }

        char timestamp_str[32];
        strftime(timestamp_str, sizeof(timestamp_str), "%Y-%m-%dT%H:%M:%SZ", gmtime(&readings[i].timestamp));

        if (cJSON_AddNumberToObject(sensor_object, "light_intensity", readings[i].lux) == NULL ||
            cJSON_AddStringToObject(sensor_object, "sensor_id", sensor_id) == NULL ||
            cJSON_AddStringToObject(sensor_object, "timestamp", timestamp_str) == NULL ||
            cJSON_AddStringToObject(sensor_object, "sensor_set_id", CONFIG_SENSOR_SET) == NULL) {
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

    // Log heap usage after building JSON
    size_t free_heap_after_json = esp_get_free_heap_size();
    ESP_LOGD(TAG, "Free heap after JSON creation: %zu bytes (used: %zu)",
             free_heap_after_json, free_heap_before - free_heap_after_json);

    json_payload = cJSON_Print(root_array);
    if (json_payload == NULL) {
        ESP_LOGE(TAG, "Failed to print JSON payload (likely out of memory)");
        size_t free_heap_now = esp_get_free_heap_size();
        ESP_LOGE(TAG, "Current free heap: %zu bytes", free_heap_now);
        result = ESP_ERR_NO_MEM;
        goto cleanup;
    }

    ESP_LOGD(TAG, "JSON Payload for chunk: %s", json_payload);
    ESP_LOGI(TAG, "Sending JSON chunk with %d records.", valid_readings);

    result = _send_json_payload_with_status(json_payload, bearer_token);

cleanup:
    if (root_array) cJSON_Delete(root_array);
    if (json_payload) free(json_payload);

    // Log final heap status
    size_t free_heap_final = esp_get_free_heap_size();
    ESP_LOGD(TAG, "Free heap after cleanup: %zu bytes", free_heap_final);

    return result;
}

// New functions with proper error handling and chunked sending
esp_err_t send_sensor_data_with_status(const sensor_reading_t* readings, int count, const char* sensor_id, const char* bearer_token) {
    if (readings == NULL || count <= 0 || sensor_id == NULL || bearer_token == NULL) {
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

        esp_err_t result = _send_sensor_data_chunk(&readings[sent_count], chunk_size, sensor_id, bearer_token);

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

esp_err_t send_status_update(const char* status_message, const char* sensor_id, const char* bearer_token) {
    if (status_message == NULL || sensor_id == NULL || bearer_token == NULL) {
        ESP_LOGE(TAG, "Invalid parameters for status update");
        return ESP_ERR_INVALID_ARG;
    }

    // Create a copy of the status message with boot reason prefix
    char enhanced_status[256];
    strncpy(enhanced_status, status_message, sizeof(enhanced_status) - 1);
    enhanced_status[sizeof(enhanced_status) - 1] = '\0';
    add_boot_reason_to_status(enhanced_status, sizeof(enhanced_status));

    cJSON *root_array = NULL;
    char *json_payload = NULL;
    esp_err_t result = ESP_FAIL;

    root_array = cJSON_CreateArray();
    if (root_array == NULL) {
        ESP_LOGE(TAG, "Failed to create JSON array for status update");
        return ESP_ERR_NO_MEM;
    }

    cJSON *status_object = cJSON_CreateObject();
    if (status_object == NULL) {
        ESP_LOGE(TAG, "Failed to create status object");
        result = ESP_ERR_NO_MEM;
        goto cleanup;
    }

    time_t now;
    time(&now);
    char timestamp_str[32];
    strftime(timestamp_str, sizeof(timestamp_str), "%Y-%m-%dT%H:%M:%SZ", gmtime(&now));

    cJSON_AddStringToObject(status_object, "sensor_id", sensor_id);
    cJSON_AddStringToObject(status_object, "timestamp", timestamp_str);
    cJSON_AddStringToObject(status_object, "sensor_set_id", CONFIG_SENSOR_SET);
    cJSON_AddStringToObject(status_object, "status", enhanced_status);

    // Add the Git commit info to the status payload
    cJSON_AddStringToObject(status_object, "commit_sha", GIT_COMMIT_SHA);
    cJSON_AddStringToObject(status_object, "commit_timestamp", GIT_COMMIT_TIMESTAMP);

    cJSON_AddItemToArray(root_array, status_object);

    json_payload = cJSON_Print(root_array);
    if (json_payload == NULL) {
        ESP_LOGE(TAG, "Failed to print JSON payload for status update");
        result = ESP_ERR_NO_MEM;
        goto cleanup;
    }

    ESP_LOGI(TAG, "Sending status update: '%s'", enhanced_status);
    ESP_LOGD(TAG, "Status JSON Payload: %s", json_payload);

    result = _send_json_payload_with_status(json_payload, bearer_token);

cleanup:
    if (root_array) cJSON_Delete(root_array);
    if (json_payload) free(json_payload);
    return result;
}

esp_err_t send_battery_status_update(const char* sensor_id, const char* bearer_token) {
    if (sensor_id == NULL || bearer_token == NULL) {
        ESP_LOGE(TAG, "Invalid parameters for battery status update");
        return ESP_ERR_INVALID_ARG;
    }

    float voltage;
    int percentage;
    int8_t rssi;

    // Get battery data - return error if no battery
    esp_err_t battery_err = battery_get_api_data(&voltage, &percentage);
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
    strncpy(enhanced_status, base_status, sizeof(enhanced_status) - 1);
    enhanced_status[sizeof(enhanced_status) - 1] = '\0';
    add_boot_reason_to_status(enhanced_status, sizeof(enhanced_status));

    cJSON_AddStringToObject(status_object, "sensor_id", sensor_id);
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

    result = _send_json_payload_with_status(json_payload, bearer_token);

cleanup:
    if (root_array) cJSON_Delete(root_array);
    if (json_payload) free(json_payload);
    return result;
}