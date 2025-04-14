#include <BLEDevice.h>
#include <BLEServer.h>
#include <BLEUtils.h>
#include <BLE2902.h>
#include <ArduinoJson.h>
#include <WiFi.h>
#include <PubSubClient.h>
// 上报设备属性

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
// 设备消息上报的 topic //用不上
#define MQTT_TOPIC_MESSAGE_UP "$oc/devices/" DEVICE_ID "/sys/messages/up"

// 蓝牙相关定义
BLECharacteristic *pCharacteristic;

#define SERVICE_UUID "6E400001-B5A3-F393-E0A9-E50E24DCCA9E"           // UART服务UUID
#define CHARACTERISTIC_UUID_RX "6E400002-B5A3-F393-E0A9-E50E24DCCA9E" // 接收特征UUID
#define CHARACTERISTIC_UUID_TX "6E400003-B5A3-F393-E0A9-E50E24DCCA9E" // 发送特征UUID

bool doScan = true;                                          // 是否开始扫描设备
bool doConnect = false;                                      // 是否连接设备
bool isConnected = false;                                    // 设备是否已连接
bool doSend = false;                                         // 是否发送命令
String led_on_off[4] = {"false", "false", "false", "false"}; // 命令暂存
bool ble_on_off = true;                                      // 命令暂存
BLEAdvertisedDevice *pServer = nullptr;                      // 存储找到的设备
BLERemoteCharacteristic *pRemoteCharacteristic = nullptr;    // 存储远程读取特征
BLERemoteCharacteristic *pRemoteCharacteristic_2 = nullptr;  // 存储远程写入特征
BLEClient *pClient = nullptr;                                // 客户端实例

float data_temp = 0.0; // 温湿度数据变量
float data_humi = 0.0;
String led_state[4] = {"false", "false", "false", "false"}; // 连接之后的led状态
String ble_name = "";                                       // 已连接的蓝牙名称
String cmd = "";                                            // 收发的命令
long last = 0;                                              // 用于定时发报

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
    client.setCallback(MQTT_CmdCallback); // 设置命令回调函数

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
void MQTT_Scan()
{
    if (!client.connected())
    {
        MQTT_Init();
    }
    else
    {
        client.loop();
    }
}

void MQTT_Report()
{
    // 创建一个 JSON 文档对象
    StaticJsonDocument<256> doc;

    // 填充 JSON 数据
    doc["services"][0]["service_id"] = SERVER_ID;
    doc["services"][0]["properties"]["temperature"] = data_temp;
    doc["services"][0]["properties"]["humidity"] = data_humi;
    // 删除旧的布尔上传
    // doc["services"][0]["properties"]["led"] = led_state ? "true" : "false";
    // 新增 stringlist led 上传
    JsonArray ledArray = doc["services"][0]["properties"].createNestedArray("led");
    for (int i = 0; i < 4; i++)
    {
        ledArray.add(led_state[i]);
    }
    // 添加 ble stringlist 属性
    JsonArray bleArray = doc["services"][0]["properties"].createNestedArray("ble");
    bleArray.add(isConnected ? "true" : "false");
    bleArray.add(isConnected ? ble_name : "");

    // 将 JSON 数据序列化为字符串
    String jsonString;
    serializeJson(doc, jsonString); // 序列化 JSON 为字符串

    // 发布到华为云平台
    boolean reportResult = client.publish(MQTT_TOPIC_REPORT, jsonString.c_str());
    Serial.println("[MQTT] Publish:");
    Serial.println(jsonString);
    Serial.println(reportResult ? "Publish Success!" : "Publish Failed!");
}

