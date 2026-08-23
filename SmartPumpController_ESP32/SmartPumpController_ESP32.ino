#if defined(ESP8266)
  #include <ESP8266WiFi.h>
#elif defined(ESP32)
  #include <WiFi.h>
#endif

// تضمين ملف التحديث السحابي
#include "SecureOTA.h"

// --- إعدادات شبكة الواي فاي للذراع التجريبي ---
const char* wifiSSID = "S23";       // اسم شبكة الواي فاي الخاصة بك
const char* wifiPassword = "";   // كلمة المرور (اتركها فارغة إذا كانت بدون كلمة سر)

#ifndef LED_BUILTIN
#define LED_BUILTIN 2
#endif

const int ledPin = LED_BUILTIN;
unsigned long lastLedBlink = 0;
const unsigned long ledBlinkInterval = 500; // ووميض كل 500 ملي ثانية
bool ledState = false;

void setup() {
  Serial.begin(115200);
  delay(1000);
  Serial.println("\n==========================================");
  Serial.println("   SmartPump Controller - Minimal OTA Test");
  Serial.println("==========================================");

  pinMode(ledPin, OUTPUT);
  digitalWrite(ledPin, HIGH); // إيقاف الـ LED مبدئياً (Active LOW على ESP8266)

  // تفعيل وضع Station للاتصال بشبكة الواي فاي
  WiFi.mode(WIFI_STA);
  WiFi.begin(wifiSSID, wifiPassword);

  Serial.print("Connecting to Wi-Fi: ");
  Serial.println(wifiSSID);

  unsigned long startAttempt = millis();
  while (WiFi.status() != WL_CONNECTED && millis() - startAttempt < 15000) {
    delay(500);
    Serial.print(".");
    // ووميض خفيف أثناء محاولة الاتصال
    ledState = !ledState;
    digitalWrite(ledPin, ledState ? LOW : HIGH);
  }

  if (WiFi.status() == WL_CONNECTED) {
    Serial.println("\nConnected to Wi-Fi successfully!");
    Serial.print("IP Address: ");
    Serial.println(WiFi.localIP());

    // 1. تفعيل مزامنة الوقت عبر NTP (مطلوب لـ SSL)
    configTime(3 * 3600, 0, "pool.ntp.org", "time.nist.gov");

    // 2. تشغيل فحص وتنفيذ التحديث السحابي OTA
    checkAndApplyOTA();
  } else {
    Serial.println("\nFailed to connect to Wi-Fi. Please check SSID/Password.");
  }
}

void loop() {
  // ووميض مستمر للـ LED المدمج للدلالة على أن الكود يعمل بنجاح
  if (millis() - lastLedBlink >= ledBlinkInterval) {
    lastLedBlink = millis();
    ledState = !ledState;
    digitalWrite(ledPin, ledState ? LOW : HIGH);
  }
}
