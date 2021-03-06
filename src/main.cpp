#include <Arduino.h>
#include <BLEDevice.h>
#include <BLEServer.h>
#include <BLE2902.h>
#include <Ticker.h>
#include <WiFi.h>
#include <ArduinoOTA.h>
#include <Preferences.h>
#include <config.h>

typedef enum {
    STATIC,
    RAINBOW,
    STROBE,
} Mode;

BLECharacteristic *batteryCharacteristic = nullptr;

BLECharacteristic *modeCharacteristic = nullptr;
BLECharacteristic *color1Characteristic = nullptr;
BLECharacteristic *color2Characteristic = nullptr;
BLECharacteristic *turnOnCharacteristic = nullptr;
BLECharacteristic *speedCharacteristic = nullptr;
BLECharacteristic *rainbowBrightnessCharacteristic = nullptr;

BLECharacteristic *otaCharacteristic = nullptr;

Ticker batteryTicker;
Ticker saveTicker;
Preferences preferences;

uint8_t mode = STATIC;
uint16_t color[] = {MAX_COLOR_VALUE, 0, 0};
uint16_t color2[] = {0, 0, 0};
uint8_t turnOn = 1;
uint8_t speed = 100;
uint8_t ota = 0;
uint8_t batteryLevel = 0;
uint8_t rainbowBrightness = 255;

uint8_t currentFadingUp = 1;
uint8_t currentFadingDown = 0;
uint16_t maxRainbowColor = rainbowBrightness * MAX_COLOR_VALUE / 255;

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
    preferences.putUChar("brightness", rainbowBrightness);

    preferences.putUShort("red", color[0]);
    preferences.putUShort("green", color[1]);
    preferences.putUShort("blue", color[2]);

    preferences.putUShort("red2", color2[0]);
    preferences.putUShort("green2", color2[1]);
    preferences.putUShort("blue2", color2[2]);

    Serial.println("Preferences saved");
}

uint8_t reading = 0;
uint sum = 0;

void readBattery(bool useMedian = false) {
    uint16_t value = analogRead(VOLTAGE_PIN);

    sum += value;
    reading++;

    if (useMedian) {
        if (reading >= 10) {
            value = sum / reading;

            reading = 0;
            sum = 0;
        } else {
            return;
        }
    }

    auto battery = map(value, BATTERY_LOW, BATTERY_HIGH, 0, 100);

    if (battery > 100) {
        battery = 100;
    } else if (battery < 0) {
        battery = 0;
    }

    uint8_t percent = battery;

    if (batteryLevel != percent) {
        batteryLevel = percent;
        batteryCharacteristic->setValue(&batteryLevel, 1);
        batteryCharacteristic->notify();
    }

    Serial.print("Battery level: ");
    Serial.print(battery);
    Serial.print("%, ");
    Serial.println(value);
}

