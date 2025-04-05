#include <BLEDevice.h>
#include <BLEServer.h>
#include <BLEUtils.h>
#include <BLE2902.h>

// 蓝牙相关定义
BLECharacteristic *pCharacteristic;
bool deviceConnected = false;
char BLEbuf[32] = {0};
uint32_t cnt = 0;

#define SERVICE_UUID "6E400001-B5A3-F393-E0A9-E50E24DCCA9E"           // UART服务UUID
#define CHARACTERISTIC_UUID_RX "6E400002-B5A3-F393-E0A9-E50E24DCCA9E" // 接收特征UUID
#define CHARACTERISTIC_UUID_TX "6E400003-B5A3-F393-E0A9-E50E24DCCA9E" // 发送特征UUID

boolean doScan = true;     // 是否开始扫描设备
boolean doConnect = false; // 是否连接设备
boolean connected = false; // 设备是否已连接

BLEAdvertisedDevice *pServer = nullptr;                     // 存储找到的设备
BLERemoteCharacteristic *pRemoteCharacteristic = nullptr;   // 存储远程读取特征
BLERemoteCharacteristic *pRemoteCharacteristic_2 = nullptr; // 存储远程写入特征
BLEClient *pClient = nullptr;                               // 客户端实例

// 搜索到设备时回调功能
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

// 客户端与服务器连接与断开回调功能
class MyClientCallback : public BLEClientCallbacks
{
public:
    void onConnect(BLEClient *pclient)
    {
        connected = true;
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
        connected = false;
        doScan = true;
        Serial.println("失去与设备的连接");

        if (pClient)
        {
            delete pClient; // 删除客户端对象
            pClient = nullptr;
        }
    }
};

// 收到服务推送的数据时的回调函数
void NotifyCallback(BLERemoteCharacteristic *pBLERemoteCharacteristic, uint8_t *pData, size_t length, bool isNotify)
{
    int numValues = length / sizeof(int16_t); // 计算接收到的int16_t数量
    int16_t *dataArr = (int16_t *)pData;      // 将数据转换为int16_t类型的数组
    Serial.printf("接收到 %d 个int16_t值:\n", numValues);
    Serial.printf("值: %s\n", dataArr); // 打印收到的值
}

// 用来连接设备获取其中的服务与特征
bool ConnectToServer()
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
        Serial.println("获取特性失败");
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

void setup()
{
    Serial.begin(115200);

    // 初始化BLE设备
    BLEDevice::init("");

    BLEScan *pBLEScan = BLEDevice::getScan();
    pBLEScan->setAdvertisedDeviceCallbacks(new MyAdvertisedDeviceCallbacks()); // 设置设备扫描回调
    pBLEScan->setActiveScan(true);                                             // 开启主动扫描
    pBLEScan->setInterval(100);                                                // 扫描间隔
    pBLEScan->setWindow(80);                                                   // 扫描窗口
}

int count = 0;
void loop()
{
    // 开始扫描设备
    if (doScan)
    {
        Serial.println("开始搜索设备");
        BLEDevice::getScan()->clearResults(); // 清除上次扫描结果
        BLEDevice::getScan()->start(0);       // 持续搜索设备
    }

    // 如果找到设备就尝试连接
    if (doConnect)
    {
        if (ConnectToServer())
        {
            connected = true; // 设置连接状态
        }
        else
        {
            doScan = true; // 重新开始扫描
        }
        doConnect = false;
    }

    // 如果已经连接，可以向设备发送数据
    if (connected && pRemoteCharacteristic && pRemoteCharacteristic_2 && pRemoteCharacteristic_2->canWrite())
    {
        String newValue = "10"; // 默认发送的值
        if (count == 1)
        {
            newValue = "00"; // 发送其他值
        }
        else if (count == 2)
        {
            newValue = "01"; // 发送其他值
        }

        Serial.printf("向特征写入消息: %s\r\n", newValue.c_str());
        pRemoteCharacteristic_2->writeValue(newValue.c_str(), newValue.length()); // 写入数据到设备

        delay(3500); // 控制发送间隔
        count++;     // 增加计数
        if (count > 2)
        {
            count = 0; // 重置计数
        }
    }
}
