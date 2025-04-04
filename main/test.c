#include <stdio.h>
#include <stdint.h>
#include <stddef.h>
#include <string.h>
#include "esp_system.h"
#include "nvs_flash.h"
#include "esp_event.h"
#include "esp_netif.h"
#include "protocol_examples_common.h"
#include "esp_log.h"
#include "mqtt_client.h"

static const char *TAG = "MQTT_EXAMPLE"; // 日志标签，用于标识日志来源
#define MQTT_SERVER "mqtt://e5e7404266.st1.iotda-device.cn-north-4.myhuaweicloud.com"
#define MQTT_PORT 1883
// https://iot-tool.obs-website.cn-north-4.myhuaweicloud.com/ 可根据DEVICE_ID和DEVICE_SECRET
#define CLIENT_ID "67ed58015367f573f77ef961_esp32_0_1_2025040315"
#define MQTT_USER "67ed58015367f573f77ef961_esp32"
#define MQTT_PASSWORD "d5030bcd2f290ed473ccdf38aa7f3d3d01d42900c9ec357ec696a7f93c346184"
#define DEVICE_ID "67ed58015367f573f77ef961_esp32"
#define DEVICE_SECRET "1dd8ae3b5de51602871d8039dabf6f9e"

// 如果错误码不为零，打印错误信息
static void log_error_if_nonzero(const char *message, int error_code)
{
    if (error_code != 0)
    {
        ESP_LOGE(TAG, "Last error %s: 0x%x", message, error_code); // 打印错误信息
    }
}

// MQTT 事件处理函数
static void mqtt_event_handler(void *handler_args, esp_event_base_t base, int32_t event_id, void *event_data)
{
    ESP_LOGD(TAG, "Event dispatched from event loop base=%s, event_id=%" PRIi32 "", base, event_id); // 打印事件信息
    esp_mqtt_event_handle_t event = event_data;
    esp_mqtt_client_handle_t client = event->client;
    int msg_id;

    switch ((esp_mqtt_event_id_t)event_id)
    {
    case MQTT_EVENT_CONNECTED: // 连接成功事件
        ESP_LOGI(TAG, "MQTT_EVENT_CONNECTED");
        msg_id = esp_mqtt_client_publish(client, "/topic/qos1", "data_3", 0, 1, 0); // 发布消息
        ESP_LOGI(TAG, "sent publish successful, msg_id=%d", msg_id);

        msg_id = esp_mqtt_client_subscribe(client, "/topic/qos0", 0); // 订阅主题
        ESP_LOGI(TAG, "sent subscribe successful, msg_id=%d", msg_id);

        msg_id = esp_mqtt_client_subscribe(client, "/topic/qos1", 1); // 订阅另一个主题
        ESP_LOGI(TAG, "sent subscribe successful, msg_id=%d", msg_id);
        break;

    case MQTT_EVENT_DISCONNECTED: // 断开连接事件
        ESP_LOGI(TAG, "MQTT_EVENT_DISCONNECTED");
        break;

    case MQTT_EVENT_SUBSCRIBED: // 订阅成功事件
        ESP_LOGI(TAG, "MQTT_EVENT_SUBSCRIBED, msg_id=%d", event->msg_id);
        msg_id = esp_mqtt_client_publish(client, "/topic/qos0", "data", 0, 0, 0); // 发布消息
        ESP_LOGI(TAG, "sent publish successful, msg_id=%d", msg_id);
        break;

    case MQTT_EVENT_UNSUBSCRIBED: // 取消订阅事件
        ESP_LOGI(TAG, "MQTT_EVENT_UNSUBSCRIBED, msg_id=%d", event->msg_id);
        break;

    case MQTT_EVENT_PUBLISHED: // 发布成功事件
        ESP_LOGI(TAG, "MQTT_EVENT_PUBLISHED, msg_id=%d", event->msg_id);
        break;

    case MQTT_EVENT_DATA: // 接收到消息事件
        ESP_LOGI(TAG, "MQTT_EVENT_DATA");
        printf("TOPIC=%.*s\r\n", event->topic_len, event->topic); // 打印接收到的主题
        printf("DATA=%.*s\r\n", event->data_len, event->data);    // 打印接收到的数据
        break;

    case MQTT_EVENT_ERROR: // 错误事件
        ESP_LOGI(TAG, "MQTT_EVENT_ERROR");
        if (event->error_handle->error_type == MQTT_ERROR_TYPE_TCP_TRANSPORT) // 检查 TCP 传输错误
        {
            log_error_if_nonzero("reported from esp-tls", event->error_handle->esp_tls_last_esp_err);
            log_error_if_nonzero("reported from tls stack", event->error_handle->esp_tls_stack_err);
            log_error_if_nonzero("captured as transport's socket errno", event->error_handle->esp_transport_sock_errno);
            ESP_LOGI(TAG, "Last errno string (%s)", strerror(event->error_handle->esp_transport_sock_errno)); // 打印错误的 errno 信息
        }
        break;

    default: // 其他事件
        ESP_LOGI(TAG, "Other event id:%d", event->event_id);
        break;
    }
}

