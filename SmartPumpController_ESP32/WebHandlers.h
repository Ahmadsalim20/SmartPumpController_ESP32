#ifndef WEB_HANDLERS_H
#define WEB_HANDLERS_H

#include <Arduino.h>
#include <ESP8266WebServer.h>
#include <ESP8266WiFi.h>
#include "Config.h"
#include "SystemState.h"
#include "Storage.h"
#include "WebPage.h"

// إنشاء كائن السيرفر على البورت 80
ESP8266WebServer server(80);

void handleReset() {
  systemError = false;
  isPumping = false;
  manualMode = false;
  digitalWrite(relayPin, HIGH);
  currentStatus = "تمت إعادة ضبط النظام";
  addLog("إعادة ضبط النظام");
  saveSettings();
  
  server.sendHeader("Location", "/", true);
  server.send(303);
}

// دالة تبديل الوضع (يدوي/تلقائي)
void handleToggleMode() {
  if (!systemError) {
    manualMode = !manualMode;
    currentStatus = manualMode ? "الوضع اليدوي" : "الوضع التلقائي";
    addLog(manualMode ? "تبديل للوضع اليدوي" : "تبديل للوضع التلقائي");
    saveSettings();
  }
  server.sendHeader("Location", "/", true);
  server.send(303);
}

// دالة التشغيل اليدوي
void handleManualOn() {
  if (!systemError) {
    if (!manualMode) {
      manualMode = true;
      addLog("تبديل للوضع اليدوي");
    }
    digitalWrite(relayPin, LOW);
    isPumping = true;
    pumpStartTime = millis();
    manualTimerActive = false;
    manualTimerDuration = 0;
    currentStatus = "جاري الضخ يدوياً";
    addLog("بدء الضخ يدوياً");
    saveSettings();
  }
  server.sendHeader("Location", "/", true);
  server.send(303);
}

// دالة تفعيل تايمر الإيقاف اليدوي
void handleSetManualTimer() {
  if (!systemError && server.hasArg("min")) {
    int requestedMin = server.arg("min").toInt();
    int maxMin = (int)(maxPumpTime / 60000UL);
    if (requestedMin < 1) requestedMin = 1;
    if (requestedMin > maxMin) requestedMin = maxMin;
    manualTimerDuration = (unsigned long)requestedMin * 60000UL;
    manualTimerActive = true;
    if (!isPumping) {
      if (!manualMode) { manualMode = true; addLog("تبديل للوضع اليدوي"); }
      digitalWrite(relayPin, LOW);
      isPumping = true;
      pumpStartTime = millis();
    }
    currentStatus = "ضخ يدوي - سيتوقف بعد " + String(requestedMin) + " دقيقة";
    addLog("تايمر يدوي: " + String(requestedMin) + " دقيقة");
    Serial.println("تايمر يدوي: " + String(requestedMin) + " دقيقة");
    saveSettings();
  }
  server.sendHeader("Location", "/", true);
  server.send(303);
}

// دالة إلغاء تايمر الوضع اليدوي
void handleCancelTimer() {
  manualTimerActive = false;
  manualTimerDuration = 0;
  currentStatus = "جاري الضخ يدوياً (التايمر ملغى)";
  addLog("إلغاء التايمر اليدوي");
  server.sendHeader("Location", "/", true);
  server.send(303);
}

// دالة الإيقاف اليدوي
void handleManualOff() {
  if (!systemError) {
    digitalWrite(relayPin, HIGH);
    isPumping = false;
    manualTimerActive = false;
    manualTimerDuration = 0;
    currentStatus = "تم إيقاف الدينمو";
    addLog("إيقاف يدوي للدينمو");
  }
  server.sendHeader("Location", "/", true);
  server.send(303);
}

