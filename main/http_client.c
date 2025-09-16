/**
* @file http_client.c
 *
 * Pure HTTP client operations.
 *
 * Copyright (c) 2025 Caden Howell (cadenhowell@gmail.com)
 *
 * Developed with assistance from ChatGPT 4o (2025), Google Gemini 2.5 Pro (2025) and Claude Sonnet 4 (2025).
 *
 * Apache 2.0 Licensed as described in the file LICENSE
 */

#include "http_client.h"
#include "app_config.h"
#include "esp_http_client.h"
#include "esp_log.h"
#include <string.h>

#define TAG "HTTP_CLIENT"

extern const uint8_t _binary_server_cert_pem_start[];
extern const uint8_t _binary_server_cert_pem_end[];

/**
 * @brief HTTP event handler for logging
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

esp_err_t http_send_json_payload(const char* json_payload, const char* bearer_token) {
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