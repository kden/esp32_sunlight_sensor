/**
* @file wifi_manager.c
 *
 * Complete WiFi management including connection and monitoring.
 *
 * Copyright (c) 2025 Caden Howell (cadenhowell@gmail.com)
 *
 * Developed with assistance from ChatGPT 4o (2025), Google Gemini 2.5 Pro (2025) and Claude Sonnet 4 (2025).
 *
 * Apache 2.0 Licensed as described in the file LICENSE
 */

#include <esp_log.h>
#include "app_config.h"
#include <string.h>
#include "esp_wifi.h"
#include "esp_event.h"
#include "esp_netif.h"
#include "wifi_manager.h"
#include "sdkconfig.h"
#include <stdbool.h>

#define TAG "WIFI_MANAGER"

#define MAX_WIFI_NETWORKS 5
#define MAX_RECONNECT_RETRIES 3

static const char* wifi_reason_to_str(uint8_t reason)
{

#if CONFIG_LOG_MAXIMUM_LEVEL >= 3
    switch (reason)
    {
    case WIFI_REASON_UNSPECIFIED: return "wifi_reason_unspecified";
    case WIFI_REASON_AUTH_EXPIRE: return "wifi_reason_auth_expire";
    case WIFI_REASON_AUTH_LEAVE: return "wifi_reason_auth_leave";
    case WIFI_REASON_ASSOC_EXPIRE: return "wifi_reason_assoc_expire";
    case WIFI_REASON_ASSOC_TOOMANY: return "wifi_reason_assoc_toomany";
    case WIFI_REASON_NOT_AUTHED: return "wifi_reason_not_authed";
    case WIFI_REASON_NOT_ASSOCED: return "wifi_reason_not_assoced";
    case WIFI_REASON_ASSOC_LEAVE: return "wifi_reason_assoc_leave";
    case WIFI_REASON_ASSOC_NOT_AUTHED: return "wifi_reason_assoc_not_authed";
    case WIFI_REASON_DISASSOC_PWRCAP_BAD: return "wifi_reason_disassoc_pwrcap_bad";
    case WIFI_REASON_DISASSOC_SUPCHAN_BAD: return "wifi_reason_disassoc_supchan_bad";
    case WIFI_REASON_BSS_TRANSITION_DISASSOC: return "wifi_reason_bss_transition_disassoc";
    case WIFI_REASON_IE_INVALID: return "wifi_reason_ie_invalid";
    case WIFI_REASON_MIC_FAILURE: return "wifi_reason_mic_failure";
    case WIFI_REASON_4WAY_HANDSHAKE_TIMEOUT: return "wifi_reason_4way_handshake_timeout";
    case WIFI_REASON_GROUP_KEY_UPDATE_TIMEOUT: return "wifi_reason_group_key_update_timeout";
    case WIFI_REASON_IE_IN_4WAY_DIFFERS: return "wifi_reason_ie_in_4way_differs";
    case WIFI_REASON_GROUP_CIPHER_INVALID: return "wifi_reason_group_cipher_invalid";
    case WIFI_REASON_PAIRWISE_CIPHER_INVALID: return "wifi_reason_pairwise_cipher_invalid";
    case WIFI_REASON_AKMP_INVALID: return "wifi_reason_akmp_invalid";
    case WIFI_REASON_UNSUPP_RSN_IE_VERSION: return "wifi_reason_unsupp_rsn_ie_version";
    case WIFI_REASON_INVALID_RSN_IE_CAP: return "wifi_reason_invalid_rsn_ie_cap";
    case WIFI_REASON_802_1X_AUTH_FAILED: return "wifi_reason_802_1x_auth_failed";
    case WIFI_REASON_CIPHER_SUITE_REJECTED: return "wifi_reason_cipher_suite_rejected";
    case WIFI_REASON_TDLS_PEER_UNREACHABLE: return "wifi_reason_tdls_peer_unreachable";
    case WIFI_REASON_TDLS_UNSPECIFIED: return "wifi_reason_tdls_unspecified";
    case WIFI_REASON_SSP_REQUESTED_DISASSOC: return "wifi_reason_ssp_requested_disassoc";
    case WIFI_REASON_NO_SSP_ROAMING_AGREEMENT: return "wifi_reason_no_ssp_roaming_agreement";
    case WIFI_REASON_BAD_CIPHER_OR_AKM: return "wifi_reason_bad_cipher_or_akm";
    case WIFI_REASON_NOT_AUTHORIZED_THIS_LOCATION: return "wifi_reason_not_authorized_this_location";
    case WIFI_REASON_SERVICE_CHANGE_PERCLUDES_TS: return "wifi_reason_service_change_percludes_ts";
    case WIFI_REASON_UNSPECIFIED_QOS: return "wifi_reason_unspecified_qos";
    case WIFI_REASON_NOT_ENOUGH_BANDWIDTH: return "wifi_reason_not_enough_bandwidth";
    case WIFI_REASON_MISSING_ACKS: return "wifi_reason_missing_acks";
    case WIFI_REASON_EXCEEDED_TXOP: return "wifi_reason_exceeded_txop";
    case WIFI_REASON_STA_LEAVING: return "wifi_reason_sta_leaving";
    case WIFI_REASON_END_BA: return "wifi_reason_end_ba";
    case WIFI_REASON_UNKNOWN_BA: return "wifi_reason_unknown_ba";
    case WIFI_REASON_TIMEOUT: return "wifi_reason_timeout";
    case WIFI_REASON_PEER_INITIATED: return "wifi_reason_peer_initiated";
    case WIFI_REASON_AP_INITIATED: return "wifi_reason_ap_initiated";
    case WIFI_REASON_INVALID_FT_ACTION_FRAME_COUNT: return "wifi_reason_invalid_ft_action_frame_count";
    case WIFI_REASON_INVALID_PMKID: return "wifi_reason_invalid_pmkid";
    case WIFI_REASON_INVALID_MDE: return "wifi_reason_invalid_mde";
    case WIFI_REASON_INVALID_FTE: return "wifi_reason_invalid_fte";
    case WIFI_REASON_TRANSMISSION_LINK_ESTABLISH_FAILED: return "wifi_reason_transmission_link_establish_failed";
    case WIFI_REASON_ALTERATIVE_CHANNEL_OCCUPIED: return "wifi_reason_alterative_channel_occupied";
    case WIFI_REASON_BEACON_TIMEOUT: return "wifi_reason_beacon_timeout";
    case WIFI_REASON_NO_AP_FOUND: return "wifi_reason_no_ap_found";
    case WIFI_REASON_AUTH_FAIL: return "wifi_reason_auth_fail";
    case WIFI_REASON_ASSOC_FAIL: return "wifi_reason_assoc_fail";
    case WIFI_REASON_HANDSHAKE_TIMEOUT: return "wifi_reason_handshake_timeout";
    case WIFI_REASON_CONNECTION_FAIL: return "wifi_reason_connection_fail";
    case WIFI_REASON_AP_TSF_RESET: return "wifi_reason_ap_tsf_reset";
    case WIFI_REASON_ROAMING: return "wifi_reason_roaming";
    case WIFI_REASON_ASSOC_COMEBACK_TIME_TOO_LONG: return "wifi_reason_assoc_comeback_time_too_long";
    case WIFI_REASON_SA_QUERY_TIMEOUT: return "wifi_reason_sa_query_timeout";
    case WIFI_REASON_NO_AP_FOUND_W_COMPATIBLE_SECURITY: return "wifi_reason_no_ap_found_w_compatible_security";
    case WIFI_REASON_NO_AP_FOUND_IN_AUTHMODE_THRESHOLD: return "wifi_reason_no_ap_found_in_authmode_threshold";
    case WIFI_REASON_NO_AP_FOUND_IN_RSSI_THRESHOLD: return "wifi_reason_no_ap_found_in_rssi_threshold";
    default: return "Unknown reason";
    }
#else
    // For builds with low verbosity, return an empty string.
    // The ESP_LOGE call will still print the numeric reason code.
    return "";
#endif
}


