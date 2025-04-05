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
uint8_t flag_dht = 0;
uint8_t flag_led = 0;
uint8_t flag_send_dht = 0;

float data_temp = 0.0; // 温湿度数据变量
float data_humi = 0.0;

bool led_state = false; // LED 状态变量

BLECharacteristic *pCharacteristic;
bool deviceConnected = false;
char BLEbuf[32] = {0};

#define SERVICE_UUID "6E400001-B5A3-F393-E0A9-E50E24DCCA9E"
#define CHARACTERISTIC_UUID_RX "6E400002-B5A3-F393-E0A9-E50E24DCCA9E"
#define CHARACTERISTIC_UUID_TX "6E400003-B5A3-F393-E0A9-E50E24DCCA9E"

// BLE服务器连接回调
class MyServerCallbacks : public BLEServerCallbacks
{
    void onConnect(BLEServer *pServer) { deviceConnected = true; }
    void onDisconnect(BLEServer *pServer) { deviceConnected = false; }
};

// BLE特征写入回调
class MyCallbacks : public BLECharacteristicCallbacks
{
    void onWrite(BLECharacteristic *pCharacteristic)
    {
        std::string rxValue = pCharacteristic->getValue();
        if (rxValue.length() > 0)
        {
            Serial.print("------> Received Value: ");
            for (int i = 0; i < rxValue.length(); i++)
                Serial.print(rxValue[i]);
            Serial.println();

            if (rxValue.find("00") != -1)
            {
                flag_dht = 1;
            }
            else if (rxValue.find("01") != -1)
            {
                flag_led = 1;
            }
        }
    }
};

// BLE 初始化函数
void BLE_Init()
{
    BLEDevice::init("ESP32-BLE");
    BLEServer *pServer = BLEDevice::createServer();
    pServer->setCallbacks(new MyServerCallbacks());

    BLEService *pService = pServer->createService(SERVICE_UUID);

    pCharacteristic = pService->createCharacteristic(
        CHARACTERISTIC_UUID_TX,
        BLECharacteristic::PROPERTY_NOTIFY);
    pCharacteristic->addDescriptor(new BLE2902());

    BLECharacteristic *pCharacteristic_RX = pService->createCharacteristic(
        CHARACTERISTIC_UUID_RX,
        BLECharacteristic::PROPERTY_WRITE);
    pCharacteristic_RX->setCallbacks(new MyCallbacks());

    pService->start();
    pServer->getAdvertising()->start();

    Serial.println("等待节点连接...");
}

// 发送数据的函数
// 发送数据的函数
void sendData()
{
    if (deviceConnected && flag_send_dht)
    {
        StaticJsonDocument<200> doc;
        doc["temperature"] = data_temp;
        doc["humidity"] = data_humi;
        doc["led"] = led_state; // 直接使用布尔值

        String jsonString;
        serializeJson(doc, jsonString);

        pCharacteristic->setValue(jsonString.c_str());
        pCharacteristic->notify();

        Serial.print("*** Sent JSON Data: ");
        Serial.println(jsonString);

        flag_send_dht = 0;
    }
}

// 读取DHT传感器的函数
void DHT_Read()
{
    if (flag_dht)
    {
        delay(2000);

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

            flag_send_dht = 1;
            flag_dht = 0;
        }
    }
}

void setup()
{
    Serial.begin(115200);

    pinMode(12, OUTPUT);
    dht.begin();
    BLE_Init(); // 初始化BLE

}

void loop()
{
    sendData(); // 发送数据
    DHT_Read(); // 读取DHT传感器
    if (flag_led)
    {
        digitalWrite(12, HIGH);
        led_state = true;
        delay(1000);

        digitalWrite(12, LOW);
        led_state = false;
        delay(1000);

        flag_led = 0;
    }

    delay(1000);
}
