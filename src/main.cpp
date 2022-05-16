#include <Arduino.h>
#include <BLEDevice.h>
#include <BLEServer.h>
#include <BLE2902.h>
#include <Ticker.h>
#include <WiFi.h>
#include <ArduinoOTA.h>

#define RED_PIN 16
#define GREEN_PIN 17
#define BLUE_PIN 19
#define VOLTAGE_PIN 36

#define DEVICE_NAME "Legendary Invention"

#define MODE_SERVICE "d6694b21-880d-4b4a-adae-256cc1f01e7b"
#define MODE_CHARACTERISTIC "20103538-ff6b-4c7f-9aba-36a32be2c7c2"
#define COLOR1_CHARACTERISTIC "5903b942-0ce7-42c2-a29f-ff434521fbe2"
#define TURN_ON_CHARACTERISTIC "c9af1949-4275-46ec-9d63-f01fe45e9477"
#define SPEED_CHARACTERISTIC "74d51f60-ed42-4f82-b189-0fab7ffa7cd9"
//#define COLOR2_CHARACTERISTIC "f42275ed-b762-4e9d-b0c4-2e01d37ae2fd"

#define BATTERY_SERVICE (uint16_t) 0x180F
#define BATTERY_CHARACTERISTIC (uint16_t) 0x2A19

#define OTA_SERVICE "e3414eb0-bfa8-41f6-a3ee-db0b722e5807"
#define OTA_CHARACTERISTIC "1e2b6f32-a786-441c-acc9-6e2e5637cfb3"

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
BLECharacteristic *turnOnCharacteristic = nullptr;
BLECharacteristic *speedCharacteristic = nullptr;

BLECharacteristic *otaCharacteristic = nullptr;

Ticker batteryTicker;

uint8_t mode = STATIC;
uint16_t red = 0;
uint16_t green = 0;
uint16_t blue = 1023;
uint8_t turnOn = 1;
uint8_t speed = 100;

uint8_t ota = 0;

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
        auto *data = pCharacteristic->getData();

        pCharacteristic->notify();

        mode = *data;

        Serial.print("Mode changed: ");
        Serial.println(mode);
    }
};

class Color1CharacteristicCallbacks : public BLECharacteristicCallbacks {
public:
    void onWrite(BLECharacteristic *pCharacteristic) override {
        auto *data = (uint16_t *) pCharacteristic->getData();

        pCharacteristic->notify();

        red = data[0];
        green = data[1];
        blue = data[2];

        ledcWrite(RED_CHANNEL, red);
        ledcWrite(GREEN_CHANNEL, green);
        ledcWrite(BLUE_CHANNEL, blue);

        Serial.print("Color1 changed: ");
        Serial.print(red);
        Serial.print(" ");
        Serial.print(green);
        Serial.print(" ");
        Serial.println(blue);
    }
};

class TurnOnCharacteristicCallbacks : public BLECharacteristicCallbacks {
public:
    void onWrite(BLECharacteristic *pCharacteristic) override {
        auto *data = pCharacteristic->getData();

        pCharacteristic->notify();

        turnOn = *data;

        if (turnOn == 1) {
            ledcWrite(RED_CHANNEL, red);
            ledcWrite(GREEN_CHANNEL, green);
            ledcWrite(BLUE_CHANNEL, blue);

            Serial.println("Turned on");
        } else {
            ledcWrite(RED_CHANNEL, 0);
            ledcWrite(GREEN_CHANNEL, 0);
            ledcWrite(BLUE_CHANNEL, 0);

            Serial.println("Turned off");
        }
    }
};

class SpeedCharacteristicCallbacks : public BLECharacteristicCallbacks {
public:
    void onWrite(BLECharacteristic *pCharacteristic) override {
        auto *data = pCharacteristic->getData();

        pCharacteristic->notify();

        speed = *data;
        Serial.print("Speed changed: ");
        Serial.println(speed);
    }
};

