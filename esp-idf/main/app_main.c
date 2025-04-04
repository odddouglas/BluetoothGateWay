#include <stdio.h>
#include <string.h>
#include <time.h>
#include "esp_system.h"
#include "nvs_flash.h"
#include "esp_event.h"
#include "esp_netif.h"
#include "esp_log.h"
#include "mqtt_client.h"
#include "protocol_examples_common.h"
#include "key.h"
#include "led.h"
#include "led_strip.h"

#define TAG "HUAWEI_MQTT"

#define LED_GPIO 48 // LED 数据线连接的 GPIO
#define LED_NUM 1   // LED 灯带上的 LED 数量

#define DEVICE_ID "67ed58015367f573f77ef961_esp32"
#define DEVICE_SECRET "1dd8ae3b5de51602871d8039dabf6f9e"
#define CLIENT_ID "67ed58015367f573f77ef961_esp32_0_0_2025040406"
#define CLIENT_SECRET "70bc70ecf2946cdbeb8a4585367b13f40122000155517925bbcab2ee92d01d3d"

#define BROKER_URI "mqtts://e5e7404266.st1.iotda-device.cn-north-4.myhuaweicloud.com:8883"

// 上报属性的主题宏定义
#define TOPIC_PROPERTIES_REPORT "$oc/devices/%s/sys/properties/report" // 这个%s预留给DEVICE_ID

// led_strip配置
led_strip_config_t strip_config = {
    .strip_gpio_num = LED_GPIO,
    .max_leds = LED_NUM,
};
led_strip_rmt_config_t rmt_config = {
    .resolution_hz = 10 * 1000 * 1000, // 10 MHz
    .flags.with_dma = false,
};
static led_strip_handle_t led_strip;

// 配置MQTT客户端（非加密）
esp_mqtt_client_config_t mqtt_cfg = {
    .broker.address.uri = BROKER_URI,
    .credentials = {
        .client_id = CLIENT_ID,
        .username = DEVICE_ID,
        .authentication.password = CLIENT_SECRET,
    },
};

static void publish_sensor_data(esp_mqtt_client_handle_t client,
                                float temp, float humi, bool led)
{
    // 创建 JSON 格式的负载数据
    char payload[512];
    snprintf(payload, sizeof(payload),
             "{\"services\":[{"
             "\"service_id\":\"gateway_data\","
             "\"properties\":{"
             "\"temperature\":%.1f,"
             "\"humidity\":%.1f,"
             "\"led\":%s"
             "}"
             "}]}",
             temp, humi, led ? "true" : "false");

    // 构建主题
    char topic[256];
    snprintf(topic, sizeof(topic), TOPIC_PROPERTIES_REPORT, DEVICE_ID);

    // 将数据发布到华为云 IoT 平台
    esp_mqtt_client_publish(client, topic, payload, 0, 1, 0);
}
/* 事件处理函数 */
static void mqtt_event_handler(void *handler_args, esp_event_base_t base,
                               int32_t event_id, void *event_data)
{
    esp_mqtt_event_handle_t event = event_data;

    switch (event->event_id)
    {
    case MQTT_EVENT_CONNECTED:
        ESP_LOGI(TAG, "Connected to Huawei IoT Platform");

        publish_sensor_data(event->client, 22.5, 100.0, true);
        break;

    case MQTT_EVENT_DATA:
        ESP_LOGI(TAG, "Received: %.*s", event->data_len, event->data);

        // 处理test服务数据
        if (strstr(event->topic, "command/test"))
        {
            if (strstr(event->data, "temperature"))
            {
                ESP_LOGI(TAG, "Temperature property updated");
            }
            if (strstr(event->data, "humidity"))
            {
                ESP_LOGI(TAG, "Humidity property updated");
            }
            if (strstr(event->data, "led"))
            {
                ESP_LOGI(TAG, "LED control command received");
            }
        }
        break;

    case MQTT_EVENT_ERROR:
        ESP_LOGE(TAG, "MQTT error: %d", event->error_handle->error_type);
        break;

    default:
        break;
    }
}

void app_main()
{
    // 初始化 LED 灯带
    ESP_ERROR_CHECK(led_strip_new_rmt_device(&strip_config, &rmt_config, &led_strip));
    // 初始化基础组件
    ESP_ERROR_CHECK(nvs_flash_init());
    ESP_ERROR_CHECK(esp_netif_init());
    ESP_ERROR_CHECK(esp_event_loop_create_default());
    // 连接WiFi
    ESP_ERROR_CHECK(example_connect());
    // 初始化 MQTT
    esp_mqtt_client_handle_t client = esp_mqtt_client_init(&mqtt_cfg);
    esp_mqtt_client_register_event(client, ESP_EVENT_ANY_ID, mqtt_event_handler, NULL);
    esp_mqtt_client_start(client);

    // 模拟数据上报
    int counter = 0;
    while (1)
    {
        // 设置红色
        led_strip_set_pixel(led_strip, 0, 255, 0, 0); // 红色 (255, 0, 0)
        led_strip_refresh(led_strip);
        ESP_LOGI(TAG, "LED ON");

        vTaskDelay(10000 / portTICK_PERIOD_MS);

        // 生成模拟数据
        float temp = 20.0 + (counter % 10);
        float humi = 90.0 + (counter % 20);
        bool led = (counter++ % 2);

        publish_sensor_data(client, temp, humi, led);
    }
}
