#include <WiFi.h>
#include <PubSubClient.h>
#include <ArduinoJson.h>

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
// 设备订阅命令的 topic
#define MQTT_TOPIC_COMMAND "$oc/devices/" DEVICE_ID "/sys/commands/#"

#define MQTT_TOPIC_COMMAND_RESPOND "$oc/devices/" DEVICE_ID "/sys/commands/response/request_id="

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
  client.setCallback(handleCommand);  // 设置命令回调函数

  while (!client.connected()) {
    Serial.println("Connecting to Huawei Cloud MQTT...");
    if (client.connect(ClientId, mqttUser, mqttPassword)) {
      Serial.println("Connected to MQTT broker");

      // 订阅命令下发Topic
      String commandTopic = MQTT_TOPIC_COMMAND;
      client.subscribe(commandTopic.c_str());  // 使用通配符订阅所有命令
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

// 命令回调函数：处理平台下发的命令

// 回调函数中
void handleCommand(char* topic, byte* payload, unsigned int length) {
  // 反序列化 JSON
  StaticJsonDocument<256> doc;
  DeserializationError error = deserializeJson(doc, payload, length);
  if (error) {
    Serial.println("Failed to parse JSON");
    return;
  }
  String payloadStr = "";
  for (unsigned int i = 0; i < length; i++) {
    payloadStr += (char)payload[i];
  }
  Serial.println("Received command: " + payloadStr);

  String commandName = doc["command_name"];
  if (commandName == "ctrl") {
    bool ledOn = doc["paras"]["led_on_off"];  // 获取布尔值

    led_state = ledOn;
    sendCommandResponse(String(topic), "success");
  } else {
    sendCommandResponse(String(topic), "failure");
  }
}

// 发送命令响应到平台
void sendCommandResponse(String topic, String result) {
  // 构造响应 JSON 数据
  char jsonBuf[128];
  snprintf(jsonBuf, sizeof(jsonBuf),
           "{\"result_code\":0,\"response_name\":\"COMMAND_RESPONSE\",\"paras\":{\"result\":\"%s\"}}",
           result.c_str());

  // 从 topic 中提取 request_id
  int idIndex = topic.lastIndexOf("request_id=");
  String requestId = (idIndex >= 0) ? topic.substring(idIndex + 11) : "";

  // 构造响应的 topic
  String responseTopic = MQTT_TOPIC_COMMAND_RESPOND + requestId;

  // 发布命令响应
  boolean resultPublish = client.publish(responseTopic.c_str(), jsonBuf);
  if (resultPublish) {
    Serial.println("[MQTT] Command Response Sent:");
    Serial.println(jsonBuf);
  } else {
    Serial.println("[MQTT] Failed to send command response");
  }
}

