#include <BLEDevice.h>
#include <BLEServer.h>
#include <BLEUtils.h>
#include <BLE2902.h>
#include <DHT.h>
#include <DHT_U.h>
#include <ArduinoJson.h>

#define DHTTYPE DHT11 // 使用 DHT11 温湿度传感器
#define DHTPIN 2      // 温湿度传感器连接到 GPIO2

DHT dht(DHTPIN, DHTTYPE, 15); // 初始化 DHT11 传感器
uint8_t flag_dht = 0;         // 控制是否读取温湿度传感器的标志位
uint8_t flag_led = 0;         // 控制是否点亮LED的标志位
uint8_t flag_send_dht = 0;    // 控制是否发送温湿度数据的标志位
float dht_buf[2];             // 存储湿度和温度数据的数组

BLECharacteristic *pCharacteristic; // 用于发送数据的BLE特征
bool deviceConnected = false;       // 设备是否连接
char BLEbuf[32] = {0};              // 用于存储发送的蓝牙数据

#define SERVICE_UUID "6E400001-B5A3-F393-E0A9-E50E24DCCA9E"           // UART服务的UUID
#define CHARACTERISTIC_UUID_RX "6E400002-B5A3-F393-E0A9-E50E24DCCA9E" // 接收特征的UUID
#define CHARACTERISTIC_UUID_TX "6E400003-B5A3-F393-E0A9-E50E24DCCA9E" // 发送特征的UUID

// BLE服务器连接回调
class MyServerCallbacks : public BLEServerCallbacks
{
    void onConnect(BLEServer *pServer)
    {
        deviceConnected = true; // 设备连接时设置连接状态为真
    }
    void onDisconnect(BLEServer *pServer)
    {
        deviceConnected = false; // 设备断开时设置连接状态为假
    }
};

// BLE特征写入回调
class MyCallbacks : public BLECharacteristicCallbacks
{
    void onWrite(BLECharacteristic *pCharacteristic)
    {
        std::string rxValue = pCharacteristic->getValue(); // 获取写入的值
        if (rxValue.length() > 0)
        {
            Serial.print("------> Received Value: ");
            for (int i = 0; i < rxValue.length(); i++)
            {
                Serial.print(rxValue[i]); // 打印接收到的值
            }
            Serial.println();

            // 判断接收到的指令并设置标志位
            if (rxValue.find("00") != -1)
            {
                flag_dht = 1; // 设置读取DHT传感器的标志位
            }
            else if (rxValue.find("01") != -1)
            {
                flag_led = 1; // 设置点亮LED的标志位
            }
        }
    }
};

void setup()
{
    Serial.begin(115200); // 初始化串口通信

    pinMode(12, OUTPUT); // 设置GPIO12为输出模式，用于控制LED
    dht.begin();         // 初始化DHT传感器

    // 初始化BLE设备
    BLEDevice::init("ESP32-BLE");
    BLEServer *pServer = BLEDevice::createServer(); // 创建BLE服务器
    pServer->setCallbacks(new MyServerCallbacks()); // 设置BLE服务器回调

    BLEService *pService = pServer->createService(SERVICE_UUID);                                                  // 创建UART服务
    pCharacteristic = pService->createCharacteristic(CHARACTERISTIC_UUID_TX, BLECharacteristic::PROPERTY_NOTIFY); // 创建发送特征
    pCharacteristic->addDescriptor(new BLE2902());                                                                // 添加BLE2902描述符，支持通知

    BLECharacteristic *pCharacteristic_RX = pService->createCharacteristic(CHARACTERISTIC_UUID_RX, BLECharacteristic::PROPERTY_WRITE); // 创建接收特征
    pCharacteristic_RX->setCallbacks(new MyCallbacks());                                                                               // 设置接收特征的回调

    pService->start();                  // 启动服务
    pServer->getAdvertising()->start(); // 开始广播
    Serial.println("等待节点连接...");
}

void loop()
{
    // 如果设备已经连接，且需要发送DHT数据
    if (deviceConnected)
    {
        if (flag_send_dht)
        {
            // 获取LED状态（假设 flag_led 为 1 时表示 LED 点亮，0 表示熄灭）
            const char *ledStatus = (flag_led == 1) ? "on" : "off";

            // 创建 JSON 数据包
            StaticJsonDocument<200> doc;
            doc["temperature"] = dht_buf[1]; // 温度
            doc["humidity"] = dht_buf[0];    // 湿度
            doc["led"] = ledStatus;          // LED 状态

            // 将 JSON 数据包转换为字符串
            String jsonString;
            serializeJson(doc, jsonString);

            // 发送 JSON 字符串
            pCharacteristic->setValue(jsonString.c_str());
            pCharacteristic->notify(); // 通知客户端接收数据

            // 打印发送的 JSON 数据
            Serial.print("*** Sent JSON Data: ");
            Serial.println(jsonString);

            flag_send_dht = 0; // 重置标志位
        }
    }

    // 如果需要读取温湿度传感器
    if (flag_dht)
    {
        delay(2000); // 等待2秒以便传感器读取

        float h = dht.readHumidity();    // 读取湿度
        float t = dht.readTemperature(); // 读取温度
        dht_buf[0] = h;                  // 保存湿度数据
        dht_buf[1] = t;                  // 保存温度数据

        if (isnan(h) || isnan(t))
        { // 如果读取失败
            Serial.println("读取DHT传感器失败");
        }
        else
        {
            // 打印读取的值
            Serial.printf("湿度: %.2f %%\t 温度: %.2f °C\n", h, t);
            flag_send_dht = 1; // 设置发送数据的标志位
            flag_dht = 0;      // 重置读取传感器的标志位
        }
    }

    // 如果需要点亮LED
    if (flag_led)
    {
        digitalWrite(12, HIGH); // 点亮LED
        delay(1000);            // 等待1秒
        digitalWrite(12, LOW);  // 熄灭LED
        delay(1000);            // 等待1秒
        flag_led = 0;           // 重置LED控制标志位
    }

    delay(1000); // 主循环延时1秒
}