void setupBattery() {
    pinMode(VOLTAGE_PIN, INPUT);

    batteryTicker.attach_ms(2000, readBattery, true);
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

            currentFadingUp = 1;
            currentFadingDown = 0;
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

class Color2CharacteristicCallbacks : public BLECharacteristicCallbacks {
public:
    void onWrite(BLECharacteristic *pCharacteristic) override {
        auto data = (uint16_t *) pCharacteristic->getData();

        pCharacteristic->notify();

        color2[0] = data[0];
        color2[1] = data[1];
        color2[2] = data[2];

        if (turnOn != 1) {
            turnOn = 1;
            turnOnCharacteristic->setValue(&turnOn, 1);
            turnOnCharacteristic->notify();
        }

        Serial.print("Color2 changed: ");
        Serial.print(color2[0]);
        Serial.print(" ");
        Serial.print(color2[1]);
        Serial.print(" ");
        Serial.println(color2[2]);

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

class RainbowBrightnessCharacteristicCallbacks : public BLECharacteristicCallbacks {
public:
    void onWrite(BLECharacteristic *pCharacteristic) override {
        auto data = pCharacteristic->getData();

        rainbowBrightness = *data;

        pCharacteristic->notify();

        maxRainbowColor = rainbowBrightness * MAX_COLOR_VALUE / 255;

        if (turnOn != 1) {
            turnOn = 1;
            turnOnCharacteristic->setValue(&turnOn, 1);
            turnOnCharacteristic->notify();
        }

        Serial.print("Rainbow brightness changed: ");
        Serial.print(rainbowBrightness);
        Serial.print(" (");
        Serial.print(maxRainbowColor);
        Serial.println(")");

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
    rainbowBrightness = preferences.getUChar("brightness", rainbowBrightness);

    Serial.print("Loaded mode: ");
    Serial.print(mode);
    Serial.print(", speed: ");
    Serial.print(speed);
    Serial.print(", rainbowBrightness: ");
    Serial.print(rainbowBrightness);
    Serial.println();

    if (mode == RAINBOW) {
        color[0] = MAX_COLOR_VALUE;
        color[1] = 0;
        color[2] = 0;
    } else {
        color[0] = preferences.getUShort("red", color[0]);
        color[1] = preferences.getUShort("green", color[1]);
        color[2] = preferences.getUShort("blue", color[2]);
    }

    color2[0] = preferences.getUShort("red2", color2[0]);
    color2[1] = preferences.getUShort("green2", color2[1]);
    color2[2] = preferences.getUShort("blue2", color2[2]);

    ledcWrite(RED_CHANNEL, color[0]);
    ledcWrite(GREEN_CHANNEL, color[1]);
    ledcWrite(BLUE_CHANNEL, color[2]);

    Serial.print("Loaded Color1: ");
    Serial.print(color[0]);
    Serial.print(" ");
    Serial.print(color[1]);
    Serial.print(" ");
    Serial.println(color[2]);

    Serial.print("Loaded Color2: ");
    Serial.print(color2[0]);
    Serial.print(" ");
    Serial.print(color2[1]);
    Serial.print(" ");
    Serial.println(color2[2]);
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
    batteryCharacteristic->setValue(&batteryLevel, 1);
//    batteryCharacteristic->setAccessPermissions(ESP_GATT_PERM_READ_ENC_MITM | ESP_GATT_PERM_WRITE_ENC_MITM);
    batteryService->start();

    auto mainService = server->createService(BLEUUID(MAIN_SERVICE), 30);

    modeCharacteristic = mainService->createCharacteristic(
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

    color1Characteristic = mainService->createCharacteristic(
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

    color2Characteristic = mainService->createCharacteristic(
            COLOR2_CHARACTERISTIC,
            BLECharacteristic::PROPERTY_READ |
            BLECharacteristic::PROPERTY_WRITE |
            BLECharacteristic::PROPERTY_WRITE_NR |
            BLECharacteristic::PROPERTY_NOTIFY
    );
    color2Characteristic->addDescriptor(new BLE2902());
    color2Characteristic->setValue((uint8_t *) color2, 6);
    color2Characteristic->setCallbacks(new Color2CharacteristicCallbacks());
//    color1Characteristic->setAccessPermissions(ESP_GATT_PERM_READ_ENC_MITM | ESP_GATT_PERM_WRITE_ENC_MITM);

    turnOnCharacteristic = mainService->createCharacteristic(
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

    speedCharacteristic = mainService->createCharacteristic(
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

    rainbowBrightnessCharacteristic = mainService->createCharacteristic(
            RAINBOW_BRIGHTNESS_CHARACTERISTIC,
            BLECharacteristic::PROPERTY_READ |
            BLECharacteristic::PROPERTY_WRITE |
            BLECharacteristic::PROPERTY_WRITE_NR |
            BLECharacteristic::PROPERTY_NOTIFY
    );
    rainbowBrightnessCharacteristic->addDescriptor(new BLE2902());
    rainbowBrightnessCharacteristic->setValue(&rainbowBrightness, 1);
    rainbowBrightnessCharacteristic->setCallbacks(new RainbowBrightnessCharacteristicCallbacks());
//    speedCharacteristic->setAccessPermissions(ESP_GATT_PERM_READ_ENC_MITM | ESP_GATT_PERM_WRITE_ENC_MITM);

    mainService->start();

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

    server->getAdvertising()->addServiceUUID(MAIN_SERVICE);
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

uint8_t strobeCurrentColor = 0;

void loop() {
    if (ota == 1) {
        ArduinoOTA.handle();
    }

    if (turnOn == 1) {
        if (mode == RAINBOW) {
            const unsigned long time = millis();
            const uint8_t interval = (255 - speed);     // TODO: find appropriate interval+fadeAmount formula

            if (time - lastTime > interval) {
                lastTime = time;

                color[currentFadingUp] += fadeAmount;
                color[currentFadingDown] -= fadeAmount;

                if (color[currentFadingUp] >= MAX_COLOR_VALUE) {
                    color[currentFadingUp] = MAX_COLOR_VALUE;
                    currentFadingUp = (currentFadingUp + 1) % 3;
                }

                // uint16 overflow -> color will be 65535 after 0
                if (color[currentFadingDown] >= MAX_COLOR_VALUE) {
                    color[currentFadingDown] = 0;
                    currentFadingDown = (currentFadingDown + 1) % 3;
                }

                ledcWrite(RED_CHANNEL, color[0] * rainbowBrightness / 255);
                ledcWrite(GREEN_CHANNEL, color[1] * rainbowBrightness / 255);
                ledcWrite(BLUE_CHANNEL, color[2] * rainbowBrightness / 255 );

                Serial.print("Rainbow color ");
                Serial.print(color[0]);
                Serial.print(", ");
                Serial.print(color[1]);
                Serial.print(", ");
                Serial.print(color[2]);
                Serial.print("; up: ");
                Serial.print(currentFadingUp);
                Serial.print("; down: ");
                Serial.print(currentFadingDown);
                Serial.println();
            }
        } else if (mode == STROBE) {
            const unsigned long time = millis();

            if (time - lastTime >= map(speed, 0, 255, 0, 2000)) {
                lastTime = time;

                if (strobeCurrentColor == 0) {
                    ledcWrite(RED_CHANNEL, color[0]);
                    ledcWrite(GREEN_CHANNEL, color[1]);
                    ledcWrite(BLUE_CHANNEL, color[2]);

                    strobeCurrentColor = 1;
                } else {
                    ledcWrite(RED_CHANNEL, color2[0]);
                    ledcWrite(GREEN_CHANNEL, color2[1]);
                    ledcWrite(BLUE_CHANNEL, color2[2]);

                    strobeCurrentColor = 0;
                }

                Serial.print("Strobe color ");
                Serial.println(strobeCurrentColor);
            }
        }
    }
}