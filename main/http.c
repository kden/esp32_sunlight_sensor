/**
 * @file http.c
 *
 * HTTP client for sending sensor spiffs.
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
 * @brief HTTP event handler for logging. It is static to this file.
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

// Function to send sensor data
void send_sensor_data(const sensor_reading_t* readings, int count, const char* sensor_id, const char* bearer_token) {

    const char *cert_pem = (const char *)_binary_server_cert_pem_start;

    // Create the root JSON array and add the sensor object to it
    cJSON *root_array = cJSON_CreateArray();
    if (root_array == NULL) {
        ESP_LOGE(TAG, "Failed to create JSON array");
        return;
    }

    for (int i = 0; i < count; i++) {
        cJSON *sensor_object = cJSON_CreateObject();
        if (sensor_object == NULL) {
            ESP_LOGE(TAG, "Failed to create sensor object for reading #%d", i);
            continue; // Try to send the rest
        }

        // Format timestamp to ISO 8601 in UTC
        char timestamp_str[32];
        strftime(timestamp_str, sizeof(timestamp_str), "%Y-%m-%dT%H:%M:%SZ", gmtime(&readings[i].timestamp));

        // Populate the sensor data object
        cJSON_AddNumberToObject(sensor_object, "light_intensity", readings[i].lux);
        cJSON_AddStringToObject(sensor_object, "sensor_id", sensor_id);
        cJSON_AddStringToObject(sensor_object, "timestamp", timestamp_str);
        cJSON_AddStringToObject(sensor_object, "sensor_set_id", CONFIG_SENSOR_SET);

        cJSON_AddItemToArray(root_array, sensor_object);
    }

    if (cJSON_GetArraySize(root_array) == 0) {
        ESP_LOGW(TAG, "No valid readings to send. Aborting HTTP POST.");
        cJSON_Delete(root_array);
        return;
    }

    // Generate the JSON payload string from the root array
    const char *json_payload = cJSON_Print(root_array);
    if (json_payload == NULL) {
        ESP_LOGE(TAG, "Failed to print JSON payload");
        cJSON_Delete(root_array);
        return;
    }
    ESP_LOGI(TAG, "Sending JSON with %d records.", cJSON_GetArraySize(root_array));


    // Configure HTTP client
    esp_http_client_config_t config = {
        .url = CONFIG_API_URL,
        .event_handler = http_event_handler,
        .cert_pem = cert_pem,
    };
    esp_http_client_handle_t client = esp_http_client_init(&config);

    // Set headers
    esp_http_client_set_method(client, HTTP_METHOD_POST);
    esp_http_client_set_header(client, "Content-Type", "application/json");
    char auth_header[128];
    snprintf(auth_header, sizeof(auth_header), "Bearer %s", bearer_token);
    esp_http_client_set_header(client, "Authorization", auth_header);

    // Set POST spiffs
    esp_http_client_set_post_field(client, json_payload, strlen(json_payload));

    // Perform the request
    esp_err_t err = esp_http_client_perform(client);
    if (err == ESP_OK) {
        ESP_LOGI(TAG, "HTTPS POST request sent successfully, status = %d", esp_http_client_get_status_code(client));
    } else {
        ESP_LOGE(TAG, "HTTPS POST request failed: %s", esp_err_to_name(err));
    }

    // Cleanup
    esp_http_client_cleanup(client);
    cJSON_Delete(root_array);
    free((void*)json_payload);
}