class OtaCharacteristicCallbacks : public BLECharacteristicCallbacks {
public:
    void onWrite(BLECharacteristic *pCharacteristic) override {
        auto *data = pCharacteristic->getData();

        pCharacteristic->notify();

        ota = *data;
        Serial.print("Ota: ");
        Serial.println(ota);

        if (ota == 1) {
            WiFi.mode(WIFI_AP);
            WiFi.softAP(DEVICE_NAME, "12345678");

            ArduinoOTA
                    .onStart([]() {
                        String type;
                        if (ArduinoOTA.getCommand() == U_FLASH)
                            type = "sketch";
                        else // U_SPIFFS
                            type = "filesystem";

                        // NOTE: if updating SPIFFS this would be the place to unmount SPIFFS using SPIFFS.end()
                        Serial.println("Start updating " + type);
                    })
                    .onEnd([]() {
                        Serial.println("\nEnd");
                    })
                    .onProgress([](unsigned int progress, unsigned int total) {
                        Serial.printf("Progress: %u%%\r", (progress / (total / 100)));
                    })
                    .onError([](ota_error_t error) {
                        Serial.printf("Error[%u]: ", error);
                        if (error == OTA_AUTH_ERROR) Serial.println("Auth Failed");
                        else if (error == OTA_BEGIN_ERROR) Serial.println("Begin Failed");
                        else if (error == OTA_CONNECT_ERROR) Serial.println("Connect Failed");
                        else if (error == OTA_RECEIVE_ERROR) Serial.println("Receive Failed");
                        else if (error == OTA_END_ERROR) Serial.println("End Failed");
                    });

            ArduinoOTA.begin();
        } else {
            WiFi.mode(WIFI_OFF);
            ArduinoOTA.end();
        }
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
    modeCharacteristic->setValue(&mode, 1);
    modeCharacteristic->setCallbacks(new ModeCharacteristicCallbacks());

    color1Characteristic = modeService->createCharacteristic(
            COLOR1_CHARACTERISTIC,
            BLECharacteristic::PROPERTY_READ |
            BLECharacteristic::PROPERTY_WRITE |
            BLECharacteristic::PROPERTY_WRITE_NR |
            BLECharacteristic::PROPERTY_NOTIFY
    );
    color1Characteristic->addDescriptor(new BLE2902());

    uint16_t value[] = {red, green, blue};
    color1Characteristic->setValue((uint8_t *) value, 6);

    color1Characteristic->setCallbacks(new Color1CharacteristicCallbacks());

    turnOnCharacteristic = modeService->createCharacteristic(
            TURN_ON_CHARACTERISTIC,
            BLECharacteristic::PROPERTY_READ |
            BLECharacteristic::PROPERTY_WRITE |
            BLECharacteristic::PROPERTY_WRITE_NR |
            BLECharacteristic::PROPERTY_NOTIFY
    );
    turnOnCharacteristic->addDescriptor(new BLE2902());
    turnOnCharacteristic->setValue(&turnOn, 1);
    turnOnCharacteristic->setCallbacks(new TurnOnCharacteristicCallbacks());

    speedCharacteristic = modeService->createCharacteristic(
            SPEED_CHARACTERISTIC,
            BLECharacteristic::PROPERTY_READ |
            BLECharacteristic::PROPERTY_WRITE |
            BLECharacteristic::PROPERTY_WRITE_NR |
            BLECharacteristic::PROPERTY_NOTIFY
    );
    speedCharacteristic->addDescriptor(new BLE2902());
    speedCharacteristic->setValue(&speed, 1);
    speedCharacteristic->setCallbacks(new SpeedCharacteristicCallbacks());

    modeService->start();

    auto otaService = server->createService(OTA_SERVICE);

    otaCharacteristic = otaService->createCharacteristic(
            MODE_CHARACTERISTIC,
            BLECharacteristic::PROPERTY_READ |
            BLECharacteristic::PROPERTY_WRITE |
            BLECharacteristic::PROPERTY_WRITE_NR |
            BLECharacteristic::PROPERTY_NOTIFY
    );
    otaCharacteristic->addDescriptor(new BLE2902());
    otaCharacteristic->setValue(&ota, 1);
    otaCharacteristic->setCallbacks(new OtaCharacteristicCallbacks());

    otaService->start();

    server->getAdvertising()->addServiceUUID(MODE_SERVICE);
    // TODO: https://specificationrefs.bluetooth.com/assigned-values/Appearance%20Values.pdf
    server->getAdvertising()->setAppearance(0x03C0);
    server->getAdvertising()->setScanResponse(true);
    server->getAdvertising()->setMinPreferred(0x06);  // functions that help with iPhone connections issue ?
    server->getAdvertising()->setMinPreferred(0x12);
    server->getAdvertising()->start();

    batteryTicker.attach(60, readBattery);
    readBattery();

    setupLed();

    Serial.println("BT Started");
}

void loop() {
    if (ota == 1) {
        ArduinoOTA.handle();
    }
}