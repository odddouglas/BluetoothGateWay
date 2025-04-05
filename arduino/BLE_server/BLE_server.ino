#include <BLEDevice.h>
#include <BLEServer.h>
#include <BLEUtils.h>
#include <BLE2902.h>
#include <ArduinoJson.h>

// 蓝牙相关定义
BLECharacteristic *pCharacteristic;

#define SERVICE_UUID "6E400001-B5A3-F393-E0A9-E50E24DCCA9E"           // UART服务UUID
#define CHARACTERISTIC_UUID_RX "6E400002-B5A3-F393-E0A9-E50E24DCCA9E" // 接收特征UUID
#define CHARACTERISTIC_UUID_TX "6E400003-B5A3-F393-E0A9-E50E24DCCA9E" // 发送特征UUID

boolean doScan = true;       // 是否开始扫描设备
boolean doConnect = false;   // 是否连接设备
boolean isConnected = false; // 设备是否已连接
boolean doSend = true;       // 是否发送命令

BLEAdvertisedDevice *pServer = nullptr;                     // 存储找到的设备
BLERemoteCharacteristic *pRemoteCharacteristic = nullptr;   // 存储远程读取特征
BLERemoteCharacteristic *pRemoteCharacteristic_2 = nullptr; // 存储远程写入特征
BLEClient *pClient = nullptr;                               // 客户端实例

float data_temp = 0.0; // 温湿度数据变量
float data_humi = 0.0;
bool led_state = false; // LED 状态变量

bool state = false; // 测试命令的
String cmd = "";    // 命令

// 搜索BLE设备回调
class MyAdvertisedDeviceCallbacks : public BLEAdvertisedDeviceCallbacks
{
public:
    void onResult(BLEAdvertisedDevice advertisedDevice)
    {
        if (advertisedDevice.haveName() && advertisedDevice.getName() == "ESP32-BLE")
        {
            advertisedDevice.getScan()->stop();                  // 停止当前扫描
            pServer = new BLEAdvertisedDevice(advertisedDevice); // 暂存设备
            doScan = false;
            doConnect = true;
            Serial.println("发现想要连接的设备");
        }
    }
};

// BLE客户端与服务器连接与断开回调功能
class MyClientCallback : public BLEClientCallbacks
{
public:
    void onConnect(BLEClient *pclient)
    {
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

// BLE收到客户端推送的数据时的回调函数
void NotifyCallback(BLERemoteCharacteristic *pBLERemoteCharacteristic, uint8_t *pData, size_t length, bool isNotify)
{
    // 创建 StaticJsonDocument 对象
    StaticJsonDocument<200> doc;

    // 尝试将接收到的数据解析为 JSON 格式
    DeserializationError error = deserializeJson(doc, pData, length);

    // 如果解析失败，输出错误信息
    if (error)
    {
        Serial.print("解析JSON失败: ");
        Serial.println(error.f_str());
        return;
    }

    // 从 JSON 中提取各个字段
    led_state = doc["led"];         // LED 状态（布尔值）
    data_temp = doc["temperature"]; // 温度
    data_humi = doc["humidity"];    // 湿度

    // 打印解析结果
    Serial.printf("接收到数据:\nLED: %s\n温度: %.2f °C\n湿度: %.2f %%\n", led_state ? "on" : "off", data_temp, data_humi);
}

// 用来连接设备获取其中的服务与特征
bool connectToServer()
{
    pClient = BLEDevice::createClient(); // 创建客户端实例
    if (!pClient)
    {
        Serial.println("创建客户端失败");
        return false;
    }

    pClient->setClientCallbacks(new MyClientCallback()); // 添加客户端连接与断开回调

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
        pRemoteCharacteristic->registerForNotify(NotifyCallback); // 注册通知回调函数
    }

    return true;
}

// BLE 初始化函数
void BLE_Init()
{
    BLEDevice::init(""); // 初始化BLE设备

    BLEScan *pBLEScan = BLEDevice::getScan();
    pBLEScan->setAdvertisedDeviceCallbacks(new MyAdvertisedDeviceCallbacks()); // 设置设备扫描回调
    pBLEScan->setActiveScan(true);                                             // 开启主动扫描
    pBLEScan->setInterval(100);                                                // 扫描间隔
    pBLEScan->setWindow(80);                                                   // 扫描窗口
}
void BLE_Scan()
{
    // 开始扫描设备
    if (doScan)
    {
        Serial.println("开始搜索设备");
        BLEDevice::getScan()->clearResults(); // 清除上次扫描结果
        BLEDevice::getScan()->start(0);       // 持续搜索设备
    }

    // 如果找到设备就尝试一次连接
    if (doConnect)
    {
        if (connectToServer())
        {
            isConnected = true; // 设置连接状态为已连接
        }
        else
        {
            doScan = true; // 重新开始扫描
        }
        doConnect = false;
    }
}
// 发送命令到设备的函数
void sendCommand()
{
    state = !state;
    cmd = (state ? "ON" : "OFF"); // 更新命令
    if (isConnected && pRemoteCharacteristic_2 && pRemoteCharacteristic_2->canWrite())
    {
        Serial.printf("向特征写入消息: %s\r\n", cmd.c_str());
        pRemoteCharacteristic_2->writeValue(cmd.c_str(), cmd.length()); // 写入数据到设备
        //doSend = false;                                                 // 重置 doSend 状态为 false
    }
}

void setup()
{
    Serial.begin(115200);
    BLE_Init(); // 初始化BLE设备
    BLE_Scan(); // 尝试扫描并连接BLE
}

void loop()
{
    // 如果已经连接，发送命令
    if (isConnected && doSend)
    {
        sendCommand(); // 调用 sendCommand 函数发送命令
        delay(3500);   // 控制发送间隔
    }
}
