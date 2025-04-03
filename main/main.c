#include <stdio.h>
#include "freertos/FreeRTOS.h"
#include "freertos/task.h"
#include "led_strip.h"
#include "esp_log.h"
#include "driver/gpio.h"

static const char *TAG = "example";

#define LED_GPIO 48 // LED 数据线连接的 GPIO
#define LED_NUM 1   // LED 灯带上的 LED 数量

static led_strip_handle_t led_strip;

void app_main(void)
{
    // LED 配置
    led_strip_config_t strip_config = {
        .strip_gpio_num = LED_GPIO,
        .max_leds = LED_NUM,
    };

    // RMT 配置
    led_strip_rmt_config_t rmt_config = {
        .resolution_hz = 10 * 1000 * 1000, // 10 MHz
        .flags.with_dma = false,
    };
    // 初始化 LED 灯带
    ESP_ERROR_CHECK(led_strip_new_rmt_device(&strip_config, &rmt_config, &led_strip));

    while (1)
    {
        // 设置红色
        ESP_ERROR_CHECK(led_strip_set_pixel(led_strip, 0, 255, 0, 0)); // 红色 (255, 0, 0)
        ESP_ERROR_CHECK(led_strip_refresh(led_strip));
        ESP_LOGI(TAG, "LED ON");

        vTaskDelay(pdMS_TO_TICKS(500)); // 等待500ms

        // 关闭 LED
        ESP_ERROR_CHECK(led_strip_clear(led_strip)); // 清空 LED
        ESP_LOGI(TAG, "LED OFF");

        vTaskDelay(pdMS_TO_TICKS(500)); // 等待500ms
    }
}
