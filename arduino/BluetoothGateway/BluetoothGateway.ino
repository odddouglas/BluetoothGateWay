#include <WiFi.h>
#include <PubSubClient.h>
 
WiFiClient espClient;
PubSubClient client(espClient);

// WiFi 配置
const char* ssid = "odddouglas";
const char* password = "odddouglas";

// MQTT 配置（请使用 mqtt 端口 1883，而非 mqtts）
const char* mqttServer = "e5e7404266.st1.iotda-device.cn-north-4.myhuaweicloud.com";
const int mqttPort = 1883;

// 三元组信息
const char* ClientId     = "67ed58015367f573f77ef961_esp32_0_0_2025040411";
const char* mqttUser     = "67ed58015367f573f77ef961_esp32";
const char* mqttPassword = "106025bd4390a90b15da1f4aa5c4da6eabc5751bb4efcc16609465b3982c08ae";

#define DEVICE_ID "67ed58015367f573f77ef961_esp32"

// 设备属性上报的 topic
#define MQTT_TOPIC_REPORT "$oc/devices/" DEVICE_ID "/sys/properties/report"

// 模拟数据
int data_temp = 25;
int data_humi = 60;
bool led_state = true;

long lastMsg = 0;

void setup() {
  Serial.begin(115200);
  WIFI_Init();
  MQTT_Init();
}

void loop() {
  if (!client.connected()) {
    MQTT_Init();
  } else {
    client.loop();
  }

  long now = millis();
  if (now - lastMsg > 10000) {  // 每 10 秒上报一次
    lastMsg = now;
    MQTT_POST();
    data_temp += 1; // 模拟数据变化
    data_humi += 2;
    led_state = !led_state;
  }
}

void WIFI_Init() {
  WiFi.begin(ssid, password);
  Serial.print("Connecting to WiFi");
  while (WiFi.status() != WL_CONNECTED) {
    delay(500);
    Serial.print(".");
  }
  Serial.println();
  Serial.println("WiFi connected");
  Serial.println(WiFi.localIP());
}

void MQTT_Init() {
  client.setServer(mqttServer, mqttPort);
  client.setKeepAlive(60);

  while (!client.connected()) {
    Serial.println("Connecting to Huawei Cloud MQTT...");
    if (client.connect(ClientId, mqttUser, mqttPassword)) {
      Serial.println("Connected to MQTT broker");
    } else {
      Serial.print("Failed with state ");
      Serial.println(client.state());
      delay(3000);
    }
  }
}

void MQTT_POST() {
  // 构造 JSON 数据（注意服务 ID 和属性结构）
  char jsonBuf[256];
  snprintf(jsonBuf, sizeof(jsonBuf),
           "{\"services\":[{\"service_id\":\"gateway_data\",\"properties\":{"
           "\"temperature\":%d,"
           "\"humidity\":%d,"
           "\"led\":%s"
           "}}]}",
           data_temp, data_humi, led_state ? "true" : "false");

  // 发布到华为云平台
  boolean result = client.publish(MQTT_TOPIC_REPORT, jsonBuf);
  Serial.println("[MQTT] Publish:");
  Serial.println(MQTT_TOPIC_REPORT);
  Serial.println(jsonBuf);
  Serial.println(result ? "Publish Success!" : "Publish Failed!");
}
