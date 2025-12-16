#include <Wire.h>
#include <LiquidCrystal_I2C.h>
#include <WiFi.h>
#include <HTTPClient.h>
#include <HardwareSerial.h>
#include <Adafruit_Fingerprint.h>
#include <Keypad.h>
#include <base64.h>
#include <WiFiManager.h>
#define BUZZER_PIN 5  // Подключен к I/O пину YL-44

// --- URLs ---
const char* URL_REGISTER = "https://1s.dnlmarket.ru/ut/hs/api/skudlog/post";
const char* URL_DELETE_FINGER = "https://1s.dnlmarket.ru/ut/hs/api/skuddata/post";
const char* URL_ADD_FINGER = "https://1s.dnlmarket.ru/ut/hs/api/skuddata/post";
const char* URL_GET_PASS = "https://1s.dnlmarket.ru/ut/hs/api/skuddata/get";

const char* DEVICE_WIFI_NAME = "DNL Eskud";
String DEVICE_NAME = "Escud";
String LOGIN_1S = "dnlmarket";
String PASSWORD_1S = "User201095";

int MAX_MODELS = 127;

WiFiManager wm;

// --- Fingerprint & LCD ---
HardwareSerial mySerial(2);
Adafruit_Fingerprint finger(&mySerial);
LiquidCrystal_I2C lcd(0x27, 16, 2);

// --- Keypad Setup ---
const byte ROWS = 4, COLS = 3;
char keys[ROWS][COLS] = {
  { '1', '2', '3' }, { '4', '5', '6' }, { '7', '8', '9' }, { '*', '0', '#' }
};
byte rowPins[ROWS] = { 33, 25, 26, 27 }, colPins[COLS] = { 14, 12, 13 };
Keypad keypad = Keypad(makeKeymap(keys), rowPins, colPins, ROWS, COLS);

// --- Helper Input --- -------------------------------------------------------------------------------------------------------------------

void PrintLcdLine(int line, String text, String log = "", String type = "") {
  text = text.substring(0, 16);
  line = constrain(line - 1, 0, 1);

  lcd.setCursor(0, line);
  lcd.print("                ");
  lcd.setCursor(0, line);
  lcd.print(text);

  if (log.length() > 0) {
    String message = text;

    if (type != "") {
      message = type + " " + message;
    }

    message += " (" + log + ")";
    Serial.println(message);
  }

  PlayAlert(type);
}

String InputLcdLine(int line, String label, bool hidden = false) {
  // label = label.substring(0, 16);
  line = constrain(line - 1, 0, 1);

  lcd.setCursor(0, line);
  lcd.print("                ");
  lcd.setCursor(0, line);
  lcd.print(label);
  lcd.blink();
  PlayAlert("warning");

  String input = "";
  while (true) {
    char key = keypad.getKey();
    if (key) {
      PlayAlert("key");
      if (key == '#') break;
      if (key == '*') return "";
      input += key;

      if (hidden) {
        lcd.setCursor(label.length() + input.length() - 1, line);
        lcd.print("*");
      } else {
        lcd.setCursor(0, line);
        lcd.print(label + input);
      }
    }
  }

  lcd.noBlink();
  return input;
}

bool ConfirmLcdLine(int line) {
  line = constrain(line - 1, 0, 1);

  lcd.setCursor(0, line);
  lcd.print("                ");
  lcd.setCursor(0, line);
  lcd.print("*-No       #-Yes");
  PlayAlert("warning");

  while (true) {
    char key = keypad.getKey();
    if (key) {
      PlayAlert("key");
      if (key == '*') {
        return false;
      }
      if (key == '#') {
        return true;
      }
    }
  }
}

bool enterPassword() {
  bool result = false;
  PlayAlert("warning");

  String pass = InputLcdLine(2, "Password:", true);
  PrintLcdLine(2, "Check pass...", "Проверка пароля", "wait");

  if (pass == HttpGetPassword()) {
    PrintLcdLine(2, "Correct pass!", "Верный пароль", "success");
    result = true;
  } else {
    PrintLcdLine(2, "Wrong pass!", "Неверный пароль", "danger");
  }

  delay(1000);
  return result;
}

void PlayTone(int volume = 500, int duration = 500, int pause = 0) {
  tone(BUZZER_PIN, volume, duration);
  noTone(BUZZER_PIN);
  delay(pause);
}

