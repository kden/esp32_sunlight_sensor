/**
* @file task_keepalive_blink.c
 *
 * Blink the onboard LED so that the power supply doesn't fall asleep.
 *
 * Copyright (c) 2025 Caden Howell (cadenhowell@gmail.com)
 *
 * Developed with assistance from ChatGPT 4o (2025) and Google Gemini 2.5 Pro (2025).
 *
 * Apache 2.0 Licensed as described in the file LICENSE
 */

#include "task_keepalive_blink.h"
#include "led_strip.h"
#include "esp_log.h"
#include "freertos/FreeRTOS.h"
#include "freertos/task.h"
#include "sdkconfig.h" // Required to access the CONFIG_ macros

#define TAG "KEEPALIVE_BLINK"

#define LED_STRIP_LENGTH   1

static led_strip_handle_t s_led_strip;

/**
 * @brief The FreeRTOS task that continuously blinks the LED.
 */
static void task_keepalive_blink(void *arg) {
    ESP_LOGI(TAG, "Keep-alive blink task running.");

    while (1) {
        // Set LED to bright white to maximize power draw
        led_strip_set_pixel(s_led_strip, 0, 255, 255, 255);
        led_strip_refresh(s_led_strip);
        vTaskDelay(pdMS_TO_TICKS(500)); // On for 500ms

        // Turn LED off
        led_strip_clear(s_led_strip);
        vTaskDelay(pdMS_TO_TICKS(2500)); // Off for 2500ms (total cycle of 3 seconds)
    }
}

/**
 * @brief Initializes the LED strip and creates the blink task.
 */
void init_keepalive_blink_task(void) {
    ESP_LOGI(TAG, "Initializing keep-alive LED on GPIO %d", CONFIG_KEEPALIVE_LED_GPIO);

    led_strip_config_t strip_config = {
        .strip_gpio_num = CONFIG_KEEPALIVE_LED_GPIO,
        .max_leds = LED_STRIP_LENGTH,
        .flags.invert_out = false,
    };

    led_strip_rmt_config_t rmt_config = {
        .clk_src = RMT_CLK_SRC_DEFAULT,
        .resolution_hz = 10 * 1000 * 1000,  // 10MHz resolution
        .mem_block_symbols = 64,
        .flags.with_dma = false,
    };

    ESP_ERROR_CHECK(led_strip_new_rmt_device(&strip_config, &rmt_config, &s_led_strip));

    // No led_strip_enable() needed in new API

    led_strip_clear(s_led_strip);

    xTaskCreate(task_keepalive_blink, "keepalive_blink_task", 2048, NULL, 5, NULL);
}
