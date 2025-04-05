#include <BLEDevice.h>
#include <BLEServer.h>
#include <BLEUtils.h>
#include <BLE2902.h>
#include <DHT.h>
#include <DHT_U.h>

/******************* 温湿度 **********************/
#define DHTTYPE DHT11 // DHT 11
#define DHTPIN 2      // GPI02

DHT dht(DHTPIN, DHTTYPE, 15);
uint8_t flag_dht = 0;
uint8_t flag_led = 0;
uint8_t flag_send_dht = 0;
float dht_buf[2];

/*****************************************/

/******************* 蓝牙 **********************/
BLECharacteristic *pCharacteristic;
bool deviceConnected = false;
char BLEbuf[32] = {0};

#define SERVICE_UUID "6E400001-B5A3-F393-E0A9-E50E24DCCA9E" // UART service UUID
#define CHARACTERISTIC_UUID_RX "6E400002-B5A3-F393-E0A9-E50E24DCCA9E"
#define CHARACTERISTIC_UUID_TX "6E400003-B5A3-F393-E0A9-E50E24DCCA9E"

class MyServerCallbacks : public BLEServerCallbacks
{
    void onConnect(BLEServer *pServer) { deviceConnected = true; }
    void onDisconnect(BLEServer *pServer) { deviceConnected = false; }
};

class MyCallbacks : public BLECharacteristicCallbacks
{
    void onWrite(BLECharacteristic *pCharacteristic)
    {
        std::string rxValue = pCharacteristic->getValue();
        if (rxValue.length() > 0)
        {
            Serial.print("------> Received Value: ");
            for (int i = 0; i < rxValue.length(); i++)
            {
                Serial.print(rxValue[i]);
            }
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
/*****************************************/

/****************** 主程序 ***********************/
void setup()
{
    Serial.begin(115200);

    pinMode(12, OUTPUT);
    dht.begin();

    BLEDevice::init("ESP32-BLE");
    BLEServer *pServer = BLEDevice::createServer();
    pServer->setCallbacks(new MyServerCallbacks());

    BLEService *pService = pServer->createService(SERVICE_UUID);
    pCharacteristic = pService->createCharacteristic(CHARACTERISTIC_UUID_TX, BLECharacteristic::PROPERTY_NOTIFY);
    pCharacteristic->addDescriptor(new BLE2902());

    BLECharacteristic *pCharacteristic_RX = pService->createCharacteristic(CHARACTERISTIC_UUID_RX, BLECharacteristic::PROPERTY_WRITE);
    pCharacteristic_RX->setCallbacks(new MyCallbacks());

    pService->start();
    pServer->getAdvertising()->start();
    Serial.println("Waiting for client connection...");
}

void loop()
{
    if (deviceConnected)
    {
        if (flag_send_dht)
        {
            memset(BLEbuf, 0, 32);
            sprintf(BLEbuf, "湿度:%.2f, 温度:%.2f", dht_buf[0], dht_buf[1]);
            pCharacteristic->setValue(BLEbuf);
            pCharacteristic->notify(); // Send the value to the app
            Serial.print("*** Sent Value: ");
            Serial.println(BLEbuf);
            flag_send_dht = 0;
        }
    }

    if (flag_dht)
    {
        delay(2000); // DHT11 sensor read delay
        float h = dht.readHumidity();
        float t = dht.readTemperature();
        dht_buf[0] = h;
        dht_buf[1] = t;

        if (isnan(h) || isnan(t))
        {
            Serial.println("读取DHT传感器失败");
        }
        else
        {
            Serial.printf("湿度: %.2f %%\t 温度: %.2f °C\n", h, t);
            flag_send_dht = 1;
            flag_dht = 0;
        }
    }

    if (flag_led)
    {
        digitalWrite(12, HIGH); // Turn LED on
        delay(1000);            // Wait for a second
        digitalWrite(12, LOW);  // Turn LED off
        delay(1000);            // Wait for a second
        flag_led = 0;
    }
    delay(1000); // Main loop delay
}
