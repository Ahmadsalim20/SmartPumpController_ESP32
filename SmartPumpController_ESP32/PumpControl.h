#ifndef PUMP_CONTROL_H
#define PUMP_CONTROL_H

#include <Arduino.h>
#include <ESP8266WiFi.h>
#include "Config.h"
#include "SystemState.h"
#include "Storage.h"

// --- دالة منطق التحكم بالمضخة والحساسات وتحديد الأخطاء ---
void processPumpLogic() {
  // فحص مستوى الماء كل ثانيتين
  if (millis() - lastSensorRead >= 2000) {
    lastSensorRead = millis();

    // إذا كان هناك خطأ، لا تقم بشيء للحفاظ على سبب المشكلة
    if (systemError) {
      return;
    }

    // فحص إيقاف المضخة التلقائية عند دخول وقت الهدوء
    if (isPumping && !manualMode && isQuietHours()) {
      digitalWrite(relayPin, HIGH);
      isPumping = false;
      currentStatus = "الضخ متوقف مؤقتاً (وقت الهدوء)";
      addLog("توقف مؤقت: وقت الهدوء");
      Serial.println("توقف الدينمو لدخول وقت الهدوء");
      return;
    }

    // قراءة الحساسات
    digitalWrite(powerPin, LOW);
    delay(10);
    int highWater = digitalRead(highSensorPin);
    int lowWater = digitalRead(lowSensorPin);
    int warningWater = digitalRead(warningSensorPin);
    int liftWater = digitalRead(liftSensorPin);
    digitalWrite(powerPin, HIGH);

    // حماية الجفاف / كشف عدم رفع الماء (الوضع التلقائي فقط)
    if (isPumping && !manualMode && (millis() - pumpStartTime >= liftTimeout)) {
      if (liftWater == HIGH) {
        digitalWrite(relayPin, HIGH);
        isPumping = false;
        manualTimerActive = false;
        manualMode = false;
        systemError = true;
        currentStatus = "خطأ: الدينمو لا يرفع ماء - مقفل";
        addLog("خطأ: فشل رفع الماء");
        Serial.println("خطأ: فشل رفع الماء");
        saveSettings();
        return;
      }
    }

    // منطق التشغيل والإيقاف
    if (highWater == LOW) {
      // الخزان ممتلئ
      if (isPumping) {
        digitalWrite(relayPin, HIGH);
        isPumping = false;
        manualTimerActive = false;
        manualTimerDuration = 0;
        currentStatus = manualMode ? "الخزان امتلأ - توقف الضخ" : "الخزان ممتلئ";
        addLog("توقف الضخ - الخزان ممتلئ");
        Serial.println("توقف الضخ - الخزان ممتلئ");
      } else {
        currentStatus = "الخزان ممتلئ";
      }
    }
    else if (lowWater == HIGH) {
      // مستوى حرج
      if (!isPumping && !manualMode) {
        if (isQuietHours()) {
          currentStatus = "مستوى حرج - مؤجل (وقت الهدوء)";
        } else {
          digitalWrite(relayPin, LOW);
          isPumping = true;
          pumpStartTime = millis();
          currentStatus = "جاري الضخ تلقائياً";
          addLog("بدء الضخ تلقائياً");
          Serial.println("بدء الضخ تلقائياً");
        }
      } else if (isPumping) {
        currentStatus = manualMode ? "جاري الضخ يدوياً" : "جاري الضخ تلقائياً";
      } else if (manualMode) {
        currentStatus = "مستوى حرج - شغل الدينمو";
      }
    }
    else if (warningWater == HIGH) {
      if (isPumping) {
        currentStatus = manualMode ? "جاري الضخ يدوياً" : "جاري الضخ تلقائياً";
      } else {
        currentStatus = "تحذير: مستوى الماء منخفض";
      }
    } else {
      if (isPumping) {
        currentStatus = manualMode ? "جاري الضخ يدوياً" : "جاري الضخ تلقائياً";
      } else {
        currentStatus = "مستوى الماء طبيعي";
      }
    }
  }

  // تايمر الوضع اليدوي
  if (isPumping && manualTimerActive && !systemError) {
    if (millis() - pumpStartTime >= manualTimerDuration) {
      digitalWrite(relayPin, HIGH);
      isPumping = false;
      manualTimerActive = false;
      manualTimerDuration = 0;
      currentStatus = "تم الإيقاف - انتهى وقت التايمر";
      addLog("انتهاء وقت التايمر");
      Serial.println("انتهاء وقت التايمر");
    }
  }

  // مؤقت الأمان الأقصى (الوضع التلقائي فقط)
  if (isPumping && !manualMode && !systemError) {
    if (millis() - pumpStartTime >= maxPumpTime) {
      digitalWrite(relayPin, HIGH);
      isPumping = false;
      manualTimerActive = false;
      manualMode = false;
      systemError = true;
      currentStatus = "خطأ: تجاوز الوقت الأقصى - مقفل";
      addLog("خطأ: تجاوز وقت الطوارئ");
      Serial.println("خطأ: تجاوز وقت الطوارئ");
      saveSettings();
    }
  }
  
  // محاولة إعادة الاتصال بالواي فاي إذا وُجدت وغير متصل
  if (wifiSSID.length() > 0 && WiFi.status() != WL_CONNECTED) {
    static unsigned long lastReconnectAttempt = 0;
    if (millis() - lastReconnectAttempt >= 30000) {
      lastReconnectAttempt = millis();
      Serial.println("محاولة إعادة الاتصال بالواي فاي...");
      WiFi.begin(wifiSSID.c_str(), wifiPassword.c_str());
    }
  }
}

#endif
