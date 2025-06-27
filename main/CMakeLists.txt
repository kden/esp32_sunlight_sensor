file(GLOB app_sources "*.c")

idf_component_register(
        SRCS ${app_sources}
        INCLUDE_DIRS "." "../include"
        REQUIRES esp_bh1750 esp_ssd1306)