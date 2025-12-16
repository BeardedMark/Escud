#pragma once
#include <WiFi.h>
#include <WebSocketsClient.h>
#include <BLEDevice.h>
#include <BLEUtils.h>
#include <BLEAdvertising.h>
#include <Wire.h>
#include <LiquidCrystal_I2C.h>

class DeviceCore {
public:
    DeviceCore();

    void begin(
        const char* wifiSsid = "M3G",
        const char* wifiPass = "20101995",
        const char* wsHost = "192.168.0.30",
        uint16_t wsPort = 8080,
        const char* wsPath = "/",
        uint8_t lcdAddr = 0x27
    );

    void loop();
    void sendMessage(String text);
    void setBleSignal(String name, String data);

private:
    // Внутренние методы
    void handleIncoming(String msg);
    static void onWsEventStatic(WStype_t type, uint8_t* payload, size_t length, void* userData);
    void onWsEvent(WStype_t type, uint8_t* payload, size_t length);

private:
    WebSocketsClient ws;
    BLEAdvertising* advertising;
    LiquidCrystal_I2C* lcd;

    // Кнопка
    uint8_t btnPin = 0;
    uint8_t ledPin = 2;
    bool lastBtnState = HIGH;
};
