/**
* @file wifi_monitor.c
 *
 * WiFi signal strength monitoring functions.
 *
 * Copyright (c) 2025 Caden Howell (cadenhowell@gmail.com)
 *
 * Developed with assistance from Claude Sonnet 4 (2025).
 *
 * Apache 2.0 Licensed as described in the file LICENSE
 */

#include "wifi_monitor.h"
#include "wifi.h"
#include "esp_wifi.h"
#include "esp_log.h"

#define TAG "WIFI_MONITOR"

esp_err_t wifi_get_rssi(int8_t *rssi) {
    if (rssi == NULL) {
        return ESP_ERR_INVALID_ARG;
    }

    if (!wifi_is_connected()) {
        ESP_LOGD(TAG, "WiFi not connected");
        return ESP_ERR_WIFI_NOT_CONNECT;
    }

    wifi_ap_record_t ap_info;
    esp_err_t err = esp_wifi_sta_get_ap_info(&ap_info);
    if (err != ESP_OK) {
        ESP_LOGE(TAG, "Failed to get AP info: %s", esp_err_to_name(err));
        return err;
    }

    *rssi = ap_info.rssi;
    ESP_LOGD(TAG, "WiFi RSSI: %d dBm", *rssi);
    return ESP_OK;
}

esp_err_t wifi_get_signal_quality(uint8_t *quality) {
    if (quality == NULL) {
        return ESP_ERR_INVALID_ARG;
    }

    int8_t rssi;
    esp_err_t err = wifi_get_rssi(&rssi);
    if (err != ESP_OK) {
        return err;
    }

    // Convert RSSI to quality percentage
    // RSSI ranges roughly from -30 (excellent) to -90 (unusable)
    // -30 dBm = 100%, -50 dBm = ~70%, -70 dBm = ~30%, -90 dBm = 0%
    if (rssi >= -30) {
        *quality = 100;
    } else if (rssi <= -90) {
        *quality = 0;
    } else {
        // Linear interpolation between -30 and -90 dBm
        *quality = (uint8_t)(100 - ((30 + rssi) * 100 / 60));
    }

    return ESP_OK;
}

esp_err_t wifi_get_status_string(char *buffer, size_t buffer_size) {
    if (buffer == NULL || buffer_size == 0) {
        return ESP_ERR_INVALID_ARG;
    }

    if (!wifi_is_connected()) {
        snprintf(buffer, buffer_size, "wifi disconnected");
        return ESP_OK;
    }

    int8_t rssi;
    uint8_t quality;
    esp_err_t err = wifi_get_rssi(&rssi);
    if (err != ESP_OK) {
        snprintf(buffer, buffer_size, "wifi signal read error");
        return err;
    }

    err = wifi_get_signal_quality(&quality);
    if (err != ESP_OK) {
        snprintf(buffer, buffer_size, "wifi signal calc error");
        return err;
    }

    // Get connected AP name
    wifi_ap_record_t ap_info;
    err = esp_wifi_sta_get_ap_info(&ap_info);
    if (err == ESP_OK) {
        snprintf(buffer, buffer_size, "wifi %s %ddBm %d%%",
                 (char*)ap_info.ssid, rssi, quality);
    } else {
        snprintf(buffer, buffer_size, "wifi %ddBm %d%%", rssi, quality);
    }

    return ESP_OK;
}

esp_err_t wifi_get_ip_address(char *ip_str, size_t buffer_size) {
    if (ip_str == NULL || buffer_size == 0) {
        return ESP_ERR_INVALID_ARG;
    }

    if (!wifi_is_connected()) {
        snprintf(ip_str, buffer_size, "not connected");
        return ESP_ERR_WIFI_NOT_CONNECT;
    }

    esp_netif_t *netif = esp_netif_get_handle_from_ifkey("WIFI_STA_DEF");
    if (netif == NULL) {
        snprintf(ip_str, buffer_size, "no interface");
        return ESP_FAIL;
    }

    esp_netif_ip_info_t ip_info;
    esp_err_t err = esp_netif_get_ip_info(netif, &ip_info);
    if (err != ESP_OK) {
        snprintf(ip_str, buffer_size, "ip error");
        return err;
    }

    snprintf(ip_str, buffer_size, IPSTR, IP2STR(&ip_info.ip));
    return ESP_OK;
}