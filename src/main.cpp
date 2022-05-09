#include <Arduino.h>
#include <BLEDevice.h>
#include <BLEServer.h>
#include <BLE2902.h>
#include <Ticker.h>

#define DEVICE_NAME "Calm Flower"

#define MODE_SERVICE "d6694b21-880d-4b4a-adae-256cc1f01e7b"
#define MODE_CHARACTERISTIC "20103538-ff6b-4c7f-9aba-36a32be2c7c2"
#define COLOR1_CHARACTERISTIC "5903b942-0ce7-42c2-a29f-ff434521fbe2"
#define COLOR2_CHARACTERISTIC "f42275ed-b762-4e9d-b0c4-2e01d37ae2fd"

#define BATTERY_SERVICE (uint16_t) 0x180F
#define BATTERY_CHARACTERISTIC (uint16_t) 0x2A19

typedef enum {
    STATIC,
    RAINBOW,
    STROBE,
} Mode;

BLEServer *server = nullptr;
BLECharacteristic *batteryCharacteristic = nullptr;
BLECharacteristic *modeCharacteristic = nullptr;
BLECharacteristic *color1Characteristic = nullptr;
uint8_t connectedCount = 0;

Ticker batteryTicker;

void readBattery() {
    auto batteryLevel = (uint8_t) (analogRead(4) * 100 / 4095);
    batteryCharacteristic->setValue(&batteryLevel, 1);
    batteryCharacteristic->notify();
}

class MyServerCallbacks : public BLEServerCallbacks {
    void onConnect(BLEServer *pServer) override {
        connectedCount++;

        Serial.print("(");
        Serial.print(connectedCount);
        Serial.println(") Device connected");

        pServer->getAdvertising()->start();
    };

    void onDisconnect(BLEServer *pServer) override {
        connectedCount--;

        Serial.print("(");
        Serial.print(connectedCount);
        Serial.println(") Device disconnected");
    }
};

class ModeCharacteristicCallbacks : public BLECharacteristicCallbacks {
public:
    void onWrite(BLECharacteristic *pCharacteristic) override {
        uint8_t *mode = pCharacteristic->getData();

        Serial.print("Mode changed: ");
        Serial.println(*mode);

        pCharacteristic->notify();
    }
};

class Color1CharacteristicCallbacks : public BLECharacteristicCallbacks {
public:
    void onWrite(BLECharacteristic *pCharacteristic) override {
        auto *data = (uint16_t *) pCharacteristic->getData();

        Serial.print("Color1 changed: ");
        Serial.print(data[0]);
        Serial.print(" ");
        Serial.print(data[1]);
        Serial.print(" ");
        Serial.println(data[2]);

        pCharacteristic->notify();
    }
};

void setup() {
    Serial.begin(115200);

    BLEDevice::init(DEVICE_NAME);
//    BLEDevice::setEncryptionLevel(ESP_BLE_SEC_ENCRYPT);
//    BLEDevice::setSecurityCallbacks();
//    BLESecurity *pSecurity = new BLESecurity();
//    pSecurity->setStaticPIN(123456);

    server = BLEDevice::createServer();
    server->setCallbacks(new MyServerCallbacks());

    auto batteryService = server->createService(BATTERY_SERVICE);
    batteryCharacteristic = batteryService->createCharacteristic(
            BATTERY_CHARACTERISTIC,
            BLECharacteristic::PROPERTY_READ |
            BLECharacteristic::PROPERTY_NOTIFY
    );
    batteryCharacteristic->addDescriptor(new BLE2902());
    batteryService->start();

    auto modeService = server->createService(MODE_SERVICE);

    modeCharacteristic = modeService->createCharacteristic(
            MODE_CHARACTERISTIC,
            BLECharacteristic::PROPERTY_READ |
            BLECharacteristic::PROPERTY_WRITE |
            BLECharacteristic::PROPERTY_WRITE_NR |
            BLECharacteristic::PROPERTY_NOTIFY
    );
    modeCharacteristic->addDescriptor(new BLE2902());
    uint8_t defaultMode = STATIC;
    modeCharacteristic->setValue(&defaultMode, 1);
    modeCharacteristic->setCallbacks(new ModeCharacteristicCallbacks());

    color1Characteristic = modeService->createCharacteristic(
            COLOR1_CHARACTERISTIC,
            BLECharacteristic::PROPERTY_READ |
            BLECharacteristic::PROPERTY_WRITE |
            BLECharacteristic::PROPERTY_WRITE_NR |
            BLECharacteristic::PROPERTY_NOTIFY
    );
    color1Characteristic->addDescriptor(new BLE2902());

    uint16_t value[] = {0, 1023, 0};
    color1Characteristic->setValue((uint8_t *) value, 6);

    color1Characteristic->setCallbacks(new Color1CharacteristicCallbacks());

    modeService->start();

    server->getAdvertising()->addServiceUUID(MODE_SERVICE);
    // TODO: https://specificationrefs.bluetooth.com/assigned-values/Appearance%20Values.pdf
//    server->getAdvertising()->setAppearance();
    server->getAdvertising()->setScanResponse(true);
    server->getAdvertising()->setMinPreferred(0x06);  // functions that help with iPhone connections issue
    server->getAdvertising()->setMinPreferred(0x12);
    server->getAdvertising()->start();

    batteryTicker.attach(60, readBattery);
    readBattery();

    Serial.println("Started");
}

void loop() {
}