typedef struct
{
    char ssid[32];
    char password[64];
} wifi_network_t;

static wifi_network_t s_wifi_networks[MAX_WIFI_NETWORKS];
static int s_num_networks = 0;
static int s_current_network_index = 0;
static int s_reconnect_retries = 0;
static bool s_is_connected = false;
static bool s_is_initialized = false;

// Forward declaration
static void try_to_connect(void);

static void wifi_event_handler(void* arg, esp_event_base_t event_base, int32_t event_id, void* event_data)
{
    if (event_base == WIFI_EVENT && event_id == WIFI_EVENT_STA_START)
    {
        try_to_connect(); // Start connection attempts when Wi-Fi stack is ready
    }
    else if (event_base == WIFI_EVENT && event_id == WIFI_EVENT_STA_DISCONNECTED)
    {
        wifi_event_sta_disconnected_t* event = (wifi_event_sta_disconnected_t*)event_data;
        ESP_LOGE(TAG, "Wi-Fi disconnected, reason: %d (%s)",
                 event->reason, wifi_reason_to_str(event->reason));
        s_is_connected = false;
        if (s_reconnect_retries < MAX_RECONNECT_RETRIES)
        {
            s_reconnect_retries++;
            ESP_LOGI(TAG, "Retrying connection to '%s' (attempt %d/%d)...",
                     s_wifi_networks[s_current_network_index].ssid, s_reconnect_retries, MAX_RECONNECT_RETRIES);
            esp_wifi_connect();
        }
        else
        {
            ESP_LOGI(TAG, "Failed to reconnect to '%s'. Trying next network.",
                     s_wifi_networks[s_current_network_index].ssid);
            s_current_network_index = (s_current_network_index + 1) % s_num_networks;
            s_reconnect_retries = 0;
            try_to_connect(); // Try the next network in the list
        }
    }
    else if (event_base == IP_EVENT && event_id == IP_EVENT_STA_GOT_IP)
    {
        ip_event_got_ip_t* event = (ip_event_got_ip_t*)event_data;
        ESP_LOGI(TAG, "Successfully connected to '%s' with IP: " IPSTR, s_wifi_networks[s_current_network_index].ssid,
                 IP2STR(&event->ip_info.ip));
        s_reconnect_retries = 0;
        s_is_connected = true;
    }
}