// MQTT 客户端启动函数
static void mqtt_app_start(void)
{
    // esp_mqtt_client_config_t mqtt_cfg = {
    //     .broker.address.uri = CONFIG_BROKER_URL, // 配置 MQTT Broker 的 URL
    // };
    esp_mqtt_client_config_t mqtt_cfg = {
        .broker.address.uri = MQTT_SERVER,
        .broker.address.port = MQTT_PORT,
        .credentials.username = MQTT_USER,
        .credentials.client_id = CLIENT_ID,
        .credentials.authentication.password = MQTT_PASSWORD,
    };
    esp_mqtt_client_handle_t client = esp_mqtt_client_init(&mqtt_cfg);                  // 初始化 MQTT 客户端
    esp_mqtt_client_register_event(client, ESP_EVENT_ANY_ID, mqtt_event_handler, NULL); // 注册 MQTT 事件处理函数

    // 将一些消息添加到消息队列中，以便查看消息队列的内存分配
    int msg_id;
    msg_id = esp_mqtt_client_enqueue(client, "/topic/qos1", "data_3", 0, 1, 0, true); // 将消息添加到队列
    ESP_LOGI(TAG, "Enqueued msg_id=%d", msg_id);
    msg_id = esp_mqtt_client_enqueue(client, "/topic/qos2", "QoS2 message", 0, 2, 0, true); // 添加另一个消息
    ESP_LOGI(TAG, "Enqueued msg_id=%d", msg_id);

    // 启动客户端，开始处理队列中的消息
    esp_mqtt_client_start(client);
}

void app_main(void)
{
    ESP_LOGI(TAG, "[APP] Startup..");
    ESP_LOGI(TAG, "[APP] Free memory: %" PRIu32 " bytes", esp_get_free_heap_size()); // 打印剩余内存

    // 设置日志级别
    esp_log_level_set("*", ESP_LOG_INFO);
    esp_log_level_set("mqtt_client", ESP_LOG_VERBOSE);
    esp_log_level_set("MQTT_EXAMPLE", ESP_LOG_VERBOSE);
    esp_log_level_set("TRANSPORT_BASE", ESP_LOG_VERBOSE);
    esp_log_level_set("esp-tls", ESP_LOG_VERBOSE);
    esp_log_level_set("TRANSPORT", ESP_LOG_VERBOSE);
    esp_log_level_set("custom_outbox", ESP_LOG_VERBOSE);

    ESP_ERROR_CHECK(nvs_flash_init());                // 初始化 NVS（非易失性存储）
    ESP_ERROR_CHECK(esp_netif_init());                // 初始化网络接口
    ESP_ERROR_CHECK(esp_event_loop_create_default()); // 创建默认的事件循环

    // 连接 Wi-Fi 或 Ethernet，具体取决于菜单配置
    ESP_ERROR_CHECK(example_connect());

    mqtt_app_start(); // 启动 MQTT 客户端
}
