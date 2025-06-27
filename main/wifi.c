#include <esp_log.h>
#include "app_config.h"
#include <string.h>
#include "esp_wifi.h"
#include "esp_event.h"
#include "esp_netif.h"
#include "nvs_flash.h"
#include "wifi.h"
#include "sdkconfig.h"
#include "freertos/event_groups.h" // Add this include

#define TAG "WIFI"

// Define FreeRTOS event group and bits
static EventGroupHandle_t s_wifi_event_group;
#define WIFI_CONNECTED_BIT BIT0
#define WIFI_FAIL_BIT BIT1

static void wifi_event_handler(void* arg, esp_event_base_t event_base,
                                int32_t event_id, void* event_data)
{
    if (event_base == WIFI_EVENT && event_id == WIFI_EVENT_STA_START) {
        esp_wifi_connect();
    } else if (event_base == WIFI_EVENT && event_id == WIFI_EVENT_STA_DISCONNECTED) {
        wifi_event_sta_disconnected_t* event = (wifi_event_sta_disconnected_t*) event_data;
        ESP_LOGE(TAG, "Wi-Fi disconnected, reason: %d", event->reason);
        // Do not call esp_wifi_connect() here to allow wifi_init_sta to try next network
        if (s_wifi_event_group != NULL) {
            xEventGroupSetBits(s_wifi_event_group, WIFI_FAIL_BIT);
        }
    } else if (event_base == IP_EVENT && event_id == IP_EVENT_STA_GOT_IP) {
        ip_event_got_ip_t* event = (ip_event_got_ip_t*) event_data;
        ESP_LOGI(TAG, "got ip:" IPSTR, IP2STR(&event->ip_info.ip));
        if (s_wifi_event_group != NULL) {
            xEventGroupSetBits(s_wifi_event_group, WIFI_CONNECTED_BIT);
        }
    }
}


void wifi_init_sta(void) {
    s_wifi_event_group = xEventGroupCreate(); // Create event group

    ESP_ERROR_CHECK(esp_netif_init());
    ESP_ERROR_CHECK(esp_event_loop_create_default());
    esp_netif_create_default_wifi_sta();

    wifi_init_config_t cfg = WIFI_INIT_CONFIG_DEFAULT();
    ESP_ERROR_CHECK(esp_wifi_init(&cfg));

    // Register the event handler
    esp_event_handler_instance_t instance_any_id;
    esp_event_handler_instance_t instance_got_ip;
    ESP_ERROR_CHECK(esp_event_handler_instance_register(WIFI_EVENT,
                                                        ESP_EVENT_ANY_ID,
                                                        &wifi_event_handler,
                                                        NULL,
                                                        &instance_any_id));
    ESP_ERROR_CHECK(esp_event_handler_instance_register(IP_EVENT,
                                                        IP_EVENT_STA_GOT_IP,
                                                        &wifi_event_handler,
                                                        NULL,
                                                        &instance_got_ip));


    char* wifi_credentials_copy = strdup(CONFIG_WIFI_CREDENTIALS);
    if (wifi_credentials_copy == NULL) {
        ESP_LOGE(TAG, "Failed to duplicate wifi credentials string.");
        return;
    }

    char* network_str_saveptr;
    char* network_str = strtok_r(wifi_credentials_copy, ";", &network_str_saveptr);
    bool connected = false;

    while (network_str != NULL && !connected) {
        char* ssid_saveptr;
        char* ssid = strtok_r(network_str, ":", &ssid_saveptr);
        char* password = strtok_r(NULL, ":", &ssid_saveptr);

        if (ssid && password) {
            wifi_config_t wifi_config = {
                .sta = {
                    .threshold.authmode = WIFI_AUTH_WPA2_PSK,
                },
            };
            strncpy((char*)wifi_config.sta.ssid, ssid, sizeof(wifi_config.sta.ssid) - 1);
            wifi_config.sta.ssid[sizeof(wifi_config.sta.ssid) - 1] = 0; // Null-terminate
            strncpy((char*)wifi_config.sta.password, password, sizeof(wifi_config.sta.password) - 1);
            wifi_config.sta.password[sizeof(wifi_config.sta.password) - 1] = 0; // Null-terminate


            ESP_LOGI(TAG, "Attempting to connect to Wi-Fi network: %s with password %s", ssid, password);

            // ESP_ERROR_CHECK(esp_wifi_set_mode(WIFI_MODE_STA));
            // ESP_ERROR_CHECK(esp_wifi_set_config(WIFI_IF_STA, &wifi_config));
            // ESP_ERROR_CHECK(esp_wifi_start());

            esp_err_t err = esp_wifi_set_mode(WIFI_MODE_STA);
            if (err != ESP_OK)
            {
                ESP_LOGE(TAG, "WiFi set mode failed: %s", esp_err_to_name(err));
            }
            err = esp_wifi_set_config(WIFI_IF_STA, &wifi_config);
            if (err != ESP_OK)
            {
                ESP_LOGE(TAG, "WiFi config failed: %s", esp_err_to_name(err));
            }
            err = esp_wifi_start();
            if (err != ESP_OK)
            {
                ESP_LOGE(TAG, "WiFi start failed: %s", esp_err_to_name(err));
            }

            // Wait for connection result
            EventBits_t bits = xEventGroupWaitBits(s_wifi_event_group,
                                                   WIFI_CONNECTED_BIT | WIFI_FAIL_BIT,
                                                   pdTRUE, // Clear bits on exit
                                                   pdFALSE, // Don't wait for all bits
                                                   portMAX_DELAY); // Wait indefinitely

            if (bits & WIFI_CONNECTED_BIT) {
                ESP_LOGI(TAG, "Successfully connected to Wi-Fi network: %s", ssid);
                connected = true;
            } else if (bits & WIFI_FAIL_BIT) {
                ESP_LOGE(TAG, "Failed to connect to network: %s. Trying next network...", ssid);
                // Stop the current Wi-Fi connection to try the next one cleanly
                esp_wifi_stop();
            }
        }
        network_str = strtok_r(NULL, ";", &network_str_saveptr);
    }

    if (!connected) {
        ESP_LOGE(TAG, "Failed to connect to any Wi-Fi network provided.");
    }

    free(wifi_credentials_copy);
    vEventGroupDelete(s_wifi_event_group); // Delete event group when done
    ESP_LOGI(TAG, "wifi_init_sta finished.");
}

esp_err_t wifi_get_mac_address(char *mac_str)
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