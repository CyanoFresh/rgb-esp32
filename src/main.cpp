#include <Arduino.h>
#include <BLEDevice.h>
#include <BLEServer.h>
#include <BLE2902.h>
#include <Ticker.h>
#include <WiFi.h>
#include <ArduinoOTA.h>
#include <Preferences.h>

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

#define MAX_COLOR_VALUE 4095

#define BATTERY_LOW 3000
#define BATTERY_HIGH 3900

#define BATTERY_LOW 3000
#define BATTERY_HIGH 3900

#define BATTERY_LOW_TURNED_ON 2800
#define BATTERY_HIGH_TURNED_ON 3700

#define OTA_PASSWORD "12345678"

typedef enum {
    STATIC,
    RAINBOW,
    STROBE,
} Mode;

BLECharacteristic *batteryCharacteristic = nullptr;

BLECharacteristic *modeCharacteristic = nullptr;
BLECharacteristic *color1Characteristic = nullptr;
BLECharacteristic *turnOnCharacteristic = nullptr;
BLECharacteristic *speedCharacteristic = nullptr;

BLECharacteristic *otaCharacteristic = nullptr;

Ticker batteryTicker;
Ticker saveTicker;
Preferences preferences;

uint8_t mode = STATIC;
uint16_t color[] = {MAX_COLOR_VALUE, 0, 0};
uint8_t turnOn = 1;
uint8_t speed = 100;
uint8_t ota = 0;
uint16_t rainbowBrightness = MAX_COLOR_VALUE;  // TODO

void setupLed() {
    pinMode(RED_PIN, OUTPUT);
    pinMode(GREEN_PIN, OUTPUT);
    pinMode(BLUE_PIN, OUTPUT);

    ledcSetup(RED_CHANNEL, 10000, 12);
    ledcSetup(GREEN_CHANNEL, 10000, 12);
    ledcSetup(BLUE_CHANNEL, 10000, 12);

    ledcAttachPin(RED_PIN, RED_CHANNEL);
    ledcAttachPin(GREEN_PIN, GREEN_CHANNEL);
    ledcAttachPin(BLUE_PIN, BLUE_CHANNEL);
}

void savePreferences() {
    preferences.putUChar("mode", mode);
    preferences.putUChar("speed", speed);

    preferences.putUShort("red", color[0]);
    preferences.putUShort("green", color[1]);
    preferences.putUShort("blue", color[2]);

    Serial.println("Preferences saved");
}

void readBattery() {
    // TODO: jumping values
    uint16_t value = analogRead(VOLTAGE_PIN);

    uint16_t low = BATTERY_LOW_TURNED_ON;
    uint16_t high = BATTERY_HIGH_TURNED_ON;

    if (turnOn == 0) {
        low = BATTERY_LOW;
        high = BATTERY_HIGH;
    }

    auto batteryLevel = map(value, low, high, 0, 100);

    if (batteryLevel > 100) {
        batteryLevel = 100;
    } else if (batteryLevel < 0) {
        batteryLevel = 0;
    }

    uint8_t percent = batteryLevel;

    batteryCharacteristic->setValue(&percent, 1);
    batteryCharacteristic->notify();
    
    Serial.print("Battery level: ");
    Serial.print(batteryLevel);
    Serial.print("%, ");
    Serial.println(value);
}

void setupBattery() {
    pinMode(VOLTAGE_PIN, INPUT);

    // TODO: adjust timings
    batteryTicker.attach(3, readBattery);
    readBattery();
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
        auto data = pCharacteristic->getData();

        mode = *data;

        pCharacteristic->notify();

        if (turnOn == 0) {
            turnOn = 1;
            turnOnCharacteristic->setValue(&turnOn, 1);
            turnOnCharacteristic->notify();
        }

        if (mode == RAINBOW) {
            color[0] = MAX_COLOR_VALUE;
            color[1] = 0;
            color[2] = 0;
        }

        color1Characteristic->setValue((uint8_t *) color, 6);
        color1Characteristic->notify();

        Serial.print("Mode changed: ");
        Serial.println(mode);

        saveTicker.once(5, savePreferences);
    }
};

