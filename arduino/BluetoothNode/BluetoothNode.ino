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

// 控制标志位

bool isConnected = false; // 是否蓝牙连接

float data_temp = 0.0; // 温湿度数据变量
float data_humi = 0.0;
bool led_state[4] = {false, false, false, false};

BLECharacteristic *pCharacteristic;

#define SERVICE_UUID "6E400001-B5A3-F393-E0A9-E50E24DCCA9E"
#define CHARACTERISTIC_UUID_RX "6E400002-B5A3-F393-E0A9-E50E24DCCA9E"
#define CHARACTERISTIC_UUID_TX "6E400003-B5A3-F393-E0A9-E50E24DCCA9E"

// BLE服务器连接回调
class BLE_MyServer_Callbacks : public BLEServerCallbacks
{
    void onConnect(BLEServer *pServer) { isConnected = true; }
    void onDisconnect(BLEServer *pServer) { isConnected = false; }
};

// BLE特征写入回调
class BLE_Characteristic_RX_Callbacks : public BLECharacteristicCallbacks
{
    void onWrite(BLECharacteristic *pCharacteristic)
    {
        String rxValueString = pCharacteristic->getValue(); // Get as Arduino String
        std::string rxValue(rxValueString.c_str()); // Convert to std::string
        if (rxValue.length() > 0)
        {
            Serial.print("------> Received Value: ");
            for (int i = 0; i < rxValue.length(); i++)
                Serial.print(rxValue[i]);

            // 确保接收到的命令是四位字符
            if (rxValue.length() == 4)
            {
                // 解析命令并设置LED状态
                for (int i = 0; i < 4; i++)
                {
                    if (rxValue[i] == '1')
                    {
                        led_state[i] = true;
                        ; // 点亮对应的LED
                    }
                    else if (rxValue[i] == '0')
                    {
                        led_state[i] = false; // 熄灭对应的LED
                    }
                }

                // 打印每个LED的状态
                for (int i = 0; i < 4; i++)
                {
                    Serial.printf("LED %d is %s\n", i + 1, led_state[i] ? "ON" : "OFF");
                }
            }
        }
    }
};

// BLE 初始化函数
void BLE_Init()
{
    BLEDevice::init("ESP32-BLE");
    BLEServer *pServer = BLEDevice::createServer();
    pServer->setCallbacks(new BLE_MyServer_Callbacks());

    BLEService *pService = pServer->createService(SERVICE_UUID);

    pCharacteristic = pService->createCharacteristic(
        CHARACTERISTIC_UUID_TX,
        BLECharacteristic::PROPERTY_NOTIFY);
    pCharacteristic->addDescriptor(new BLE2902());

    BLECharacteristic *pCharacteristic_RX = pService->createCharacteristic(
        CHARACTERISTIC_UUID_RX,
        BLECharacteristic::PROPERTY_WRITE);
    pCharacteristic_RX->setCallbacks(new BLE_Characteristic_RX_Callbacks());

    pService->start();
    pServer->getAdvertising()->start();

    Serial.println("等待节点连接...");
}

// 发送数据的函数
void BLE_SendData()
{
    if (isConnected)
    {

        StaticJsonDocument<200> doc;
        doc["temperature"] = data_temp;
        doc["humidity"] = data_humi;

        // 转换 bool 数组为 StringList
        JsonArray ledArray = doc.createNestedArray("led");
        for (int i = 0; i < 4; i++)
        {
            ledArray.add(led_state[i] ? "true" : "false");
        }

        String jsonString;
        serializeJson(doc, jsonString);

        pCharacteristic->setValue(jsonString.c_str());
        pCharacteristic->notify();

        Serial.print("*** Sent JSON Data: ");
        Serial.println(jsonString);
    }
}

// 读取DHT传感器的函数
void DHT_Read()
{
    float h = dht.readHumidity();
    float t = dht.readTemperature();

    if (isnan(h) || isnan(t))
    {
        Serial.println("读取DHT传感器失败");
    }
    else
    {
        data_humi = h;
        data_temp = t;
        Serial.printf("湿度: %.2f %%\t 温度: %.2f °C\n", data_humi, data_temp);
    }
}

void setup()
{
    Serial.begin(115200);

    pinMode(12, OUTPUT);
    pinMode(13, OUTPUT);
    pinMode(18, OUTPUT);
    pinMode(19, OUTPUT);
    dht.begin();
    delay(2000); // DHT传感器的稳定时间
    BLE_Init();  // 初始化BLE
}

void loop()
{
    DHT_Read();     // 读取DHT传感器
    BLE_SendData(); // 发送数据

    digitalWrite(12, led_state[0]); // 点亮LED
    digitalWrite(13, led_state[1]); // 点亮LED
    digitalWrite(18, led_state[2]); // 点亮LED
    digitalWrite(19, led_state[3]); // 点亮LED

    delay(1000); // 延迟
}
