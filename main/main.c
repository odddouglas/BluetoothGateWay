#include <stdio.h>
#include <string.h>
#include <time.h>
#include "esp_system.h"
#include "nvs_flash.h"
#include "esp_event.h"
#include "esp_netif.h"
#include "esp_log.h"
#include "mqtt_client.h"
#include "esp_wifi.h"

#define TAG "HUAWEI_MQTT"
#define WIFI_SSID "odddouglas"     // 修改为你想连接的 Wi-Fi SSID
#define WIFI_PASSWORD "odddouglas" // 修改为 Wi-Fi 密码

#define DEVICE_ID "67ed58015367f573f77ef961_esp32"
#define DEVICE_SECRET "1dd8ae3b5de51602871d8039dabf6f9e"
#define CLIENT_ID "67ed58015367f573f77ef961_esp32_0_0_2025040406"
#define CLIENT_SECRET "70bc70ecf2946cdbeb8a4585367b13f40122000155517925bbcab2ee92d01d3d"

#define BROKER_URI "mqtt://e5e7404266.st1.iotda-device.cn-north-4.myhuaweicloud.com:1883"

// 上报属性的主题宏定义
#define TOPIC_PROPERTIES_REPORT "$oc/devices/%s/sys/properties/report"

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

void wifi_init(void)
{
    // 初始化 Wi-Fi 配置结构体
    wifi_init_config_t cfg = WIFI_INIT_CONFIG_DEFAULT();

    // 初始化 Wi-Fi 驱动
    ESP_ERROR_CHECK(esp_wifi_init(&cfg));

    wifi_config_t wifi_config = {
        .sta = {
            .ssid = WIFI_SSID,
            .password = WIFI_PASSWORD,
        },
    };

    ESP_LOGI(TAG, "Setting WiFi configuration SSID %s password %s", WIFI_SSID, WIFI_PASSWORD);

    // 设置 Wi-Fi 模式为 Station 模式
    ESP_ERROR_CHECK(esp_wifi_set_mode(WIFI_MODE_STA));

    // 设置 Wi-Fi 配置
    ESP_ERROR_CHECK(esp_wifi_set_config(ESP_IF_WIFI_STA, &wifi_config));

    // 启动 Wi-Fi
    ESP_ERROR_CHECK(esp_wifi_start());

    // 等待 Wi-Fi 连接
    ESP_LOGI(TAG, "Attempting to connect to Wi-Fi...");

    ESP_ERROR_CHECK(esp_wifi_connect());
}

void app_main()
{
    // 初始化基础组件
    ESP_ERROR_CHECK(nvs_flash_init());
    ESP_ERROR_CHECK(esp_netif_init());
    ESP_ERROR_CHECK(esp_event_loop_create_default());

    // 连接WiFi
    wifi_init();
    // ESP_ERROR_CHECK(example_connect());

    // 配置MQTT客户端（非加密）
    esp_mqtt_client_config_t mqtt_cfg = {
        .broker.address.uri = BROKER_URI,
        .credentials = {
            .client_id = CLIENT_ID,
            .username = DEVICE_ID,
            .authentication.password = CLIENT_SECRET,
        },
    };

    esp_mqtt_client_handle_t client = esp_mqtt_client_init(&mqtt_cfg);
    esp_mqtt_client_register_event(client, ESP_EVENT_ANY_ID, mqtt_event_handler, NULL);
    esp_mqtt_client_start(client);

    // 模拟数据上报
    int counter = 0;
    while (1)
    {
        vTaskDelay(10000 / portTICK_PERIOD_MS);

        // 生成模拟数据
        float temp = 20.0 + (counter % 10);
        float humi = 90.0 + (counter % 20);
        bool led = (counter++ % 2);

        publish_sensor_data(client, temp, humi, led);
    }
}
