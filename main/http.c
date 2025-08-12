/**
 * @file http.c
 *
 * HTTP client for sending sensor data.
 *
 * Copyright (c) 2025 Caden Howell (cadenhowell@gmail.com)
 *
 * Developed with assistance from ChatGPT 4o (2025) and Google Gemini 2.5 Pro (2025).
 *
 * Apache 2.0 Licensed as described in the file LICENSE
 */
#include <esp_log.h>
#include "app_config.h"
#include "esp_http_client.h"
#include "cJSON.h"
#include "http.h"
#include "sensor_data.h"
#include <time.h>
#include <string.h>

#define TAG "HTTP"

extern const uint8_t _binary_server_cert_pem_start[];
extern const uint8_t _binary_server_cert_pem_end[];

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
 * @brief Private helper to send a pre-formatted JSON payload.
 * This function handles the entire HTTP client lifecycle.
 * It is declared with __attribute__((weak)) so it can be easily mocked in tests.
 * @return ESP_OK on success, ESP_FAIL on server error, or other error codes.
 */
esp_err_t __attribute__((weak)) _send_json_payload(const char* json_payload, const char* bearer_token) {
    esp_http_client_handle_t client = NULL;
    esp_err_t err = ESP_FAIL;

    const char *cert_pem = (const char *)_binary_server_cert_pem_start;

    esp_http_client_config_t config = {
        .url = CONFIG_API_URL,
        .event_handler = http_event_handler,
        .cert_pem = cert_pem,
    };
    client = esp_http_client_init(&config);
    if (client == NULL) {
        ESP_LOGE(TAG, "Failed to initialize HTTP client");
        return ESP_FAIL;
    }

    esp_http_client_set_method(client, HTTP_METHOD_POST);
    esp_http_client_set_header(client, "Content-Type", "application/json");
    char auth_header[128];
    snprintf(auth_header, sizeof(auth_header), "Bearer %s", bearer_token);
    esp_http_client_set_header(client, "Authorization", auth_header);
    esp_http_client_set_post_field(client, json_payload, strlen(json_payload));

    err = esp_http_client_perform(client);
    if (err == ESP_OK) {
        int status_code = esp_http_client_get_status_code(client);
        ESP_LOGI(TAG, "HTTPS POST request sent, status = %d", status_code);
        // Consider any 2xx status code as success
        if (status_code < 200 || status_code >= 300) {
            ESP_LOGE(TAG, "Server returned non-success status code: %d", status_code);
            err = ESP_FAIL; // Treat non-2xx as a failure for retry purposes
        }
    } else {
        ESP_LOGE(TAG, "HTTPS POST request failed: %s", esp_err_to_name(err));
    }

    esp_http_client_cleanup(client);
    return err;
}


// Function to send sensor data
esp_err_t send_sensor_data(const sensor_reading_t* readings, int count, const char* sensor_id, const char* bearer_token) {
    cJSON *root_array = NULL;
    char *json_payload = NULL;
    esp_err_t err = ESP_FAIL;

    root_array = cJSON_CreateArray();
    if (root_array == NULL) {
        ESP_LOGE(TAG, "Failed to create JSON array");
        goto cleanup;
    }

    for (int i = 0; i < count; i++) {
        cJSON *sensor_object = cJSON_CreateObject();
        if (sensor_object == NULL) {
            ESP_LOGE(TAG, "Failed to create sensor object for reading #%d", i);
            continue;
        }

        char timestamp_str[32];
        strftime(timestamp_str, sizeof(timestamp_str), "%Y-%m-%dT%H:%M:%SZ", gmtime(&readings[i].timestamp));

        cJSON_AddNumberToObject(sensor_object, "light_intensity", readings[i].lux);
        cJSON_AddStringToObject(sensor_object, "sensor_id", sensor_id);
        cJSON_AddStringToObject(sensor_object, "timestamp", timestamp_str);
        cJSON_AddStringToObject(sensor_object, "sensor_set_id", CONFIG_SENSOR_SET);

        cJSON_AddItemToArray(root_array, sensor_object);
    }

    if (cJSON_GetArraySize(root_array) == 0) {
        ESP_LOGW(TAG, "No valid readings to send. Aborting HTTP POST.");
        err = ESP_OK; // Not a failure, just nothing to do
        goto cleanup;
    }

    json_payload = cJSON_Print(root_array);
    if (json_payload == NULL) {
        ESP_LOGE(TAG, "Failed to print JSON payload");
        goto cleanup;
    }

    ESP_LOGD(TAG, "JSON Payload: %s", json_payload);
    ESP_LOGI(TAG, "Sending JSON with %d records.", cJSON_GetArraySize(root_array));

    err = _send_json_payload(json_payload, bearer_token);

cleanup:
    if (root_array) cJSON_Delete(root_array);
    if (json_payload) free(json_payload);
    return err;
}

void send_status_update(const char* status_message, const char* sensor_id, const char* bearer_token) {
    cJSON *root_array = NULL;
    cJSON *status_object = NULL;
    char *json_payload = NULL;

    // Validate input parameters
    if (!status_message || !sensor_id || !bearer_token) {
        ESP_LOGE(TAG, "Invalid parameters for status update");
        return;
    }

    root_array = cJSON_CreateArray();
    if (root_array == NULL) {
        ESP_LOGE(TAG, "Failed to create JSON array for status update");
        goto cleanup;
    }

    status_object = cJSON_CreateObject();
    if (status_object == NULL) {
        ESP_LOGE(TAG, "Failed to create status object");
        goto cleanup;
    }

    time_t now;
    time(&now);
    char timestamp_str[32];
    strftime(timestamp_str, sizeof(timestamp_str), "%Y-%m-%dT%H:%M:%SZ", gmtime(&now));

    // Add all fields and check for failures
    if (!cJSON_AddStringToObject(status_object, "sensor_id", sensor_id) ||
        !cJSON_AddStringToObject(status_object, "timestamp", timestamp_str) ||
        !cJSON_AddStringToObject(status_object, "sensor_set_id", CONFIG_SENSOR_SET) ||
        !cJSON_AddStringToObject(status_object, "status", status_message)) {
        ESP_LOGE(TAG, "Failed to add fields to status object");
        goto cleanup;
    }

    // Add to array
    cJSON_AddItemToArray(root_array, status_object);
    status_object = NULL; // Array now owns this object

    json_payload = cJSON_Print(root_array);
    if (json_payload == NULL) {
        ESP_LOGE(TAG, "Failed to print JSON payload for status update");
        goto cleanup;
    }

    ESP_LOGI(TAG, "Sending status update: '%s'", status_message);
    ESP_LOGD(TAG, "Status JSON Payload: %s", json_payload);

    _send_json_payload(json_payload, bearer_token);

cleanup:
    // Clean up in reverse order
    if (json_payload) {
        free(json_payload);
        json_payload = NULL;
    }
    if (status_object) {
        cJSON_Delete(status_object);
        status_object = NULL;
    }
    if (root_array) {
        cJSON_Delete(root_array);
        root_array = NULL;
    }
}