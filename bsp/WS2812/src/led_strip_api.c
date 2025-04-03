/*
 * SPDX-FileCopyrightText: 2022 Espressif Systems (Shanghai) CO LTD
 *
 * SPDX-License-Identifier: Apache-2.0
 */
#include "esp_log.h"
#include "esp_check.h"
#include "led_strip.h"
#include "led_strip_interface.h"

static const char *TAG = "led_strip";
#define LED_GPIO 48 // LED 数据线连接的 GPIO
#define LED_NUM 1   // LED 灯带上的 LED 数量
static led_strip_handle_t led_strip;

void WS2812_init()
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
    led_strip_new_rmt_device(&strip_config, &rmt_config, &led_strip);
}
void WS2812_blink(uint32_t red, uint32_t green, uint32_t blue)
{
    // 设置红色
    ESP_ERROR_CHECK(led_strip_set_pixel(led_strip, 0, red, green, blue)); // 红色 (255, 0, 0)
    ESP_ERROR_CHECK(led_strip_refresh(led_strip));
    vTaskDelay(pdMS_TO_TICKS(500)); // 等待500ms
    // 关闭 LED
    ESP_ERROR_CHECK(led_strip_clear(led_strip)); // 清空 LED
    vTaskDelay(pdMS_TO_TICKS(500));              // 等待500ms
}
esp_err_t led_strip_set_pixel(led_strip_handle_t strip, uint32_t index, uint32_t red, uint32_t green, uint32_t blue)
{
    ESP_RETURN_ON_FALSE(strip, ESP_ERR_INVALID_ARG, TAG, "invalid argument");
    return strip->set_pixel(strip, index, red, green, blue);
}

esp_err_t led_strip_set_pixel_hsv(led_strip_handle_t strip, uint32_t index, uint16_t hue, uint8_t saturation, uint8_t value)
{
    ESP_RETURN_ON_FALSE(strip, ESP_ERR_INVALID_ARG, TAG, "invalid argument");

    uint32_t red = 0;
    uint32_t green = 0;
    uint32_t blue = 0;

    uint32_t rgb_max = value;
    uint32_t rgb_min = rgb_max * (255 - saturation) / 255.0f;

    uint32_t i = hue / 60;
    uint32_t diff = hue % 60;

    // RGB adjustment amount by hue
    uint32_t rgb_adj = (rgb_max - rgb_min) * diff / 60;

    switch (i)
    {
    case 0:
        red = rgb_max;
        green = rgb_min + rgb_adj;
        blue = rgb_min;
        break;
    case 1:
        red = rgb_max - rgb_adj;
        green = rgb_max;
        blue = rgb_min;
        break;
    case 2:
        red = rgb_min;
        green = rgb_max;
        blue = rgb_min + rgb_adj;
        break;
    case 3:
        red = rgb_min;
        green = rgb_max - rgb_adj;
        blue = rgb_max;
        break;
    case 4:
        red = rgb_min + rgb_adj;
        green = rgb_min;
        blue = rgb_max;
        break;
    default:
        red = rgb_max;
        green = rgb_min;
        blue = rgb_max - rgb_adj;
        break;
    }

    return strip->set_pixel(strip, index, red, green, blue);
}

esp_err_t led_strip_set_pixel_rgbw(led_strip_handle_t strip, uint32_t index, uint32_t red, uint32_t green, uint32_t blue, uint32_t white)
{
    ESP_RETURN_ON_FALSE(strip, ESP_ERR_INVALID_ARG, TAG, "invalid argument");
    return strip->set_pixel_rgbw(strip, index, red, green, blue, white);
}

esp_err_t led_strip_refresh(led_strip_handle_t strip)
{
    ESP_RETURN_ON_FALSE(strip, ESP_ERR_INVALID_ARG, TAG, "invalid argument");
    return strip->refresh(strip);
}

esp_err_t led_strip_clear(led_strip_handle_t strip)
{
    ESP_RETURN_ON_FALSE(strip, ESP_ERR_INVALID_ARG, TAG, "invalid argument");
    return strip->clear(strip);
}

esp_err_t led_strip_del(led_strip_handle_t strip)
{
    ESP_RETURN_ON_FALSE(strip, ESP_ERR_INVALID_ARG, TAG, "invalid argument");
    return strip->del(strip);
}
