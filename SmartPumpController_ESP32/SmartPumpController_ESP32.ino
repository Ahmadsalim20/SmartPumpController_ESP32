#include <ESP8266WiFi.h>
#include <ESP8266WebServer.h>
#include <time.h>
#include <EEPROM.h>

#include "Config.h"
#include "SystemState.h"
#include "Storage.h"
#include "StatusLEDs.h"
#include "WebPage.h"
#include "WebHandlers.h"
#include "PumpControl.h"
#include "SecureOTA.h"

// ---------------------------------------------------------
void setup() {
  Serial.begin(115200);

  // تحميل الإعدادات المحفوظة من EEPROM
  loadSettings();

  // إعداد المنافذ
  pinMode(relayPin, OUTPUT);
  pinMode(powerPin, OUTPUT);
  pinMode(highSensorPin, INPUT_PULLUP);
  pinMode(lowSensorPin, INPUT_PULLUP);
  pinMode(warningSensorPin, INPUT_PULLUP);
  pinMode(liftSensorPin, INPUT_PULLUP);
  pinMode(ledPin1, OUTPUT);
  pinMode(ledPin2, OUTPUT);

  digitalWrite(powerPin, HIGH); 
  digitalWrite(relayPin, HIGH);
  digitalWrite(ledPin1, HIGH); // إطفاء مبدئي (Active LOW)
  digitalWrite(ledPin2, HIGH); // إطفاء مبدئي (Active LOW)

  // تهيئة وتزامن وقت النظام عبر الإنترنت (GMT+3)
  configTime(3 * 3600, 0, "pool.ntp.org", "time.nist.gov");

  // تفعيل الوضع المزدوج للواي فاي (نقطة بث AP + الاتصال بالراوتر STA)
  WiFi.mode(WIFI_AP_STA);
  
  // تشغيل شبكة البث المباشر المفتوحة للمتحكم
  WiFi.softAP(apSSID, apPassword);
  Serial.print("تم تشغيل نقطة البث الخاصة بالمتحكم (AP): ");
  Serial.println(apSSID);
  Serial.print("IP نقطة البث المباشر: ");
  Serial.println(WiFi.softAPIP());

  // في حال وجود شبكة واي فاي منزلية محفوظة، نحاول الاتصال بها
  if (wifiSSID.length() > 0) {
    Serial.print("جاري محاولة الاتصال بشبكة: ");
    Serial.println(wifiSSID);
    WiFi.begin(wifiSSID.c_str(), wifiPassword.c_str());

    unsigned long startAttempt = millis();
    while (WiFi.status() != WL_CONNECTED && millis() - startAttempt < 10000) {
      delay(500);
      Serial.print(".");
    }

    if (WiFi.status() == WL_CONNECTED) {
      Serial.println("\nتم الاتصال بالواي فاي بنجاح!");
      Serial.print("IP Address (Station): ");
      Serial.println(WiFi.localIP());
      addLog("اتصال بالواي فاي: " + wifiSSID);
    } else {
      Serial.println("\nتعذر الاتصال بالشبكة المحفوظة. يعمل بالنقطة المباشرة AP فقط.");
      addLog("تعذر الاتصال بالواي فاي");
    }
  } else {
    Serial.println("لم تكتشف شبكة واي فاي محفوظة. اتصل بـ SmartPump-Setup لإدخال الواي فاي.");
    addLog("تشغيل نقطة البث AP");
  }

  // تهيئة سيرفر الويب وتسجيل المسارات
  setupWebServer();
  addLog("تشغيل النظام");

  // فحص التحديثات اللاسلكية الآمنة OTA
  checkAndApplyOTA(); 
}

// ---------------------------------------------------------
void loop() {
  // 1. الاستماع لطلبات المتصفح
  server.handleClient();

  // 2. تحديث إضاءة اليد المدمج الأول والثاني (مؤشرات النظام والواي فاي)
  updateStatusLEDs();

  // 3. معالجة قراءات الحساسات وحالة المضخة والمؤقتات
  processPumpLogic();
}