class Color1CharacteristicCallbacks : public BLECharacteristicCallbacks {
public:
    void onWrite(BLECharacteristic *pCharacteristic) override {
        auto data = (uint16_t *) pCharacteristic->getData();

        pCharacteristic->notify();

        color[0] = data[0];
        color[1] = data[1];
        color[2] = data[2];

        ledcWrite(RED_CHANNEL, color[0]);
        ledcWrite(GREEN_CHANNEL, color[1]);
        ledcWrite(BLUE_CHANNEL, color[2]);

        if (turnOn != 1) {
            turnOn = 1;
            turnOnCharacteristic->setValue(&turnOn, 1);
            turnOnCharacteristic->notify();
        }

        Serial.print("Color1 changed: ");
        Serial.print(color[0]);
        Serial.print(" ");
        Serial.print(color[1]);
        Serial.print(" ");
        Serial.println(color[2]);

        saveTicker.once(5, savePreferences);
    }
};

class TurnOnCharacteristicCallbacks : public BLECharacteristicCallbacks {
public:
    void onWrite(BLECharacteristic *pCharacteristic) override {
        auto data = pCharacteristic->getData();

        turnOn = *data;

        pCharacteristic->notify();

        if (turnOn == 1) {
            ledcWrite(RED_CHANNEL, color[0]);
            ledcWrite(GREEN_CHANNEL, color[1]);
            ledcWrite(BLUE_CHANNEL, color[2]);

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
        auto data = pCharacteristic->getData();

        speed = *data;

        pCharacteristic->notify();

        if (turnOn != 1) {
            turnOn = 1;
            turnOnCharacteristic->setValue(&turnOn, 1);
            turnOnCharacteristic->notify();
        }

        Serial.print("Speed changed: ");
        Serial.println(speed);

        saveTicker.once(5, savePreferences);
    }
};

class OtaCharacteristicCallbacks : public BLECharacteristicCallbacks {
public:
    void onWrite(BLECharacteristic *pCharacteristic) override {
        auto data = pCharacteristic->getData();

        ota = *data;

        pCharacteristic->notify();

        Serial.print("Ota changed: ");
        Serial.println(ota);

        if (ota == 1) {
            WiFi.mode(WIFI_AP);
            WiFi.softAP(DEVICE_NAME, OTA_PASSWORD);

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

void setupPreferences() {
    preferences.begin("rgb-esp32", false);

    mode = preferences.getUChar("mode", mode);
    speed = preferences.getUChar("speed", speed);

    Serial.print("Loaded mode: ");
    Serial.print(mode);
    Serial.print(", speed: ");
    Serial.println(speed);

    if (mode == RAINBOW) {
        color[0] = MAX_COLOR_VALUE;
        color[1] = 0;
        color[2] = 0;
    } else {
        color[0] = preferences.getUShort("red", color[0]);
        color[1] = preferences.getUShort("green", color[1]);
        color[2] = preferences.getUShort("blue", color[2]);
    }

    ledcWrite(RED_CHANNEL, color[0]);
    ledcWrite(GREEN_CHANNEL, color[1]);
    ledcWrite(BLUE_CHANNEL, color[2]);

    Serial.print("Loaded Color1: ");
    Serial.print(color[0]);
    Serial.print(" ");
    Serial.print(color[1]);
    Serial.print(" ");
    Serial.println(color[2]);
}

void setupBLE() {
    BLEDevice::init(DEVICE_NAME);
    // TODO: debug why bonding is not saved
//    BLEDevice::setEncryptionLevel(ESP_BLE_SEC_ENCRYPT_MITM);
//    auto pSecurity = new BLESecurity();
//    pSecurity->setCapability(ESP_IO_CAP_OUT);
//    pSecurity->setAuthenticationMode(ESP_LE_AUTH_REQ_SC_MITM_BOND);
//    pSecurity->setStaticPIN(123456);

    auto server = BLEDevice::createServer();
    server->setCallbacks(new MyServerCallbacks());

    auto batteryService = server->createService(BATTERY_SERVICE);
    batteryCharacteristic = batteryService->createCharacteristic(
            BATTERY_CHARACTERISTIC,
            BLECharacteristic::PROPERTY_READ |
            BLECharacteristic::PROPERTY_NOTIFY
    );
    batteryCharacteristic->addDescriptor(new BLE2902());
//    batteryCharacteristic->setAccessPermissions(ESP_GATT_PERM_READ_ENC_MITM | ESP_GATT_PERM_WRITE_ENC_MITM);
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
//    modeCharacteristic->setAccessPermissions(ESP_GATT_PERM_READ_ENC_MITM | ESP_GATT_PERM_WRITE_ENC_MITM);

    color1Characteristic = modeService->createCharacteristic(
            COLOR1_CHARACTERISTIC,
            BLECharacteristic::PROPERTY_READ |
            BLECharacteristic::PROPERTY_WRITE |
            BLECharacteristic::PROPERTY_WRITE_NR |
            BLECharacteristic::PROPERTY_NOTIFY
    );
    color1Characteristic->addDescriptor(new BLE2902());

    color1Characteristic->setValue((uint8_t *) color, 6);

    color1Characteristic->setCallbacks(new Color1CharacteristicCallbacks());
//    color1Characteristic->setAccessPermissions(ESP_GATT_PERM_READ_ENC_MITM | ESP_GATT_PERM_WRITE_ENC_MITM);

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
//    turnOnCharacteristic->setAccessPermissions(ESP_GATT_PERM_READ_ENC_MITM | ESP_GATT_PERM_WRITE_ENC_MITM);

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
//    speedCharacteristic->setAccessPermissions(ESP_GATT_PERM_READ_ENC_MITM | ESP_GATT_PERM_WRITE_ENC_MITM);

    modeService->start();

    auto otaService = server->createService(OTA_SERVICE);

    otaCharacteristic = otaService->createCharacteristic(
            OTA_CHARACTERISTIC,
            BLECharacteristic::PROPERTY_READ |
            BLECharacteristic::PROPERTY_WRITE |
            BLECharacteristic::PROPERTY_WRITE_NR |
            BLECharacteristic::PROPERTY_NOTIFY
    );
    otaCharacteristic->addDescriptor(new BLE2902());
    otaCharacteristic->setValue(&ota, 1);
    otaCharacteristic->setCallbacks(new OtaCharacteristicCallbacks());
//    otaCharacteristic->setAccessPermissions(ESP_GATT_PERM_READ_ENC_MITM | ESP_GATT_PERM_WRITE_ENC_MITM);

    otaService->start();

    server->getAdvertising()->addServiceUUID(MODE_SERVICE);
    server->getAdvertising()->setAppearance(0x03C0);
    server->getAdvertising()->setScanResponse(true);
    server->getAdvertising()->setMinPreferred(0x06);  // functions that help with iPhone connections issue ?
    server->getAdvertising()->setMinPreferred(0x12);
    server->getAdvertising()->start();

    Serial.println("BT Started");
}

void setup() {
    Serial.begin(115200);

    setupLed();
    setupPreferences();
    setupBLE();
    setupBattery();
}

unsigned long lastTime = 0;

const uint8_t fadeAmount = 5;
uint8_t currentFadingUp = 1;
uint8_t currentFadingDown = 0;

void loop() {
    if (ota == 1) {
        ArduinoOTA.handle();
    }

    if (mode == RAINBOW && turnOn == 1) {
        const unsigned long time = millis();
        const uint8_t interval = (255 - speed);     // TODO: find appropriate interval+fadeAmount formula

        if (time - lastTime > interval) {
            lastTime = time;

            color[currentFadingUp] += fadeAmount;
            color[currentFadingDown] -= fadeAmount;

            if (color[currentFadingUp] >= rainbowBrightness) {
                color[currentFadingUp] = rainbowBrightness;
                currentFadingUp = (currentFadingUp + 1) % 3;
            }

            // uint16 overflow -> color will be 65535 after 0
            if (color[currentFadingDown] >= rainbowBrightness) {
                color[currentFadingDown] = 0;
                currentFadingDown = (currentFadingDown + 1) % 3;
            }

            ledcWrite(RED_CHANNEL, color[0]);
            ledcWrite(GREEN_CHANNEL, color[1]);
            ledcWrite(BLUE_CHANNEL, color[2]);

            Serial.print("Rainbow color ");
            Serial.print(color[0]);
            Serial.print(", ");
            Serial.print(color[1]);
            Serial.print(", ");
            Serial.println(color[2]);
        }
    }
}