static void parse_wifi_credentials()
{
    char* wifi_credentials_copy = strdup(CONFIG_WIFI_CREDENTIALS);
    if (wifi_credentials_copy == NULL)
    {
        ESP_LOGE(TAG, "Failed to allocate memory for Wi-Fi credentials.");
        return;
    }

    char* network_str_saveptr;
    char* network_str = strtok_r(wifi_credentials_copy, ";", &network_str_saveptr);
    s_num_networks = 0;
    while (network_str != NULL && s_num_networks < MAX_WIFI_NETWORKS)
    {
        char* ssid_saveptr;
        char* ssid = strtok_r(network_str, ":", &ssid_saveptr);
        char* password = strtok_r(NULL, ":", &ssid_saveptr);
        if (ssid && password)
        {
            strncpy(s_wifi_networks[s_num_networks].ssid, ssid, sizeof(s_wifi_networks[s_num_networks].ssid) - 1);
            strncpy(s_wifi_networks[s_num_networks].password, password,
                    sizeof(s_wifi_networks[s_num_networks].password) - 1);
            s_num_networks++;
        }
        network_str = strtok_r(NULL, ";", &network_str_saveptr);
    }
    free(wifi_credentials_copy);
    ESP_LOGI(TAG, "Found %d Wi-Fi networks in credentials.", s_num_networks);
}

static void try_to_connect(void)
{
    if (s_num_networks == 0)
    {
        ESP_LOGE(TAG, "No Wi-Fi networks configured.");
        return;
    }

    ESP_LOGI(TAG, "Attempting to connect to network: %s", s_wifi_networks[s_current_network_index].ssid);

    wifi_config_t wifi_config = {
        .sta = {
            .threshold.authmode = WIFI_AUTH_WPA2_PSK,
        },
    };
    strncpy((char*)wifi_config.sta.ssid, s_wifi_networks[s_current_network_index].ssid, sizeof(wifi_config.sta.ssid));
    strncpy((char*)wifi_config.sta.password, s_wifi_networks[s_current_network_index].password,
            sizeof(wifi_config.sta.password));

    ESP_ERROR_CHECK(esp_wifi_set_config(WIFI_IF_STA, &wifi_config));
    esp_wifi_connect();
}

void wifi_manager_init(void)
{
    // This block contains one-time initializations and should only run once.
    if (!s_is_initialized) {
        ESP_ERROR_CHECK(esp_netif_init());
        ESP_ERROR_CHECK(esp_event_loop_create_default());
        esp_netif_create_default_wifi_sta();

        wifi_init_config_t cfg = WIFI_INIT_CONFIG_DEFAULT();
        ESP_ERROR_CHECK(esp_wifi_init(&cfg));

        parse_wifi_credentials();

        ESP_ERROR_CHECK(esp_event_handler_instance_register(WIFI_EVENT, ESP_EVENT_ANY_ID, &wifi_event_handler, NULL, NULL));
        ESP_ERROR_CHECK(esp_event_handler_instance_register(IP_EVENT, IP_EVENT_STA_GOT_IP, &wifi_event_handler, NULL, NULL));

        s_is_initialized = true;
        ESP_LOGI(TAG, "Wi-Fi system initialized.");
    }

    // These functions are safe to call multiple times to start/restart the Wi-Fi connection.
    ESP_ERROR_CHECK(esp_wifi_set_mode(WIFI_MODE_STA));
    ESP_ERROR_CHECK(esp_wifi_start());
    ESP_ERROR_CHECK(esp_wifi_set_ps(WIFI_PS_NONE));

}

bool wifi_is_connected(void)
{
    return s_is_connected;
}

esp_err_t wifi_get_mac_address(char* mac_str)
{
    uint8_t mac[6];
    esp_err_t err = esp_wifi_get_mac(WIFI_IF_STA, mac);
    if (err == ESP_OK)
    {
        sprintf(mac_str, "%02x:%02x:%02x:%02x:%02x:%02x",
                mac[0], mac[1], mac[2], mac[3], mac[4], mac[5]);
    }
    return err;
}

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