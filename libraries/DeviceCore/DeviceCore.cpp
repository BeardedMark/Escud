#include "DeviceCore.h"

DeviceCore::DeviceCore() {}

void DeviceCore::begin(
    const char* wifiSsid,
    const char* wifiPass,
    const char* wsHost,
    uint16_t wsPort,
    const char* wsPath,
    uint8_t lcdAddr
) {
    Serial.begin(115200);
    delay(200);

    // LCD
    lcd = new LiquidCrystal_I2C(lcdAddr, 16, 2);
    lcd->init();
    lcd->backlight();

    // Пины
    pinMode(ledPin, OUTPUT);
    pinMode(btnPin, INPUT_PULLUP);

    // WiFi
    WiFi.begin(wifiSsid, wifiPass);
    while (WiFi.status() != WL_CONNECTED) {
        delay(300);
        Serial.print(".");
    }
    Serial.println("\nWiFi OK!");

    // WebSocket
    ws.begin(wsHost, wsPort, wsPath);
    ws.onEvent(onWsEventStatic);
    ws.setReconnectInterval(2000);
    ws.setUserData(this);

    // BLE
    BLEDevice::init("ESP32_DEVICE");
    advertising = BLEDevice::getAdvertising();
    setBleSignal("Name", "Hello");
}

void DeviceCore::setBleSignal(String name, String data) {
    BLEAdvertisementData advData;
    advData.setName(name.c_str());
    advData.setManufacturerData(data);

    advertising->setAdvertisementData(advData);
}

void DeviceCore::loop() {
    ws.loop();

    lcd->setCursor(0, 0);
    lcd->print("ONLINE          ");

    bool now = digitalRead(btnPin);
    if (lastBtnState != now && now == LOW) {
        sendMessage("button_click");
    }
    lastBtnState = now;
}

void DeviceCore::sendMessage(String text) {
    ws.sendTXT(text);
}

void DeviceCore::handleIncoming(String msg) {
    msg.trim();

    if (msg == "on") {
        digitalWrite(ledPin, HIGH);

    } else if (msg == "off") {
        digitalWrite(ledPin, LOW);

    } else if (msg == "play") {
        advertising->start();
        digitalWrite(ledPin, HIGH);
        delay(2000);
        advertising->stop();
        digitalWrite(ledPin, LOW);
    }
}

void DeviceCore::onWsEventStatic(WStype_t type, uint8_t* payload, size_t length, void* userData) {
    DeviceCore* self = static_cast<DeviceCore*>(userData);
    self->onWsEvent(type, payload, length);
}

void DeviceCore::onWsEvent(WStype_t type, uint8_t* payload, size_t length) {
    switch (type) {
    case WStype_CONNECTED:
        Serial.println("WS connected");
        ws.sendTXT("/register/1");
        break;

    case WStype_TEXT:
        Serial.printf("WS: %s\n", payload);
        handleIncoming((char*)payload);
        break;

    case WStype_DISCONNECTED:
        Serial.println("WS disconnected");
        break;
    }
}
