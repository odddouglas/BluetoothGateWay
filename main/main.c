#include <stdio.h>
#include "freertos/FreeRTOS.h"
#include "freertos/task.h"
#include "led_strip.h"
#include "esp_log.h"
#include "driver/gpio.h"
#include "key.h"

void app_main(void)
{
    key_init();
    WS2812_init();
    while (1)
    {

        WS2812_blink(12, 12, 0);

        if (scan_keyval() == 1)
        {
            printf("key pressed\n");
        }
    }
}