// دالة تحديث بيانات شبكة الواي فاي
void handleSetWiFi() {
  if (server.hasArg("ssid")) {
    wifiSSID = server.arg("ssid");
    wifiSSID.trim();
    wifiPassword = server.arg("password");
    saveSettings();
    addLog("حفظ واي فاي: " + wifiSSID);
    currentStatus = "جاري الاتصال بـ " + wifiSSID + "...";
    
    WiFi.disconnect();
    WiFi.begin(wifiSSID.c_str(), wifiPassword.c_str());
  }
  server.sendHeader("Location", "/", true);
  server.send(303);
}

// دالة مسح شبكة الواي فاي المحفوظة
void handleForgetWiFi() {
  wifiSSID = "";
  wifiPassword = "";
  saveSettings();
  WiFi.disconnect();
  currentStatus = "تم مسح شبكة الواي فاي المحفوظة";
  addLog("مسح شبكة الواي فاي");
  server.sendHeader("Location", "/", true);
  server.send(303);
}

// دالة تحديث وقت الطوارئ وزمن كشف رفع الماء ووقت الهدوء
void handleSetTimeout() {
  bool updated = false;
  String statusMsg = "";

  if (server.hasArg("minutes")) {
    int minutes = server.arg("minutes").toInt();
    if (minutes >= 1 && minutes <= 300) {
      maxPumpTime = (unsigned long)minutes * 60000UL;
      statusMsg += "تم تحديث وقت الطوارئ إلى " + String(minutes) + " دقيقة. ";
      addLog("تحديث وقت الطوارئ: " + String(minutes) + " دقيقة");
      updated = true;
    }
  }

  if (server.hasArg("liftSec")) {
    int seconds = server.arg("liftSec").toInt();
    if (seconds >= 1 && seconds <= 300) {
      liftTimeout = (unsigned long)seconds * 1000UL;
      statusMsg += "تم تحديث زمن كشف رفع الماء إلى " + String(seconds) + " ثانية. ";
      addLog("تحديث زمن رفع الماء: " + String(seconds) + " ثانية");
      updated = true;
    }
  }

  if (server.hasArg("minutes")) {
    bool newEnabled = server.hasArg("quietEnabled");
    if (newEnabled != quietModeEnabled) {
      quietModeEnabled = newEnabled;
      updated = true;
      addLog(quietModeEnabled ? "تفعيل وقت الهدوء" : "إلغاء وقت الهدوء");
    }
  }

  if (server.hasArg("quietStart")) {
    int startH = server.arg("quietStart").toInt();
    if (startH >= 0 && startH <= 23 && startH != quietStartHour) {
      quietStartHour = startH;
      updated = true;
      addLog("بدء وقت الهدوء: " + String(startH) + ":00");
    }
  }

  if (server.hasArg("quietEnd")) {
    int endH = server.arg("quietEnd").toInt();
    if (endH >= 0 && endH <= 23 && endH != quietEndHour) {
      quietEndHour = endH;
      updated = true;
      addLog("انتهاء وقت الهدوء: " + String(endH) + ":00");
    }
  }

  if (updated) {
    if (statusMsg == "") {
      statusMsg = "تم حفظ الإعدادات بنجاح";
    }
    currentStatus = statusMsg;
    saveSettings();
  } else {
    currentStatus = "خطأ في تعديل الإعدادات";
  }

  server.sendHeader("Location", "/", true);
  server.send(303);
}

// دالة تهيئة وتسجيل مسارات السيرفر
void setupWebServer() {
  server.on("/", handleRoot);
  server.on("/status", handleStatus);
  server.on("/reset", handleReset);
  server.on("/toggle-mode", handleToggleMode);
  server.on("/manual-on", handleManualOn);
  server.on("/manual-off", handleManualOff);
  server.on("/set-timeout", handleSetTimeout);
  server.on("/set-manual-timer", handleSetManualTimer);
  server.on("/cancel-timer", handleCancelTimer);
  server.on("/set-wifi", handleSetWiFi);
  server.on("/forget-wifi", handleForgetWiFi);
  server.begin();
  Serial.println("سيرفر الويب يعمل الآن...");
}

#endif
