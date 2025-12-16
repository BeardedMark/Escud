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

int melody[] = { 262, 294, 330, 349, 392, 440, 494, 523 };  // Ноты (C4 - C5)
int durations[] = { 200, 200, 200, 200, 200, 200, 200, 400 };
// --- URLs ---
const char* serverUrl = "https://1s.dnlmarket.ru/ut/hs/api/skudlog/post";
const char* deleteFingerUrl = "https://1s.dnlmarket.ru/ut/hs/api/skuddata/post";
const char* addFingerUrl = "https://1s.dnlmarket.ru/ut/hs/api/skuddata/post";
const char* addPassUrl = "https://1s.dnlmarket.ru/ut/hs/api/skuddata/get";
const char* wifi = "DNLSkudScaner";
String Wifipass = "";
String auth = "Basic " + base64::encode("dnlmarket:User201095");


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

// --- Setup ---
void setup() {
  Serial.begin(115200);
  mySerial.begin(57600, SERIAL_8N1, 16, 17);
  lcd.init();
  lcd.backlight();
  pinMode(BUZZER_PIN, OUTPUT);
  for (int i = 0; i < 8; i++) {
    tone(BUZZER_PIN, melody[i], durations[i]);
    delay(durations[i] + 50);
  }
  noTone(BUZZER_PIN);

//
lcd.setCursor(0, 0);
lcd.print("Connect wifi......");
lcd.setCursor(0, 1);
lcd.print(wifi);
WiFiManager wm;
if (wm.autoConnect(wifi)) {
    Serial.println("✅ Подключено!");
    lcd.clear();
    lcd.setCursor(0, 0);
    lcd.print("Connected:");
    lcd.setCursor(0, 1);
    lcd.print(WiFi.SSID()); // Имя Wi-Fi сети
    delay(2000);
  }
  Serial.println(WiFi.localIP());
  lcd.clear();
  lcd.setCursor(0, 0);
  lcd.print("Check wifi");
  lcd.setCursor(0, 1);
  lcd.print("Result: OK");
  tone(BUZZER_PIN, 2000, 300);  // Писк 2000 Гц на 300 мс
  noTone(BUZZER_PIN);
  delay(1000);
  lcd.clear();
  finger.begin(57600);
  lcd.setCursor(0, 0);
  lcd.print("Check scaner");
  lcd.setCursor(0, 1);
  lcd.print(finger.verifyPassword() ? "Result: OK" : "Result: ERR");
  tone(BUZZER_PIN, 2000, 300);  // Писк 2000 Гц на 300 мс
  noTone(BUZZER_PIN);

  ////////////////////////////////
Serial.println("Сканирование базы данных отпечатков...");
  // Перебор ID от минимального до максимального
  for (uint8_t id = 1; id <= 127; id++) {
    // Попытка загрузить шаблон по заданному ID
    if (finger.loadModel(id) == FINGERPRINT_OK) {
      Serial.print("Найден отпечаток с ID: ");
      Serial.println(id);
    }
    delay(10);  // Небольшая задержка для стабильности
  }
  Serial.println("Сканирование завершено.");
  ///////////////////////////////

  if (!finger.verifyPassword())
    while (1) delay(1);

  delay(1500);
  lcd.clear();
}

// --- Main Loop ---
void loop() {
  lcd.setCursor(0, 0);
  lcd.print("Tuch scaner");
  lcd.setCursor(0, 1);
  lcd.print("Finger:");


  int id = getFingerprintID();
  if (id >= 0) {
    lcd.clear();
    lcd.setCursor(0, 0);
    lcd.print("Welcome!       V");
    tone(BUZZER_PIN, 2000, 300);  // Писк 2000 Гц на 300 мс
    noTone(BUZZER_PIN);
    sendHttpRequest(id);
    delay(1000);
    lcd.clear();
  } else if (id == -2) {
    lcd.setCursor(0, 0);
    lcd.print("Error!         X");
    lcd.setCursor(0, 1);
    lcd.print("Not found user");

    lcd.clear();
  }

  char key = keypad.getKey();
  if (key == '#') addFingerprintProcess();
  if (key == '*') deleteFingerprintProcess();
}

// --- Logic Blocks ---
int getFingerprintID() {
  if (finger.getImage() != FINGERPRINT_OK) return -1;
  if (finger.image2Tz() != FINGERPRINT_OK) return -1;
  return (finger.fingerFastSearch() == FINGERPRINT_OK) ? finger.fingerID : -2;
}

void addFingerprintProcess() {
  if (!requestPassword()) return;
  lcd.setCursor(0, 1);
  int id = getManualID("Add finger ID:");
  if (id < 0 || id > 127){
  lcd.setCursor(0, 0);
  lcd.print("Error!         X");
  lcd.setCursor(0, 1);
  lcd.print("Wrong finger ID");
  delay(2000);
  for (int i = 0; i < 3; i++) {
      tone(BUZZER_PIN, 2000);  // Частота 1000 Гц
      delay(200);              // Длительность сигнала
      noTone(BUZZER_PIN);      // Остановить звук
      delay(100);              // Пауза между сигналами
    }
  lcd.clear();
  return;
  }
  addFingerprint(id);
}

