#include <Arduino.h>
#include <BLEDevice.h>
#include <BLEServer.h>
#include <BLE2902.h>
#include <Ticker.h>

#define RED_PIN 16
#define GREEN_PIN 17
#define BLUE_PIN 19
#define VOLTAGE_PIN 36

#define DEVICE_NAME "Calm Flower"

#define MODE_SERVICE "d6694b21-880d-4b4a-adae-256cc1f01e7b"
#define MODE_CHARACTERISTIC "20103538-ff6b-4c7f-9aba-36a32be2c7c2"
#define COLOR1_CHARACTERISTIC "5903b942-0ce7-42c2-a29f-ff434521fbe2"
//#define COLOR2_CHARACTERISTIC "f42275ed-b762-4e9d-b0c4-2e01d37ae2fd"

#define BATTERY_SERVICE (uint16_t) 0x180F
#define BATTERY_CHARACTERISTIC (uint16_t) 0x2A19

#define RED_CHANNEL 0
#define GREEN_CHANNEL 1
#define BLUE_CHANNEL 2

typedef enum {
    STATIC,
    RAINBOW,
    STROBE,
} Mode;

BLEServer *server = nullptr;
BLECharacteristic *batteryCharacteristic = nullptr;
BLECharacteristic *modeCharacteristic = nullptr;
BLECharacteristic *color1Characteristic = nullptr;

Ticker batteryTicker;

void readBattery() {
    auto batteryLevel = (uint8_t) (analogRead(VOLTAGE_PIN) * 100 / 4095);
    batteryCharacteristic->setValue(&batteryLevel, 1);
    batteryCharacteristic->notify();
}

void setupLed() {
    ledcSetup(RED_CHANNEL, 10000, 10);
    ledcSetup(GREEN_CHANNEL, 10000, 10);
    ledcSetup(BLUE_CHANNEL, 10000, 10);

    ledcAttachPin(RED_PIN, RED_CHANNEL);
    ledcAttachPin(GREEN_PIN, GREEN_CHANNEL);
    ledcAttachPin(BLUE_PIN, BLUE_CHANNEL);
}

class MyServerCallbacks : public BLEServerCallbacks {
protected:
    uint8_t connectedCount = 0;
public:
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

        pCharacteristic->notify();

        ledcWrite(RED_CHANNEL, data[0]);
        ledcWrite(GREEN_CHANNEL, data[1]);
        ledcWrite(BLUE_CHANNEL, data[2]);

        Serial.print("Color1 changed: ");
        Serial.print(data[0]);
        Serial.print(" ");
        Serial.print(data[1]);
        Serial.print(" ");
        Serial.println(data[2]);
    }
};

void setup() {
    Serial.begin(115200);

    BLEDevice::init(DEVICE_NAME);
    // TODO: security
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

    setupLed();

    Serial.println("Started");
}

void loop() {
}