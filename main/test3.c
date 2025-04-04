#include <stdio.h>
#include <string.h>
#include <time.h>
#include "esp_system.h"
#include "nvs_flash.h"
#include "esp_event.h"
#include "esp_netif.h"
#include "esp_log.h"
#include "esp_tls.h"
#include "mqtt_client.h"
#include "mbedtls/sha256.h"
#include "esp_sntp.h"
#include "protocol_examples_common.h"

static const char *TAG = "HUAWEI_IOT";

// 华为IoT平台配置（替换为您的实际信息）
#define DEVICE_ID "67ed58015367f573f77ef961_esp32"
#define DEVICE_SECRET "1dd8ae3b5de51602871d8039dabf6f9e"
#define BROKER_URI "mqtts://e5e7404266.st1.iotda-device.cn-north-4.myhuaweicloud.com:8883"

// 华为IoT平台CA证书（示例证书，请替换为实际证书）
static const char *huawei_root_ca_pem = \
"-----BEGIN CERTIFICATE-----\n" \
"MIIDdTCCAl2gAwIBAgILBAAAAAABFUtaw5QwDQYJKoZIhvcNAQEFBQAwVzELMAkG\n" \
"...（完整证书内容）...\n" \
"-----END CERTIFICATE-----\n";

// 生成HMAC-SHA256密码（华为要求）
static void generate_password(char *password, size_t max_len, const char *secret, const char *timestamp) {
    uint8_t hmac_result[32];
    mbedtls_md_context_t ctx;
    mbedtls_md_type_t md_type = MBEDTLS_MD_SHA256;
    
    mbedtls_md_init(&ctx);
    mbedtls_md_setup(&ctx, mbedtls_md_info_from_type(md_type), 1);
    mbedtls_md_hmac_starts(&ctx, (const unsigned char *)timestamp, strlen(timestamp));
    mbedtls_md_hmac_update(&ctx, (const unsigned char *)secret, strlen(secret));
    mbedtls_md_hmac_finish(&ctx, hmac_result);
    mbedtls_md_free(&ctx);
    
    for (int i = 0; i < sizeof(hmac_result); i++) {
        snprintf(password + (i * 2), max_len - (i * 2), "%02x", hmac_result[i]);
    }
}

// 错误日志辅助函数
static void log_error_if_nonzero(const char *message, int error_code) {
    if (error_code != 0) {
        ESP_LOGE(TAG, "Last error %s: 0x%x", message, error_code);
    }
}

// 初始化SNTP时间同步（使用新的esp_sntp_ API）
static void initialize_sntp(void) {
    ESP_LOGI(TAG, "Initializing SNTP");
    esp_sntp_setoperatingmode(SNTP_OPMODE_POLL);
    esp_sntp_setservername(0, "pool.ntp.org");
    esp_sntp_init();
    
    // 等待时间同步（最多等待10秒）
    int retry = 0;
    const int retry_count = 10;
    while (sntp_get_sync_status() == SNTP_SYNC_STATUS_RESET && ++retry < retry_count) {
        ESP_LOGI(TAG, "Waiting for system time to be set... (%d/%d)", retry, retry_count);
        vTaskDelay(1000 / portTICK_PERIOD_MS);
    }
}

// 上报数据到华为IoT平台
static void report_device_data(esp_mqtt_client_handle_t client, const char *service_data) {
    char publish_topic[128];
    snprintf(publish_topic, sizeof(publish_topic), 
            "/huawei/v1/devices/%s/data/json", DEVICE_ID);
    
    char payload[512];
    snprintf(payload, sizeof(payload),
            "{\"msgType\":\"deviceReq\",\"data\":[{\"serviceId\":\"DeviceInfo\",\"serviceData\":%s}]}", 
            service_data);
    
    int msg_id = esp_mqtt_client_publish(client, publish_topic, payload, 0, 1, 0);
    ESP_LOGI(TAG, "Sent publish successful, msg_id=%d", msg_id);
}

// 华为IoT平台MQTT事件处理
static void mqtt_event_handler(void *handler_args, esp_event_base_t base, int32_t event_id, void *event_data) {
    esp_mqtt_event_handle_t event = event_data;
    
    switch ((esp_mqtt_event_id_t)event_id) {
        case MQTT_EVENT_CONNECTED:
            ESP_LOGI(TAG, "Connected to Huawei IoT Platform");
            
            char subscribe_topic[128];
            snprintf(subscribe_topic, sizeof(subscribe_topic), 
                    "/huawei/v1/devices/%s/command/json", DEVICE_ID);
            esp_mqtt_client_subscribe(event->client, subscribe_topic, 1);
            
            report_device_data(event->client, "{\"status\":1}");
            break;
            
        case MQTT_EVENT_DATA:
            ESP_LOGI(TAG, "Received Huawei IoT Message");
            printf("TOPIC=%.*s\n", event->topic_len, event->topic);
            printf("DATA=%.*s\n", event->data_len, event->data);
            
            if (strstr(event->topic, "/command/")) {
                ESP_LOGI(TAG, "Processing Huawei command...");
            }
            break;
            
        case MQTT_EVENT_ERROR:
            ESP_LOGE(TAG, "MQTT Error: %d", event->error_handle->error_type);
            if (event->error_handle->error_type == MQTT_ERROR_TYPE_TCP_TRANSPORT) {
                log_error_if_nonzero("reported from esp-tls", event->error_handle->esp_tls_last_esp_err);
            }
            break;
            
        default:
            break;
    }
}

// 启动MQTT客户端
static void mqtt_app_start(void) {
    char timestamp[32];
    snprintf(timestamp, sizeof(timestamp), "%lld", (long long)time(NULL));
    
    char password[256];
    generate_password(password, sizeof(password), DEVICE_SECRET, timestamp);
    
    char client_id[256];
    snprintf(client_id, sizeof(client_id), "%s_0_0_%s", DEVICE_ID, timestamp);
    
    esp_mqtt_client_config_t mqtt_cfg = {
        .broker = {
            .address.uri = BROKER_URI,
            .verification.certificate = huawei_root_ca_pem
        },
        .credentials = {
            .username = DEVICE_ID,
            .client_id = client_id,
            .authentication.password = password
        },
        .network = {
            .timeout_ms = 10000,
            .disable_auto_reconnect = false,
            .reconnect_timeout_ms = 5000
        },
        .buffer = {
            .size = 4096,
            .out_size = 2048
        }
    };
    
    esp_mqtt_client_handle_t client = esp_mqtt_client_init(&mqtt_cfg);
    esp_mqtt_client_register_event(client, ESP_EVENT_ANY_ID, mqtt_event_handler, NULL);
    esp_mqtt_client_start(client);
    
    while (1) {
        vTaskDelay(60000 / portTICK_PERIOD_MS);
        char data[128];
        snprintf(data, sizeof(data), "{\"status\":1,\"timestamp\":%lld}", (long long)time(NULL));
        report_device_data(client, data);
    }
}

void app_main(void) {
    ESP_ERROR_CHECK(nvs_flash_init());
    ESP_ERROR_CHECK(esp_netif_init());
    ESP_ERROR_CHECK(esp_event_loop_create_default());
    
    ESP_ERROR_CHECK(example_connect());
    
    initialize_sntp();
    mqtt_app_start();
}