void deleteFingerprintProcess() {
  if (!requestPassword()) return;

  int id = getManualID("Delete Finger ID:");
  lcd.setCursor(0, 1);
  lcd.print("ID:");
  if (id <= 0 || id > 127) {
  lcd.setCursor(0, 0);
  lcd.print("Error!         X");
  lcd.setCursor(0, 1);
  lcd.print("Wrong finger ID");
  delay(2000);
  lcd.clear();
  return;
  }
  deleteFingerprint(id);
}

void addFingerprint(int id) {
  lcd.clear();
  lcd.setCursor(0, 0);
  lcd.print("Enter scan");
  lcd.setCursor(0, 1);
  lcd.print("Finger");
  while (finger.getImage() != FINGERPRINT_OK);
  if (finger.image2Tz(1) != FINGERPRINT_OK) return;
  lcd.setCursor(0, 0);
  lcd.print("Success!       V");
  lcd.setCursor(0, 1);
  lcd.print("Finger scanned");
  while (finger.getImage() != FINGERPRINT_NOFINGER);
  delay(500);
  lcd.clear();
  lcd.setCursor(0, 0);
  lcd.print("Enter confirm");
  lcd.setCursor(0, 1);
  lcd.print("Finger: ");
  tone(BUZZER_PIN, 2000, 300);  // Писк 2000 Гц на 300 мс
  noTone(BUZZER_PIN);
  while (finger.getImage() != FINGERPRINT_OK);
  if (finger.image2Tz(2) != FINGERPRINT_OK) return;
  delay(500);
  lcd.clear();
  lcd.setCursor(0, 0);
  lcd.print("Success!       V");
  lcd.setCursor(0, 1);
  lcd.print("Finger scanned");
  delay(2000);

   // 🧠 СОЗДАЁМ МОДЕЛЬ
  if (finger.createModel() != FINGERPRINT_OK) {
    lcd.clear(); lcd.print("Failed model");
    delay(2000); lcd.clear();
    return;
  }

  // 💾 СОХРАНЯЕМ МОДЕЛЬ
  if (finger.storeModel(id) == FINGERPRINT_OK) {
    lcd.clear();
    lcd.setCursor(0, 0);
    lcd.print("Saved! ID: ");
    lcd.print(id);
  } else {
    lcd.clear();
    lcd.setCursor(0, 0);
    lcd.print("Store error!");
  }

  lcd.setCursor(0, 1);
  int userID = getManualID("Enter User ID:");
  if (userID <= 0) {
  lcd.setCursor(0, 0);
  lcd.print("Error!         X");
  lcd.setCursor(0, 1);
  lcd.print("Wrong User ID");
  for (int i = 0; i < 3; i++) {
      tone(BUZZER_PIN, 2000);  // Частота 1000 Гц
      delay(200);              // Длительность сигнала
      noTone(BUZZER_PIN);      // Остановить звук
      delay(100);              // Пауза между сигналами
  }
  delay(2000);
  lcd.clear();
  return;
  }

sendAddRequest(id, userID);
}

void deleteFingerprint(int id) {
  if (finger.deleteModel(id) == FINGERPRINT_OK) {
    lcd.clear();
    lcd.setCursor(0, 0);
    lcd.print("Delete finger");
    lcd.setCursor(0, 1);
    lcd.print("ID:" + String(id));
    sendDeleteRequest(id);
  } else {
    lcd.clear();
    lcd.setCursor(0, 0);
    lcd.print("Error!         X");
    lcd.setCursor(0, 1);
    lcd.print("Wrong finger ID");
  }
  delay(2000);
}

String readKeyInput(bool hide = false) {
  String input = "";
  lcd.setCursor(0, 1);  // Начинаем ввод на второй строке
  while (true) {
    char key = keypad.getKey();
    if (key) {
      if (key == '#') break;
      if (key == '*') return "";
      input += key;
      if (hide) {
      lcd.setCursor(input.length() - 1, 1);
      lcd.print("*");
      }else {
      lcd.setCursor(0, 1); lcd.print(input);
      }
    }
  }
  return input;
}

// --- Helper Input ---
String enterPassword() {
  String password = getPasswordFrom1C();
  Serial.println("Пароль из 1С: " + password);
  lcd.clear();
  lcd.setCursor(0, 0);
  lcd.print("Access denied");
  lcd.setCursor(0, 1);
  lcd.print("password:");
  delay(500);
  lcd.setCursor(0, 1);            // Переходим на вторую строку
  lcd.print("                ");  // 16 пробелов — очищает всю строку

  return readKeyInput(true);
}

int getManualID(const char* prompt) {
  lcd.clear();
  lcd.setCursor(0, 0);
  lcd.print(prompt);
  String input = readKeyInput();
  return input.toInt();
}



bool requestPassword() {
  String pass = enterPassword();
  if (pass != Wifipass) {
    lcd.clear();
    lcd.setCursor(0, 0);
    lcd.print("Error!         X");
    lcd.setCursor(0, 1);
    lcd.print("Wrong password!");
    delay(2000);
    lcd.clear();
    return false;
  }
  return true;
}

