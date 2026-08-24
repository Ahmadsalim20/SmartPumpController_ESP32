#ifndef CONFIG_H
#define CONFIG_H

#include <Arduino.h>

// --- إعدادات الواي فاي (نقطة بث AP + الاتصال بالراوتر STA) ---
String wifiSSID = "";        // اسم شبكة الواي فاي المنزلية المحفوظة
String wifiPassword = "";    // كلمة مرور شبكة الواي فاي المنزلية

const char* apSSID = "SmartPump-Setup"; // اسم شبكة البث الخاصة بالمتحكم
const char* apPassword = "";            // كلمة مرور البث (مفتوحة لسهولة الاتصال)

// --- تعريف دبابيس التوصيل ---
const int relayPin = 5;          // D1: للتحكم بالدينمو
const int highSensorPin = 4;     // D2: السلك العلوي (نقطة الإيقاف)
const int lowSensorPin = 14;     // D5: السلك السفلي (نقطة التشغيل)
const int warningSensorPin = 12; // D6: سلك التحذير (قبل النفاد)
const int powerPin = 0;          // D3: سلك الطاقة في القاع
const int liftSensorPin = 13;    // D7: سلك كشف رفع الماء (عند مصب الأنبوب)
const int ledPin1 = 2;           // D4: اليد المدمج الأول (GPIO 2 / LED_BUILTIN) - مؤشر حالة المضخة والنظام
const int ledPin2 = 16;          // D0: اليد المدمج الثاني (GPIO 16 / NodeMCU LED) - مؤشر حالة الواي فاي

// --- متغيرات التوقيت (بديل الـ Delay) ---
unsigned long pumpStartTime = 0; 
unsigned long maxPumpTime = 60000UL; // وقت الطوارئ (بالميلي ثانية) - قابل للتعديل من الويب (افتراضي دقيقة واحدة)
const unsigned long maxAllowedTime = 300000UL; // 300 دقيقة (5 ساعات) كحد أقصى (للحماية)
unsigned long liftTimeout = 15000UL; // فترة سماح كشف رفع الماء (بالملي ثانية) - قابلة للتعديل من الويب (افتراضي 15 ثانية)
unsigned long lastSensorRead = 0;            // متى كانت آخر قراءة للحساس؟

// --- تايمر الوضع اليدوي ---
bool manualTimerActive = false;   // هل التايمر اليدوي مُفعَّل؟
unsigned long manualTimerDuration = 0; // مدة التايمر اليدوي بالميلي ثانية

#endif
