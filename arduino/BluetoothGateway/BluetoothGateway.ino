#include <WiFi.h>
#include <PubSubClient.h>
#include <ArduinoJson.h>

WiFiClient espClient;
PubSubClient client(espClient);

// WiFi 配置
const char *ssid = "odddouglas";
const char *password = "odddouglas";

// MQTT 配置（请使用 mqtt 端口 1883，而非 mqtts）
const char *mqttServer = "e5e7404266.st1.iotda-device.cn-north-4.myhuaweicloud.com";
const int mqttPort = 1883;

// 三元组信息
const char *ClientId = "67ed58015367f573f77ef961_esp32_0_0_2025040411";
const char *mqttUser = "67ed58015367f573f77ef961_esp32";
const char *mqttPassword = "106025bd4390a90b15da1f4aa5c4da6eabc5751bb4efcc16609465b3982c08ae";

#define DEVICE_ID "67ed58015367f573f77ef961_esp32"
#define SERVER_ID "gateway_data"
// 设备属性上报的 topic
#define MQTT_TOPIC_REPORT "$oc/devices/" DEVICE_ID "/sys/properties/report"
// 设备订阅命令的 topic
#define MQTT_TOPIC_COMMAND "$oc/devices/" DEVICE_ID "/sys/commands/#"
#define MQTT_TOPIC_COMMAND_RESPOND "$oc/devices/" DEVICE_ID "/sys/commands/response/request_id="

// 模拟数据
float data_temp = 25.0;
float data_humi = 60.0;
bool led_state = true;

long lastMsg = 0;

void setup()
{
  Serial.begin(115200);
  WIFI_Init();
  MQTT_Init();
}

void loop()
{
  if (!client.connected())
  {
    MQTT_Init();
  }
  else
  {
    client.loop();
  }

  long now = millis();
  if (now - lastMsg > 10000)
  { // 每 10 秒上报一次
    lastMsg = now;
    MQTT_Report();
    data_temp += 1; // 模拟数据变化
    data_humi += 2;
    led_state = !led_state;
  }
}

void WIFI_Init()
{
  WiFi.begin(ssid, password);
  Serial.print("Connecting to WiFi");
  while (WiFi.status() != WL_CONNECTED)
  {
    delay(500);
    Serial.print(".");
  }
  Serial.println();
  Serial.println("WiFi connected");
  Serial.println(WiFi.localIP());
}

void MQTT_Init()
{
  client.setServer(mqttServer, mqttPort);
  client.setKeepAlive(60);
  client.setCallback(handleCommand); // 设置命令回调函数

  Serial.println("[MQTT] Connecting to Huawei Cloud...");

  while (!client.connected())
  {
    boolean result = client.connect(ClientId, mqttUser, mqttPassword);

    Serial.println(result ? "[MQTT] Connected to Broker!" : "[MQTT] Connection Failed!");
    if (result)
    {
      // 订阅命令下发 Topic
      boolean subResult = client.subscribe(MQTT_TOPIC_COMMAND);
      Serial.println("[MQTT] Subscribe to Command Topic:");
      Serial.println(subResult ? "Subscribe Success!" : "Subscribe Failed!");
    }
    else
    {
      Serial.print("[MQTT] Failed State Code: ");
      Serial.println(client.state());
      delay(3000); // 等待后重连
    }
  }
}

// 上报设备属性
void MQTT_Report()
{
  // 构造 JSON 数据（注意服务 ID 和属性结构）
  char jsonBuf[256];
  snprintf(jsonBuf, sizeof(jsonBuf),
           "{\"services\":[{\"service_id\":\"%s\",\"properties\":{"
           "\"temperature\":%.2f,"
           "\"humidity\":%.2f,"
           "\"led\":%s"
           "}}]}",
           SERVER_ID, data_temp, data_humi, led_state ? "true" : "false");

  // 发布到华为云平台
  boolean reportResult = client.publish(MQTT_TOPIC_REPORT, jsonBuf);
  Serial.println("[MQTT] Publish:");
  Serial.println(jsonBuf);
  Serial.println(reportResult ? "Publish Success!" : "Publish Failed!");
}

// 发送命令响应到平台
void MQTT_Respond(String topic, String result)
{
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
  boolean respondResult = client.publish(responseTopic.c_str(), jsonBuf);
  Serial.println("[MQTT] Publish (Command Response):");
  Serial.println(jsonBuf);
  Serial.println(respondResult ? "Publish Success!" : "Publish Failed!");
}

// 命令回调函数：处理平台下发的命令
void handleCommand(char *topic, byte *payload, unsigned int length)
{
  StaticJsonDocument<256> doc; // 创建静态 JSON 文档用于解析

  DeserializationError error = deserializeJson(doc, payload, length); // 解析接收到的 JSON 数据
  if (error)
  {
    Serial.println("Failed to parse JSON"); // 打印解析失败信息
    return;                                 // 退出处理函数
  }

  String payloadStr = ""; // 用于打印接收到的原始 JSON 字符串
  for (unsigned int i = 0; i < length; i++)
  {
    payloadStr += (char)payload[i]; // 字节流转换为字符串
  }
  Serial.println("Received command: " + payloadStr); // 打印接收到的命令

  String commandName = doc["command_name"]; // 获取命令名称字段

  if (commandName == "ctrl") // 判断是否为控制命令
  {
    bool ledOn = doc["paras"]["led_on_off"]; // 读取参数：LED 开关布尔值
    led_state = ledOn;                       // 更新本地 LED 状态变量
    MQTT_Respond(String(topic), "success");  // 回复命令成功
  }
  else
  {
    MQTT_Respond(String(topic), "failure"); // 命令无效，回复失败
  }
}
