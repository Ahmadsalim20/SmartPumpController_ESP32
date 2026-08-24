#ifndef SYSTEM_STATE_H
#define SYSTEM_STATE_H

#include <Arduino.h>
#include <time.h>

// --- متغيرات حالة النظام ---
bool isPumping = false; 
bool systemError = false; 
bool manualMode = false;
String currentStatus = "جاري التهيئة...";
String operationLog[10]; // سجل العمليات (آخر 10 عمليات)
int logIndex = 0;

// --- متغيرات وقت الهدوء المحظور (Quiet Hours) ---
bool quietModeEnabled = false;   // تفعيل وقت الهدوء
int quietStartHour = 22;         // ساعة البدء (0-23) - افتراضي 10 مساءً
int quietEndHour = 6;            // ساعة الانتهاء (0-23) - افتراضي 6 صباحاً

// دالة لإضافة عملية للسجل
void addLog(String message) {
  operationLog[logIndex] = message;
  logIndex = (logIndex + 1) % 10;
}

// دالة التحقق مما إذا كنا في وقت الهدوء المحظور للتشغيل التلقائي
bool isQuietHours() {
  if (!quietModeEnabled) return false;
  
  time_t now = time(nullptr);
  if (now < 1000000000ULL) {
    return false; 
  }
  
  struct tm* timeinfo = localtime(&now);
  int currentHour = timeinfo->tm_hour;
  
  if (quietStartHour == quietEndHour) {
    return false; // معطل
  }
  
  if (quietStartHour < quietEndHour) {
    return (currentHour >= quietStartHour && currentHour < quietEndHour);
  } else {
    return (currentHour >= quietStartHour || currentHour < quietEndHour);
  }
}

#endif
