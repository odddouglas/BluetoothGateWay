#include "led.h"
#include "driver/gpio.h"
#include "esp_log.h"

#define LED1_GPIO 48  // 普通 LED GPIO


// 普通 LED 初始化
void led_init(void)
{
    gpio_config_t io_conf;
    io_conf.intr_type = GPIO_INTR_DISABLE;    // 关闭外部中断
    io_conf.mode = GPIO_MODE_OUTPUT;          // 设置端口为输出模式
    io_conf.pin_bit_mask = 1ULL << LED1_GPIO; // 定义输出端口
    io_conf.pull_down_en = GPIO_PULLDOWN_DISABLE;
    io_conf.pull_up_en = GPIO_PULLUP_ENABLE;

    gpio_config(&io_conf); // 传入结构体进行配置
}

// 点亮普通 LED（低电平点亮）
void led_on(void)
{
    gpio_set_level(LED1_GPIO, 0);
}

// 熄灭普通 LED（高电平熄灭）
void led_off(void)
{
    gpio_set_level(LED1_GPIO, 1);
}

