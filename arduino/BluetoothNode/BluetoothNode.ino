
#include <BLEDevice.h>
#include <BLEServer.h>
#include <BLEUtils.h>
#include <BLE2902.h>
// 温湿度
#include <DHT.h>
#include <DHT_U.h>

/*******************温湿度**********************/
// 定义DHT传感器类型和连接的GPIO
#define DHTTYPE DHT11 // DHT 11
#define DHTPIN 2      // GPI02

// 初始化DHT传感器
DHT dht(DHTPIN, DHTTYPE, 15);

uint8_t flag_dht = 0;
uint8_t flag_led = 0;
uint8_t flag_send_dht = 0;
float dht_buf[2];

/*****************************************/

/*******************蓝牙**********************/
BLECharacteristic *pCharacteristic;
bool deviceConnected = false;
char BLEbuf[32] = {0};
uint32_t cnt = 0;

#define SERVICE_UUID "6E400001-B5A3-F393-E0A9-E50E24DCCA9E" // UART service UUID
#define CHARACTERISTIC_UUID_RX "6E400002-B5A3-F393-E0A9-E50E24DCCA9E"
#define CHARACTERISTIC_UUID_TX "6E400003-B5A3-F393-E0A9-E50E24DCCA9E"

class MyServerCallbacks : public BLEServerCallbacks
{
    void onConnect(BLEServer *pServer)
    {
        deviceConnected = true;
    };

    void onDisconnect(BLEServer *pServer)
    {
        deviceConnected = false;
    }
};

class MyCallbacks : public BLECharacteristicCallbacks
{
    void onWrite(BLECharacteristic *pCharacteristic)
    {
        std::string rxValue = pCharacteristic->getValue();

        if (rxValue.length() > 0)
        {
            Serial.print("------>Received Value: ");

            for (int i = 0; i < rxValue.length(); i++)
            {
                Serial.print(rxValue[i]);
            }
            Serial.println();

            if (rxValue.find("10") != -1)
            {
                Serial.print("Rx comment 0!");
            }
            else if (rxValue.find("00") != -1)
            {
                Serial.print("Rx comment 0!");
                flag_dht = 1;
            }
            else if (rxValue.find("01") != -1)
            {
                flag_led = 1;
            }
            Serial.println();
        }
    }
};
/*****************************************/

/******************主程序***********************/
void setup()
{
    /************************************************/
    /*              串口初始化                       */
    /************************************************/
    Serial.begin(115200);

    /************************************************/
    /*              温湿度和LED初始化                       */
    /************************************************/
    // initialize digital pin LED_BUILTIN as an output.
    pinMode(12, OUTPUT);
    pinMode(12, OUTPUT);

    Serial.println("DHTl1 test!");
    dht.begin();

    /************************************************/
    /*              蓝牙初始化                       */
    /************************************************/
    // Create the BLE Device
    BLEDevice::init("ESP32-BLE");

    // Create the BLE Server
    BLEServer *pServer = BLEDevice::createServer();
    pServer->setCallbacks(new MyServerCallbacks());

    // Create the BLE Service
    BLEService *pService = pServer->createService(SERVICE_UUID);

    // Create a BLE Characteristic
    pCharacteristic = pService->createCharacteristic(CHARACTERISTIC_UUID_TX, BLECharacteristic::PROPERTY_NOTIFY);

    pCharacteristic->addDescriptor(new BLE2902());

    BLECharacteristic *pCharacteristic = pService->createCharacteristic(CHARACTERISTIC_UUID_RX, BLECharacteristic::PROPERTY_WRITE);

    pCharacteristic->setCallbacks(new MyCallbacks());

    // Start the service
    pService->start();

    // Start advertising
    pServer->getAdvertising()->start();
    Serial.println("Waiting a client connection to notify...");
}

void loop()
{
    if (deviceConnected)
    {

        // memcpy(BLEbuf, (char*)"Hello BLE APP!", 32);
        if (flag_send_dht)
        {
            memset(BLEbuf, 0, 32);
            sprintf(BLEbuf, "湿度:%2f,温度:%2f", dht_buf[0], dht_buf[1]);
            pCharacteristic->setValue(BLEbuf);

            pCharacteristic->notify(); // Send the value to the app!
            Serial.print("*** Sent Value: ");
            Serial.print(BLEbuf);
            Serial.println(" ***");
            flag_send_dht = 0;
        }
    }
    if (flag_dht)
    {

        // 等待几秒钟，DHT11的读取速度很慢
        delay(2000);
        // 读取湿度
        float h = dht.readHumidity();
        // 读取摄氏度
        float t = dht.readTemperature();

        dht_buf[0] = h;
        dht_buf[1] = t;
        // 检查数据是否读取正常
        if (isnan(h) || isnan(t))
        {
            Serial.println("读取DHT传感器失败");
            return;
        }
        else
        {
            // 计算华氏度
            float f = dht.computeHeatIndex(t, h, false);
            Serial.print("湿度:");
            Serial.print(h);
            Serial.print(" %\t");
            Serial.print("温度:");
            Serial.print(t);
            Serial.print("°c");
            Serial.print(f);
            Serial.println(" °F");
            flag_send_dht = 1;
            flag_dht = 0;
        }
    }
    if (flag_led)
    {
        digitalWrite(12, HIGH); // turn the LED on (HIGH is the voltage level)
        delay(1000);            // wait for a second
        digitalWrite(12, LOW);  // turn the LED off by making the voltage LOW
        delay(1000);            // wait for a second
        flag_led = 0;
    }
    delay(1000);
}
