file(GLOB app_sources "*.c")

idf_component_register(
        SRCS ${app_sources}
        INCLUDE_DIRS "." "../include"
        REQUIRES driver esp_wifi esp_event esp_netif nvs_flash esp_http_client json)