// 蓝牙连接状态上报
void MQTT_Send()
{
    StaticJsonDocument<200> doc;
    doc["content"]["ble_status"] = isConnected; // 连接状态 (connected / disconnected)
    doc["content"]["device_name"] = ble_name;   // 设备名称

    String jsonString;
    serializeJson(doc, jsonString); // 转换为 JSON 字符串

    // 发布到消息上报 Topic
    boolean reportResult = client.publish(MQTT_TOPIC_MESSAGE_UP, jsonString.c_str());
    Serial.println("[MQTT] Bluetooth connection status published:");
    Serial.println(jsonString);
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
void MQTT_CmdCallback(char *topic, byte *payload, unsigned int length)
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
        // 读取参数：BLE 开关布尔值
        ble_on_off = doc["paras"]["ble_on_off"].as<bool>();

        // 读取参数：LED 开关布尔值（作为数组处理）
        if (doc["paras"].containsKey("led_on_off"))
        {
            JsonArray ledArray = doc["paras"]["led_on_off"].as<JsonArray>();
            // 清空原有数据
            for (int i = 0; i < 4; i++)
            {
                led_on_off[i] = "false"; // 将所有元素重置为空
            }

            // 遍历数组并存储每个值
            for (int i = 0; i < ledArray.size(); i++)
            {
                if (i < 4) // 确保不越界
                {
                    led_on_off[i] = ledArray[i].as<String>(); // 直接赋值
                }
            }
        }

        // 构造 4 位 01 字符串命令
        cmd = "";
        for (int i = 0; i < 4; i++)
        {
            cmd += (led_on_off[i] == "true") ? "1" : "0";
        }
        MQTT_Respond(String(topic), "success"); // 回复命令成功
        doSend = true;                          // 标记需要发送数据
    }
    else
    {
        MQTT_Respond(String(topic), "failure"); // 命令无效，回复失败
    }
}

// 搜索BLE设备回调
class BLE_MyAdvertisedDeviceCallbacks : public BLEAdvertisedDeviceCallbacks
{
public:
    void onResult(BLEAdvertisedDevice advertisedDevice)
    {
        if (advertisedDevice.haveName() && advertisedDevice.getName() == "ESP32-BLE")
        {
            advertisedDevice.getScan()->stop();                  // 停止当前扫描
            pServer = new BLEAdvertisedDevice(advertisedDevice); // 暂存设备
            doScan = false;
            doConnect = true; // 准备连接
            Serial.println("发现想要连接的设备");
        }
    }
};

// BLE客户端与服务器连接与断开回调功能
class BLE_MyClientCallbacks : public BLEClientCallbacks
{
public:
    void onConnect(BLEClient *pclient)
    {
        ble_name = pServer->getName().c_str(); // 获取蓝牙名称
        isConnected = true;
        Serial.println("连接设备成功");
        // 设置MTU大小
        if (pClient->setMTU(512))
        {
            Serial.println("MTU设置成功.");
        }
        else
        {
            Serial.println("MTU设置失败.");
        }
    }

    void onDisconnect(BLEClient *pclient)
    {
        isConnected = false;
        doScan = true;
        Serial.println("失去与设备的连接");

        if (pClient)
        {
            delete pClient; // 删除客户端对象
            pClient = nullptr;
        }
    }
};

// BLE 初始化函数
void BLE_Init()
{
    BLEDevice::init(""); // 初始化BLE设备

    BLEScan *pBLEScan = BLEDevice::getScan();
    pBLEScan->setAdvertisedDeviceCallbacks(new BLE_MyAdvertisedDeviceCallbacks()); // 设置设备扫描回调
    pBLEScan->setActiveScan(true);                                                 // 开启主动扫描
    pBLEScan->setInterval(100);                                                    // 扫描间隔
    pBLEScan->setWindow(80);                                                       // 扫描窗口
}

void BLE_Scan()
{
    // 开始扫描设备
    if (doScan)
    {
        MQTT_Report(); // 上传一次，确保更新ble的状态上云
        Serial.println("开始搜索设备");
        BLEDevice::getScan()->clearResults(); // 清除上次扫描结果
        BLEDevice::getScan()->start(0);       // 持续搜索设备
    }

    // 如果找到设备就尝试一次连接
    if (doConnect)

        if (BLE_Connect())
        {
            isConnected = true; // 设置连接状态为已连接
        }
        else
        {
            Serial.println("连接设备失败");
            doScan = true; // 重新开始扫描
        }
    doConnect = false; // 完成连接
}

