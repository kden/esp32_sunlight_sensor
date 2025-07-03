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
#include <time.h>
#include <string.h>

#define TAG "HTTP"

extern const uint8_t _binary_server_cert_pem_start[];
extern const uint8_t _binary_server_cert_pem_end[];


// Forward declaration for safety
esp_err_t _http_event_handler(esp_http_client_event_t *evt);

// Function to send sensor data
void send_sensor_data(float lux, const char* sensor_id, const char* bearer_token) {

    const char *cert_pem = (const char *)_binary_server_cert_pem_start;

    cJSON *root = cJSON_CreateObject();
    if (root == NULL) {
        ESP_LOGE(TAG, "Failed to create JSON object");
        return;
    }

    // Get current time
    time_t now;
    time(&now);
    char timestamp[32];
    // Format timestamp to ISO 8601 in UTC
    strftime(timestamp, sizeof(timestamp), "%Y-%m-%dT%H:%M:%SZ", gmtime(&now));

    // Create JSON payload
    cJSON_AddNumberToObject(root, "light_intensity", lux);
    cJSON_AddStringToObject(root, "sensor_id", sensor_id);
    cJSON_AddStringToObject(root, "timestamp", timestamp);

    const char *json_payload = cJSON_Print(root);
    ESP_LOGI(TAG, "Sending JSON: %s", json_payload);


    // Configure HTTP client
    esp_http_client_config_t config = {
        .url = CONFIG_API_URL,
        .event_handler = _http_event_handler,
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
    cJSON_Delete(root);
    free((void*)json_payload);
}

// HTTP event handler for logging
esp_err_t _http_event_handler(esp_http_client_event_t *evt)
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