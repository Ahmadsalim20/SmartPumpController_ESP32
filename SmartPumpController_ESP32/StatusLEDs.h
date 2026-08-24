#ifndef STATUS_LEDS_H
#define STATUS_LEDS_H

#include <Arduino.h>
#include <ESP8266WiFi.h>
#include "Config.h"
#include "SystemState.h"

// --- دالة التحكم في إضاءة اليد المدمج الأول والثاني (بدون delay) ---
void updateStatusLEDs() {
  unsigned long currentMillis = millis();

  // 1. التحكم في اليد المدمج الأول (ledPin1 - D4 / GPIO 2): حالة المضخة والنظام
  if (systemError) {
    // خطأ بالنظام: وميض سريع جداً (100 ملي ثانية)
    static unsigned long lastBlink1 = 0;
    static bool ledState1 = HIGH;
    if (currentMillis - lastBlink1 >= 100) {
      lastBlink1 = currentMillis;
      ledState1 = !ledState1;
      digitalWrite(ledPin1, ledState1);
    }
  } else if (isPumping) {
    // مضخة تعمل: إضاءة مستمرة (LOW = تشغيل)
    digitalWrite(ledPin1, LOW);
  } else {
    // حالة الاستعداد (طبيعي): نبضة هارت بيت كل ثانيتين
    static unsigned long lastHeartbeat = 0;
    if (currentMillis - lastHeartbeat >= 2000) {
      lastHeartbeat = currentMillis;
      digitalWrite(ledPin1, LOW); // إضاءة قصيرة
    } else if (currentMillis - lastHeartbeat >= 50) {
      digitalWrite(ledPin1, HIGH); // إطفاء
    }
  }

  // 2. التحكم في اليد المدمج الثاني (ledPin2 - D0 / GPIO 16): حالة الواي فاي
  if (WiFi.status() == WL_CONNECTED) {
    // متصل بالراوتر: إضاءة مستمرة (LOW = تشغيل)
    digitalWrite(ledPin2, LOW);
  } else {
    // غير متصل / يعمل بنقطة البث AP: وميض بطيء (500 ملي ثانية)
    static unsigned long lastBlink2 = 0;
    static bool ledState2 = HIGH;
    if (currentMillis - lastBlink2 >= 500) {
      lastBlink2 = currentMillis;
      ledState2 = !ledState2;
      digitalWrite(ledPin2, ledState2);
    }
  }
}

#endif