// --- HTTP Requests ---
void sendHttpRequest(int id) {
  HTTPClient https;
  https.begin(serverUrl);
  https.setTimeout(3000);
  https.addHeader("Authorization", auth);
  https.addHeader("Content-Type", "application/json");
  String payload = "{\"Device_ID\": " + String(getDeviceSerial()) + ", \"Finger_ID\": " + String(id) + "}";
  // int code = https.POST(payload);
  // Serial.println("POST Response: " + String(code));
  Serial.println("📤 Отправка запроса на добавление:");
  Serial.println(payload);

  int httpCode = https.POST(payload);

  if (httpCode > 0) {
    String response = https.getString();
    Serial.print("✅ Ответ 1С: ");
    Serial.println(httpCode);
    Serial.println("📥 Тело ответа:");
    Serial.println(response);
  

    // Если хочешь показать часть ответа на LCD:
    lcd.clear();
    lcd.setCursor(0, 0);
    lcd.print("Welcome!       V");
    lcd.setCursor(0, 1);
    lcd.print(response.substring(0, 16));  // ограничим 16 символами
    delay(3000);
    lcd.clear();

  } else {
    Serial.print("❌ Ошибка при запросе: ");
    Serial.println(https.errorToString(httpCode));
    
    lcd.clear();
    lcd.setCursor(0, 0);
    lcd.print("Ошибка запроса");
    delay(2000);
    lcd.clear();
  }
  https.end();
}

void sendAddRequest(int id, int userID) {
  HTTPClient https;
  https.begin(addFingerUrl);
  https.addHeader("Authorization", auth);
  https.addHeader("Content-Type", "application/json");
  String payload = "{\"Device_ID\": " + String(getDeviceSerial()) + ", \"Finger_ID\": " + String(id) + ", \"User_ID\": " + String(userID) + "}";
  // https.POST(payload);
  Serial.println("📤 Отправка запроса на добавление:");
  Serial.println(payload);

  int httpCode = https.POST(payload);

  if (httpCode > 0) {
    String response = https.getString();
    Serial.print("✅ Ответ 1С: ");
    Serial.println(httpCode);
    Serial.println("📥 Тело ответа:");
    Serial.println(response);
  

    // Если хочешь показать часть ответа на LCD:
    lcd.clear();
    lcd.setCursor(0, 0);
    lcd.print("Saved!         V");
    lcd.setCursor(0, 1);
    lcd.print(response.substring(0, 16));  // ограничим 16 символами
    delay(3000);
    lcd.clear();

  } else {
    Serial.print("❌ Ошибка при запросе: ");
    Serial.println(https.errorToString(httpCode));
    
    lcd.clear();
    lcd.setCursor(0, 0);
    lcd.print("HTTP ERR");
    delay(2000);
    lcd.clear();
  }
  https.end();
}

void sendDeleteRequest(int id) {
  HTTPClient https;
  https.begin(deleteFingerUrl);
  https.addHeader("Authorization", auth);
  https.addHeader("Content-Type", "application/json");
  String payload = "{\"Device_ID\": " + String(getDeviceSerial()) + ", \"Finger_ID\": " + String(id) + ", \"method\": \"DELETE\"}";
  // https.POST(payload);
  Serial.println("📤 Отправка запроса на добавление:");
  Serial.println(payload);

  int httpCode = https.POST(payload);

  if (httpCode > 0) {
    String response = https.getString();
    Serial.print("✅ Ответ 1С: ");
    Serial.println(httpCode);
    Serial.println("📥 Тело ответа:");
    Serial.println(response);
   

    // Если хочешь показать часть ответа на LCD:
    lcd.clear();
    lcd.setCursor(0, 0);
    lcd.print("Deleted!       V");
    lcd.setCursor(0, 1);
    lcd.print(response.substring(0, 16));  // ограничим 16 символами
    delay(3000);
    lcd.clear();

  } else {
    Serial.print("❌ Ошибка при запросе: ");
    Serial.println(https.errorToString(httpCode));
    
    lcd.clear();
    lcd.setCursor(0, 0);
    lcd.print("Ошибка запроса");
    delay(2000);
    lcd.clear();
  }
  https.end();
}

////
// 📡 Получаем пароль из 1С
String getPasswordFrom1C() {
  HTTPClient https;

  // Формируем полный URL с Device_ID
  String fullUrl = String(addPassUrl) + "?Device_ID=" + String(getDeviceSerial());
  Serial.println("🔗 Запрос пароля по URL:");
  Serial.println(fullUrl);

  https.begin(fullUrl);
  https.addHeader("Authorization", auth);

  int httpCode = https.GET();
  

  if (httpCode == 200) {
    Wifipass = https.getString(); // Получаем пароль
    Wifipass.trim(); // Убираем пробелы и \n
    Serial.println("✅ Получен пароль: " + Wifipass);
  } else {
    Serial.print("❌ Ошибка запроса: ");
    Serial.println(httpCode);
  }

  https.end();
  return Wifipass;
}
/////////////////////
uint32_t getDeviceSerial() {
  return ESP.getEfuseMac();
}
