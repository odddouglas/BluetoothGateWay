#include "key.h"
#include "driver/gpio.h"
#include "freertos/FreeRTOS.h"
#include "freertos/task.h"

#define KEY_GPIO 0

void key_init(void)
{
    // Set up GPIO pin for the key
    gpio_config_t io_conf;
    io_conf.intr_type = GPIO_INTR_DISABLE;        // 关闭外部中断
    io_conf.mode = GPIO_MODE_INPUT;               // 设置端口为输入模式
    io_conf.pin_bit_mask = 1ULL << KEY_GPIO;      // 定义输出端口
    io_conf.pull_down_en = GPIO_PULLDOWN_DISABLE; // 失能下拉电阻
    io_conf.pull_up_en = GPIO_PULLUP_ENABLE;      // 使能上拉电阻
    gpio_config(&io_conf);                        // 传入结构体进行配置
}

uint8_t key_read(void)
{
    return gpio_get_level(KEY_GPIO);
}

uint8_t scan_keyval(void)
{
    uint8_t value = 0;
    if (key_read() == 0)
    {
        vTaskDelay(10 / portTICK_PERIOD_MS);
        if (key_read() == 0)
        {
            value = 1;
            while (key_read() == 0)
            {
                vTaskDelay(10 / portTICK_PERIOD_MS);
            }
        }
    }
    return value;
}