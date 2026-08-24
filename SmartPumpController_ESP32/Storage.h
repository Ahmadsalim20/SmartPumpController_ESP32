#ifndef STORAGE_H
#define STORAGE_H

#include <Arduino.h>
#include <EEPROM.h>
#include "Config.h"
#include "SystemState.h"

// --- دوال التعامل مع النصوص في الذاكرة الدائمة EEPROM ---
void writeEEPROMString(int addr, String str, int maxLen) {
  int len = str.length();
  if (len >= maxLen) len = maxLen - 1;
  for (int i = 0; i < len; i++) {
    EEPROM.write(addr + i, str[i]);
  }
  EEPROM.write(addr + len, '\0');
}

String readEEPROMString(int addr, int maxLen) {
  String str = "";
  for (int i = 0; i < maxLen; i++) {
    char c = EEPROM.read(addr + i);
    if (c == '\0' || c == (char)0xFF) break;
    str += c;
  }
  return str;
}

// دالة حفظ الإعدادات والأوضاع في الذاكرة الدائمة
void saveSettings() {
  byte magic = 0xAA;
  EEPROM.put(0, magic);
  EEPROM.put(1, manualMode);
  EEPROM.put(2, quietModeEnabled);
  EEPROM.put(3, systemError);
  EEPROM.put(4, maxPumpTime);
  EEPROM.put(8, liftTimeout);
  EEPROM.put(12, quietStartHour);
  EEPROM.put(16, quietEndHour);
  writeEEPROMString(20, wifiSSID, 32);
  writeEEPROMString(52, wifiPassword, 64);
  EEPROM.commit();
  Serial.println("تم حفظ الإعدادات والواي فاي في الذاكرة الدائمة EEPROM.");
}

// دالة تحميل الإعدادات والأوضاع من الذاكرة الدائمة
void loadSettings() {
  EEPROM.begin(512);
  byte magic;
  EEPROM.get(0, magic);
  if (magic == 0xAA) {
    EEPROM.get(1, manualMode);
    EEPROM.get(2, quietModeEnabled);
    EEPROM.get(3, systemError);
    EEPROM.get(4, maxPumpTime);
    EEPROM.get(8, liftTimeout);
    EEPROM.get(12, quietStartHour);
    EEPROM.get(16, quietEndHour);
    wifiSSID = readEEPROMString(20, 32);
    wifiPassword = readEEPROMString(52, 64);
    Serial.println("تم تحميل الإعدادات من الذاكرة الدائمة EEPROM بنجاح.");
    
    if (systemError) {
      currentStatus = "طوارئ: تم استعادة حالة الإقفال للحماية (انقطاع الكهرباء أثناء الخطأ)";
      addLog("طوارئ: استعادة الإقفال");
    }
  } else {
    Serial.println("لم يتم العثور على إعدادات مخزنة. سيتم حفظ القيم الافتراضية.");
    saveSettings(); // حفظ القيم الافتراضية للمرة الأولى
  }
}

#endif
