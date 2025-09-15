/**
* @file http.c
 *
 * HTTP client for sending sensor data with HTTP (non-secure) connections.
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
#include "git_version.h"
#include <time.h>
#include <string.h>

#define TAG "HTTP"
#define MAX_READINGS_PER_CHUNK 50

static esp_err_t http_event_handler(esp_http_client_event_t *evt)
{
    switch(evt->event_id) {
    case HTTP_EVENT_ERROR:
        ESP_LOGD(TAG, "HTTP_EVENT_ERROR");
        break;
    case HTTP_EVENT_ON_CONNECTED:
        ESP_LOGD(TAG, "HTTP_EVENT_ON_CONNECTED");
        break;
    case HTTP_EVENT_DISCONNECTED:
        ESP_LOGD(TAG, "HTTP_EVENT_DISCONNECTED");
        break;
    default:
        break;
    }
    return ESP_OK;
}

static esp_err_t send_json_payload(const char* json_payload, const char* bearer_token) {
    if (json_payload == NULL || bearer_token == NULL) {
        return ESP_ERR_INVALID_ARG;
    }

    esp_http_client_config_t config = {
        .url = CONFIG_API_URL,
        .event_handler = http_event_handler,
        .timeout_ms = 30000,
    };

    esp_http_client_handle_t client = esp_http_client_init(&config);
    if (client == NULL) {
        return ESP_ERR_NO_MEM;
    }

    esp_err_t err = esp_http_client_set_method(client, HTTP_METHOD_POST);
    if (err != ESP_OK) goto cleanup;

    err = esp_http_client_set_header(client, "Content-Type", "application/json");
    if (err != ESP_OK) goto cleanup;

    char auth_header[128];
    snprintf(auth_header, sizeof(auth_header), "Bearer %s", bearer_token);
    err = esp_http_client_set_header(client, "Authorization", auth_header);
    if (err != ESP_OK) goto cleanup;

    err = esp_http_client_set_post_field(client, json_payload, strlen(json_payload));
    if (err != ESP_OK) goto cleanup;

    err = esp_http_client_perform(client);
    if (err == ESP_OK) {
        int status_code = esp_http_client_get_status_code(client);
        if (status_code >= 200 && status_code < 300) {
            ESP_LOGI(TAG, "HTTP request successful, status %d", status_code);
        } else {
            ESP_LOGE(TAG, "HTTP request failed, status %d", status_code);
            err = ESP_FAIL;
        }
    }

cleanup:
    esp_http_client_cleanup(client);
    return err;
}

static esp_err_t send_sensor_data_chunk(const sensor_reading_t* readings, int count,
                                       const char* sensor_id, const char* bearer_token) {
    cJSON *root_array = cJSON_CreateArray();
    if (root_array == NULL) {
        return ESP_ERR_NO_MEM;
    }

    for (int i = 0; i < count; i++) {
        cJSON *obj = cJSON_CreateObject();
        if (obj == NULL) break;

        char timestamp_str[32];
        strftime(timestamp_str, sizeof(timestamp_str), "%Y-%m-%dT%H:%M:%SZ", gmtime(&readings[i].timestamp));

        cJSON_AddNumberToObject(obj, "light_intensity", readings[i].lux);
        cJSON_AddStringToObject(obj, "sensor_id", sensor_id);
        cJSON_AddStringToObject(obj, "timestamp", timestamp_str);
        cJSON_AddStringToObject(obj, "sensor_set_id", CONFIG_SENSOR_SET);
        cJSON_AddItemToArray(root_array, obj);
    }

    char *json_payload = cJSON_Print(root_array);
    cJSON_Delete(root_array);

    if (json_payload == NULL) {
        return ESP_ERR_NO_MEM;
    }

    esp_err_t result = send_json_payload(json_payload, bearer_token);
    free(json_payload);
    return result;
}

esp_err_t send_sensor_data_with_status(const sensor_reading_t* readings, int count,
                                     const char* sensor_id, const char* bearer_token) {
    if (readings == NULL || count <= 0) {
        return ESP_ERR_INVALID_ARG;
    }

    int sent_count = 0;
    while (sent_count < count) {
        int chunk_size = (count - sent_count > MAX_READINGS_PER_CHUNK) ?
                        MAX_READINGS_PER_CHUNK : (count - sent_count);

        esp_err_t result = send_sensor_data_chunk(&readings[sent_count], chunk_size, sensor_id, bearer_token);
        if (result != ESP_OK) {
            return result;
        }

        sent_count += chunk_size;
        if (sent_count < count) {
            vTaskDelay(pdMS_TO_TICKS(1000));
        }
    }

    return ESP_OK;
}

esp_err_t send_status_update_with_status(const char* status_message, const char* sensor_id,
                                       const char* bearer_token) {
    cJSON *root_array = cJSON_CreateArray();
    if (root_array == NULL) {
        return ESP_ERR_NO_MEM;
    }

    cJSON *obj = cJSON_CreateObject();
    if (obj == NULL) {
        cJSON_Delete(root_array);
        return ESP_ERR_NO_MEM;
    }

    time_t now;
    time(&now);
    char timestamp_str[32];
    strftime(timestamp_str, sizeof(timestamp_str), "%Y-%m-%dT%H:%M:%SZ", gmtime(&now));

    cJSON_AddStringToObject(obj, "sensor_id", sensor_id);
    cJSON_AddStringToObject(obj, "timestamp", timestamp_str);
    cJSON_AddStringToObject(obj, "sensor_set_id", CONFIG_SENSOR_SET);
    cJSON_AddStringToObject(obj, "status", status_message);
    cJSON_AddStringToObject(obj, "commit_sha", GIT_COMMIT_SHA);
    cJSON_AddStringToObject(obj, "commit_timestamp", GIT_COMMIT_TIMESTAMP);
    cJSON_AddItemToArray(root_array, obj);

    char *json_payload = cJSON_Print(root_array);
    cJSON_Delete(root_array);

    if (json_payload == NULL) {
        return ESP_ERR_NO_MEM;
    }

    esp_err_t result = send_json_payload(json_payload, bearer_token);
    free(json_payload);
    return result;
}

esp_err_t send_device_status_with_status(const device_status_t* status,
                                        const char* sensor_id, const char* bearer_token) {
    if (status == NULL || sensor_id == NULL || bearer_token == NULL) {
        return ESP_ERR_INVALID_ARG;
    }

    cJSON *root_array = cJSON_CreateArray();
    if (root_array == NULL) {
        return ESP_ERR_NO_MEM;
    }

    cJSON *obj = cJSON_CreateObject();
    if (obj == NULL) {
        cJSON_Delete(root_array);
        return ESP_ERR_NO_MEM;
    }

    char timestamp_str[32];
    strftime(timestamp_str, sizeof(timestamp_str), "%Y-%m-%dT%H:%M:%SZ", gmtime(&status->timestamp));

    cJSON_AddStringToObject(obj, "sensor_id", sensor_id);
    cJSON_AddStringToObject(obj, "timestamp", timestamp_str);
    cJSON_AddStringToObject(obj, "sensor_set_id", CONFIG_SENSOR_SET);

    // Add battery voltage if available (> 0)
    if (status->battery_volts > 0.0) {
        cJSON_AddNumberToObject(obj, "battery_volts", status->battery_volts);
    }

    // Add WiFi signal strength if available (!= 0)
    if (status->wifi_dbm != 0) {
        cJSON_AddNumberToObject(obj, "wifi_dbm", status->wifi_dbm);
    }

    cJSON_AddItemToArray(root_array, obj);

    char *json_payload = cJSON_Print(root_array);
    cJSON_Delete(root_array);

    if (json_payload == NULL) {
        return ESP_ERR_NO_MEM;
    }

    esp_err_t result = send_json_payload(json_payload, bearer_token);
    free(json_payload);
    return result;
}