void PlayAlert(String type) {
  if (type == "success") {
    PlayTone(1000, 200);
    delay(50);
    PlayTone(1200, 200);
  } else if (type == "warning") {
    PlayTone(800, 300);
    delay(50);
    PlayTone(800, 300);
  } else if (type == "danger") {
    PlayTone(600, 500);
    delay(50);
    PlayTone(600, 500);
  } else if (type == "key") {
    PlayTone(1500, 100);
  }
}

String GetFingerprintCount() {
  int occupiedCount = 0;

  for (uint8_t id = 1; id <= MAX_MODELS; id++) {
    if (finger.loadModel(id) == FINGERPRINT_OK) {
      occupiedCount++;
      Serial.println(id);
    }
    delay(10);
  }

  return String(occupiedCount);
}

String GetDeviceSerial() {
  return String(ESP.getEfuseMac());
}

// --- Setup --- -------------------------------------------------------------------------------------------------------------------

void setup() {
  Serial.begin(115200);
  mySerial.begin(57600, SERIAL_8N1, 16, 17);
  pinMode(BUZZER_PIN, OUTPUT);
  lcd.init();
  finger.begin(57600);

  construct();
}

void construct() {
  lcd.backlight();
  finger.LEDcontrol(false);

  PrintLcdLine(1, "Start " + DEVICE_NAME + "...", "Старт устройства", "warning");
  PrintLcdLine(2, "ID:" + GetDeviceSerial(), "Идентификатор устройства");
  delay(1000);

  if (finger.verifyPassword()) {
    PrintLcdLine(2, "Memory:" + GetFingerprintCount() + "/" + MAX_MODELS, "Колличество записей");
    delay(1000);
  } else {
    while (1) delay(1);
  }

  PrintLcdLine(1, "Use last wifi?", "Использовать старую сеть?");

  if (!ConfirmLcdLine(2)) {
    wm.resetSettings();
    PrintLcdLine(1, "Open Wifi settings...");
    PrintLcdLine(2, String(DEVICE_WIFI_NAME), "Подключение создано", "success");
  }

  PrintLcdLine(2, "Connect...", "Подключение к сети");

  if (wm.autoConnect(DEVICE_WIFI_NAME)) {
    PrintLcdLine(2, WiFi.SSID() + " connected!", "Подключение установлено", "success");
    delay(1000);
  }
}

// --- Main Loop --- -------------------------------------------------------------------------------------------------------------------

unsigned long previousMillis = 0;
const long interval = 10;

void loop() {
  lcd.clear();
  lcd.noBlink();
  finger.LEDcontrol(true);

  PrintLcdLine(1, "Welcome to DNL!", "Приветствие");
  PrintLcdLine(2, DEVICE_NAME + " wait...");

  int id = -1;
  char key = '\0';

  while (true) {
    unsigned long currentMillis = millis();

    if (currentMillis - previousMillis >= interval) {
      previousMillis = currentMillis;

      id = getFingerID();
      key = keypad.getKey();

      if (id >= 0 || key == '#' || key == '*') {
        break;
      }
    }
  }

  if (id >= 0) {
    PrintLcdLine(2, "...", "Ожидание записи", "success");
    PrintLcdLine(2, HttpSetVisit(id), "Посещение записано");
    delay(2000);
  }

  // Действия с клавишами
  if (key == '#') {
    AddFingerprint();
  } else if (key == '*') {
    DeleteFingerprint();
  }
}

// --- Logic Blocks --- -------------------------------------------------------------------------------------------------------------------

int getFingerID() {
  if (finger.getImage() != FINGERPRINT_OK) return -1;
  if (finger.image2Tz() != FINGERPRINT_OK) return -1;
  return (finger.fingerFastSearch() == FINGERPRINT_OK) ? finger.fingerID : -2;
}