// 用来连接设备获取其中的服务与特征
bool BLE_Connect()
{
    pClient = BLEDevice::createClient(); // 创建客户端实例
    if (!pClient)
    {
        Serial.println("创建客户端失败");
        return false;
    }

    pClient->setClientCallbacks(new BLE_MyClientCallbacks()); // 添加客户端连接与断开回调

    if (!pClient->connect(pServer))
    { // 尝试连接设备
        Serial.println("连接设备失败");
        delete pClient;
        pClient = nullptr;
        return false;
    }

    // 获取设备中的服务
    BLERemoteService *pRemoteService = pClient->getService(SERVICE_UUID);
    if (!pRemoteService)
    {
        Serial.println("获取服务失败");
        pClient->disconnect();
        return false;
    }

    // 获取服务中的特征
    pRemoteCharacteristic = pRemoteService->getCharacteristic(CHARACTERISTIC_UUID_TX);
    pRemoteCharacteristic_2 = pRemoteService->getCharacteristic(CHARACTERISTIC_UUID_RX);

    if (!pRemoteCharacteristic || !pRemoteCharacteristic_2)
    {
        Serial.println("获取特征失败");
        pClient->disconnect();
        return false;
    }

    Serial.println("获取特征成功");

    if (pRemoteCharacteristic->canRead())
    {
        Serial.printf("该特征值可以读取并且当前值为: %s\r\n", pRemoteCharacteristic->readValue().c_str());
    }

    if (pRemoteCharacteristic->canNotify())
    {
        pRemoteCharacteristic->registerForNotify(BLE_NotifyCallback); // 注册通知回调函数
    }

    return true;
}
// 发送命令到设备的函数
void BLE_Send_CMD()
{ // 如果已经连接，发送命令
    if (doSend)
    {
        if (isConnected && pRemoteCharacteristic_2 && pRemoteCharacteristic_2->canWrite())
        {
            Serial.printf("向特征写入消息: %s\r\n", cmd.c_str());
            pRemoteCharacteristic_2->writeValue(cmd.c_str(), cmd.length()); // 写入数据到设备
            doSend = false;                                                 // 重置 doSend 状态为 false
        }
    }
}
// BLE收到客户端推送的数据时的回调函数
void BLE_NotifyCallback(BLERemoteCharacteristic *pBLERemoteCharacteristic, uint8_t *pData, size_t length, bool isNotify)
{
    StaticJsonDocument<200> doc;
    DeserializationError error = deserializeJson(doc, pData, length);

    if (error)
    {
        Serial.print("解析JSON失败: ");
        Serial.println(error.f_str());
        return;
    }

    // 解析 LED 状态为字符串数组
    JsonArray ledArray = doc["led"].as<JsonArray>();
    for (int i = 0; i < 4 && i < ledArray.size(); i++)
    {
        led_state[i] = ledArray[i].as<const char *>();
    }

    // 解析温湿度
    data_temp = doc["temperature"];
    data_humi = doc["humidity"];

    // 打印 LED 状态
    Serial.print("接收到数据:\nLED状态: ");
    for (int i = 0; i < 4; i++)
    {
        Serial.printf("[%d]=%s ", i, led_state[i].c_str());
    }
    Serial.printf("\n温度: %.2f °C\n湿度: %.2f %%\n", data_temp, data_humi);
}

void setup()
{
    Serial.begin(115200);
    delay(100);  // 等待串口初始化
    WIFI_Init(); // 等待wifi连接
    MQTT_Init(); // 初始化MQTT尝试连接
    BLE_Init();  // 初始化BLE设备
}

void loop()
{
    BLE_Scan();  // 尝试扫描并连接BLE
    MQTT_Scan(); // 尝试扫描并连接云，如果没有连接将会一直client.loop

    long now = millis();
    if (now - last > 1000)
    { // 每 10 秒上报一次
        last = now;
        MQTT_Report();
        // MQTT_Send();
        BLE_Send_CMD(); // 发送命令
    }
}
