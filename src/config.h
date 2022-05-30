#ifndef RGB_ESP32_CONFIG_H
#define RGB_ESP32_CONFIG_H


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

#define BATTERY_LOW 3000    // 2700 = 2.3v = 9.3v
#define BATTERY_HIGH 3900

#define BATTERY_LOW_TURNED_ON 2800
#define BATTERY_HIGH_TURNED_ON 3700

#define OTA_PASSWORD "12345678"

#endif //RGB_ESP32_CONFIG_H