void AddFingerprint() {
  finger.LEDcontrol(false);

  PrintLcdLine(1, "Add finger", "Добавление отпечатка", "warning");

  if (!enterPassword()) return;

  int id = InputLcdLine(2, "FingerID:").toInt();
  if (id < 1 || id > MAX_MODELS) {
    PrintLcdLine(2, "FingerID failed!", "Неверный идентификатор", "success");
    delay(1000);
    return;
  };

  int userID = InputLcdLine(2, "UserID:").toInt();


  PrintLcdLine(2, "First scan...", "Получение первого скана", "warning");
  finger.LEDcontrol(true);
  while (finger.getImage() != FINGERPRINT_OK)
    ;
  if (finger.image2Tz(1) != FINGERPRINT_OK) return;
  PrintLcdLine(2, "Finger scanned!", "Палец отсканирован", "success");
  finger.LEDcontrol(false);
  delay(1000);
  while (finger.getImage() != FINGERPRINT_NOFINGER)
    ;

  PrintLcdLine(2, "Second scan...", "Получение второго скана");
  finger.LEDcontrol(true);
  while (finger.getImage() != FINGERPRINT_OK)
    ;
  if (finger.image2Tz(2) != FINGERPRINT_OK) return;
  PrintLcdLine(2, "Finger confirmed!", "Палец отсканирован", "success");
  finger.LEDcontrol(false);
  delay(1000);
  while (finger.getImage() != FINGERPRINT_NOFINGER)
    ;


  if (finger.createModel() == FINGERPRINT_OK) {
    PrintLcdLine(2, "Model created!", "Модель создана", "success");
  } else {
    PrintLcdLine(2, "Failed model!", "Ошибка создания модели", "warning");
    return;
  }
  delay(1000);

  if (!ConfirmLcdLine(2)) {
    PrintLcdLine(2, "Saved aborted!", "Сохранение отменено", "danger");
    delay(1000);
    return;
  }

  if (finger.storeModel(id) == FINGERPRINT_OK) {
    PrintLcdLine(2, "Model saved!", "Модель сохранена", "success");
  } else {
    lcd.print("Store error!");
    PrintLcdLine(2, "Store error!", "Ошибка слхранения", "danger");
  }
  delay(1000);

  PrintLcdLine(2, HttpSetFinger(id, userID), "Допуск создан", "success");
  delay(2000);
}

void DeleteFingerprint() {
  finger.LEDcontrol(false);

  PrintLcdLine(1, "Delete finger", "Удаление отпечатка", "warning");

  if (!enterPassword()) return;

  int id = InputLcdLine(2, "FingerID:").toInt();
  if (id < 1 || id > MAX_MODELS) {
    PrintLcdLine(2, "FingerID failed!", "Неверный идентификатор", "success");
    delay(1000);
    return;
  };

  if (!ConfirmLcdLine(2)) {
    PrintLcdLine(2, "Delete aborted!", "Удаление отменено", "warning");
    delay(1000);
    return;
  }

  if (finger.deleteModel(id) == FINGERPRINT_OK) {
    PrintLcdLine(2, "Deleted!", "Успешно удалено", "success");
  } else {
    PrintLcdLine(2, "Error deleted!", "Ошибка удаления", "danger");
  }
  delay(1000);

  PrintLcdLine(2, HttpRemoveFinger(id), "Доступ удален", "success");
  delay(2000);

  finger.LEDcontrol(true);
}

// --- HTTP Requests --- -------------------------------------------------------------------------------------------------------------------

String HttpRequest(String method, String url, String payload = "") {
  HTTPClient https;
  https.begin(url);
  https.setTimeout(3000);

  https.addHeader("Authorization", "Basic " + base64::encode(LOGIN_1S + ":" + PASSWORD_1S));
  https.addHeader("Content-Type", "application/json");

  int httpCode;
  String response;

  if (method == "POST") {
    httpCode = https.POST(payload);
  } else if (method == "GET") {
    httpCode = https.GET();
  } else {
    response = "Unsupported HTTP method";
    https.end();
    return response;
  }

  response = (httpCode > 0) ? https.getString() : https.errorToString(httpCode);
  https.end();

  return response;
}

String HttpSetVisit(int id) {
  String payload = "{\"Device_ID\": " + GetDeviceSerial() + ", \"Finger_ID\": " + String(id) + "}";
  return HttpRequest("POST", URL_REGISTER, payload);
}

String HttpSetFinger(int id, int userID) {
  String payload = "{\"Device_ID\": " + GetDeviceSerial() + ", \"Finger_ID\": " + String(id) + ", \"User_ID\": " + String(userID) + "}";
  return HttpRequest("POST", URL_ADD_FINGER, payload);
}

String HttpRemoveFinger(int id) {
  String payload = "{\"Device_ID\": " + GetDeviceSerial() + ", \"Finger_ID\": " + String(id) + ", \"method\": \"DELETE\"}";
  return HttpRequest("POST", URL_DELETE_FINGER, payload);
}

String HttpGetPassword() {
  String fullUrl = String(URL_GET_PASS) + "?Device_ID=" + GetDeviceSerial();
  return HttpRequest("GET", fullUrl);
}
