file(GLOB app_sources "*.c")

idf_component_register(
        SRCS ${app_sources}
        INCLUDE_DIRS "." "../include"
        EMBED_FILES "server_cert.pem"
        REQUIRES esp_bh1750 esp_wifi esp_event esp_netif nvs_flash esp_http_client esp-tls json)
