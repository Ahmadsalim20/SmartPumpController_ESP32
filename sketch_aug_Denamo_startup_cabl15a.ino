#include <ESP8266WiFi.h>
#include <ESP8266WebServer.h>

// --- إعدادات الواي فاي ---
const char* ssid = "LINK";         // اسم شبكة الواي فاي
const char* password = "774860879AA"; // كلمة مرور الواي فاي

// إنشاء كائن السيرفر على البورت 80
ESP8266WebServer server(80);

// --- تعريف دبابيس التوصيل ---
const int relayPin = 5;       // D1: للتحكم بالدينمو
const int highSensorPin = 4;  // D2: السلك العلوي (نقطة الإيقاف)
const int lowSensorPin = 14;  // D5: السلك السفلي (نقطة التشغيل)
const int powerPin = 0;       // D3: سلك الطاقة في القاع

// --- متغيرات حالة النظام ---
bool isPumping = false; 
bool systemError = false; 
bool manualMode = false;
String currentStatus = "جاري تهيئة النظام..."; // نص لعرضه في المتصفح
String operationLog[10]; // سجل العمليات (آخر 10 عمليات)
int logIndex = 0;

// --- متغيرات التوقيت (بديل الـ Delay) ---
unsigned long pumpStartTime = 0; 
unsigned long maxPumpTime = 60000UL; // وقت الطوارئ (بالميلي ثانية) - قابل للتعديل من الويب (افتراضي دقيقة واحدة)
const unsigned long maxAllowedTime = 300000UL; // 300 دقيقة (5 ساعات) كحد أقصى (للحماية)
unsigned long lastSensorRead = 0;            // متى كانت آخر قراءة للحساس؟

// دالة لإضافة عملية للسجل
void addLog(String message) {
  operationLog[logIndex] = message;
  logIndex = (logIndex + 1) % 10;
}

// ---------------------------------------------------------
// دالة بناء صفحة الـ HTML (واجهة المستخدم)
// ---------------------------------------------------------
void handleRoot() {
  String html = "<!DOCTYPE html><html lang='ar' dir='rtl'>";
  html += "<head><meta charset='UTF-8'><meta name='viewport' content='width=device-width, initial-scale=1.0'>";
  html += "<meta http-equiv='refresh' content='3'> ";
  html += "<title>مراقبة الخزان</title>";
  html += "<style>";
  html += "* { box-sizing: border-box; }";
  html += "body { font-family: 'Segoe UI', Tahoma, Arial, sans-serif; text-align: center; background: linear-gradient(135deg, #1a1a2e 0%, #16213e 50%, #0f3460 100%); padding: 15px; min-height: 100vh; margin: 0; color: #fff; }";
  html += ".container { max-width: 500px; margin: 0 auto; }";
  html += ".header { background: rgba(255,255,255,0.1); backdrop-filter: blur(10px); padding: 20px; border-radius: 20px; margin-bottom: 20px; border: 1px solid rgba(255,255,255,0.2); }";
  html += "h1 { color: #fff; margin: 0 0 15px 0; font-size: 28px; text-shadow: 0 2px 10px rgba(0,0,0,0.3); }";
  html += ".wifi-status { font-size: 14px; color: #4ade80; margin-top: 10px; }";
  html += ".card { background: rgba(255,255,255,0.95); padding: 25px; border-radius: 20px; box-shadow: 0 15px 35px rgba(0,0,0,0.3); margin-bottom: 20px; }";
  html += ".status { font-size: 20px; font-weight: bold; margin: 20px 0; padding: 25px; border-radius: 15px; transition: all 0.4s cubic-bezier(0.4, 0, 0.2, 1); text-shadow: 0 2px 5px rgba(0,0,0,0.2); }";
  html += ".pump-info { font-size: 18px; color: #333; margin: 15px 0; padding: 15px; background: #f8fafc; border-radius: 12px; }";
  html += ".pump-info b { color: #1e40af; }";
  html += ".controls { margin: 20px 0; display: grid; grid-template-columns: repeat(auto-fit, minmax(140px, 1fr)); gap: 12px; }";
  html += ".btn { padding: 15px 20px; border: none; border-radius: 12px; font-size: 16px; font-weight: 600; cursor: pointer; transition: all 0.3s cubic-bezier(0.4, 0, 0.2, 1); text-decoration: none; display: block; text-align: center; box-shadow: 0 4px 15px rgba(0,0,0,0.2); }";
  html += ".btn:hover { transform: translateY(-3px); box-shadow: 0 8px 25px rgba(0,0,0,0.3); }";
  html += ".btn:active { transform: translateY(-1px); }";
  html += ".btn-primary { background: linear-gradient(135deg, #667eea 0%, #764ba2 100%); color: white; }";
  html += ".btn-danger { background: linear-gradient(135deg, #f093fb 0%, #f5576c 100%); color: white; }";
  html += ".btn-success { background: linear-gradient(135deg, #4facfe 0%, #00f2fe 100%); color: white; }";
  html += ".btn-warning { background: linear-gradient(135deg, #fa709a 0%, #fee140 100%); color: white; }";
  html += ".log-section { background: rgba(255,255,255,0.95); padding: 20px; border-radius: 15px; margin-bottom: 20px; text-align: right; max-height: 180px; overflow-y: auto; }";
  html += ".log-section::-webkit-scrollbar { width: 8px; }";
  html += ".log-section::-webkit-scrollbar-track { background: #f1f1f1; border-radius: 10px; }";
  html += ".log-section::-webkit-scrollbar-thumb { background: #667eea; border-radius: 10px; }";
  html += ".log-title { font-weight: bold; color: #1e40af; margin-bottom: 12px; font-size: 16px; }";
  html += ".log-item { font-size: 13px; color: #475569; padding: 8px 0; border-bottom: 1px solid #e2e8f0; }";
  html += ".log-item:last-child { border-bottom: none; }";
  html += ".settings-section { background: rgba(255,255,255,0.95); padding: 20px; border-radius: 15px; margin-bottom: 20px; }";
  html += ".settings-title { font-weight: bold; color: #1e40af; margin-bottom: 15px; font-size: 16px; }";
  html += ".input-group { display: flex; gap: 12px; justify-content: center; align-items: center; flex-wrap: wrap; }";
  html += ".input-field { padding: 12px 15px; border: 2px solid #e2e8f0; border-radius: 10px; font-size: 16px; width: 120px; text-align: center; transition: all 0.3s; background: #f8fafc; }";
  html += ".input-field:focus { border-color: #667eea; outline: none; box-shadow: 0 0 0 3px rgba(102, 126, 234, 0.2); background: white; }";
  html += ".water-level { display: flex; justify-content: space-between; margin: 15px 0; }";
  html += ".level-indicator { flex: 1; margin: 0 5px; padding: 10px; border-radius: 8px; font-size: 12px; font-weight: bold; }";
  html += ".level-high { background: #dcfce7; color: #166534; }";
  html += ".level-mid { background: #dbeafe; color: #1e40af; }";
  html += ".level-low { background: #fee2e2; color: #991b1b; }";
  html += ".level-active { transform: scale(1.1); box-shadow: 0 4px 15px rgba(0,0,0,0.2); }";
  
  if (systemError) {
    html += ".status { background: linear-gradient(45deg, #ff6b6b, #ee5a5a); color: white; }";
  } else if (isPumping) {
    html += ".status { background: linear-gradient(45deg, #4facfe, #00f2fe); color: white; }";
  } else {
    html += ".status { background: linear-gradient(45deg, #43e97b, #38f9d7); color: white; }";
  }
  
  html += "</style></head><body>";
  
  html += "<div class='container'>";
  html += "<div class='header'>";
  html += "<h1>💧 نظام التحكم بالخزان</h1>";
  html += "<div class='wifi-status'>📶 متصل بالواي فاي</div>";
  html += "</div>";
  
  html += "<div class='card'>";
  html += "<div class='status'>" + currentStatus + "</div>";
  
  html += "<div class='water-level'>";
  html += "<div class='level-indicator level-high'>🔴 عالي</div>";
  html += "<div class='level-indicator level-mid'>🟡 متوسط</div>";
  html += "<div class='level-indicator level-low'>🟢 منخفض</div>";
  html += "</div>";
  
  html += "<div class='pump-info'>";
  if (isPumping && !systemError) {
    long elapsed = (millis() - pumpStartTime) / 1000;
    html += "الدينمو: <b>يعمل</b> ⚡<br>";
    html += "وقت التشغيل: " + String(elapsed / 60) + " دقيقة و " + String(elapsed % 60) + " ثانية";
  } else {
    html += "الدينمو: <b>متوقف</b> 🛑";
  }
  html += "</div>";
  
  // أزرار التحكم
  html += "<div class='controls'>";
  if (systemError) {
    html += "<a href='/reset' class='btn btn-danger'>🚨 إعادة ضبط الطوارئ</a>";
  } else {
    html += "<a href='/manual-on' class='btn btn-success'>▶️ تشغيل يدوي</a>";
    html += "<a href='/manual-off' class='btn btn-warning'>⏸️ إيقاف يدوي</a>";
    html += "<a href='/reset' class='btn btn-primary'>🔄 تصفير النظام</a>";
  }
  html += "</div>";
  html += "</div>";
  
  // سجل العمليات
  html += "<div class='log-section'>";
  html += "<div class='log-title'>📋 سجل العمليات:</div>";
  for (int i = 0; i < 10; i++) {
    int idx = (logIndex - 1 - i + 10) % 10;
    if (operationLog[idx] != "") {
      html += "<div class='log-item'>" + operationLog[idx] + "</div>";
    }
  }
  html += "</div>";
  
  // قسم الإعدادات (وقت الطوارئ)
  html += "<div class='settings-section'>";
  html += "<div class='settings-title'>⚙️ إعدادات وقت الطوارئ:</div>";
  html += "<div class='input-group'>";
  html += "<form action='/set-timeout' method='GET'>";
  html += "<input type='number' name='minutes' class='input-field' min='1' max='300' value='" + String(maxPumpTime / 60000UL) + "' required>";
  html += "<span style='color: #555; font-weight: 500;'>دقائق</span>";
  html += "<button type='submit' class='btn btn-primary' style='padding: 12px 25px;'>💾 حفظ</button>";
  html += "</form>";
  html += "</div>";
  html += "<div style='font-size: 13px; color: #64748b; margin-top: 12px; background: #f1f5f9; padding: 8px; border-radius: 8px;'>الحد الأقصى: 300 دقيقة (5 ساعات)</div>";
  html += "</div>";
  
  html += "</div></body></html>";

  server.send(200, "text/html", html);
}
// دالة إعادة ضبط النظام
void handleReset() {
  systemError = false;   // إلغاء حالة الطوارئ
  isPumping = false;     // إيقاف حالة الضخ
  manualMode = false;    // إلغاء الوضع اليدوي
  digitalWrite(relayPin, HIGH); // التأكد من إطفاء الدينمو للأمان
  currentStatus = "تم إعادة ضبط النظام بنجاح 🟢";
  addLog("تم إعادة ضبط النظام");
  
  server.sendHeader("Location", "/", true);
  server.send(303);
}

// دالة التشغيل اليدوي
void handleManualOn() {
  if (!systemError) {
    manualMode = true;
    digitalWrite(relayPin, LOW);
    isPumping = true;
    pumpStartTime = millis();
    currentStatus = "تشغيل يدوي - جاري التعبئة 🔄";
    addLog("تم التشغيل اليدوي للدينمو");
  }
  server.sendHeader("Location", "/", true);
  server.send(303);
}

// دالة الإيقاف اليدوي
void handleManualOff() {
  if (!systemError) {
    manualMode = true; // نحتفظ بالوضع اليدوي لمنع إعادة التشغيل التلقائي
    digitalWrite(relayPin, HIGH);
    isPumping = false;
    currentStatus = "تم الإيقاف اليدوي 🛑";
    addLog("تم الإيقاف اليدوي للدينمو");
  }
  server.sendHeader("Location", "/", true);
  server.send(303);
}

// دالة تحديث وقت الطوارئ
void handleSetTimeout() {
  if (server.hasArg("minutes")) {
    int minutes = server.arg("minutes").toInt();
    
    // التحقق من أن القيمة بين 1 و 300 دقيقة
    if (minutes >= 1 && minutes <= 300) {
      maxPumpTime = minutes * 60000UL; // تحويل الدقائق إلى ميلي ثانية
      currentStatus = "تم تحديث وقت الطوارئ إلى " + String(minutes) + " دقيقة ⚙️";
      addLog("تم تحديث وقت الطوارئ: " + String(minutes) + " دقيقة");
      Serial.println("تم تحديث وقت الطوارئ إلى " + String(minutes) + " دقيقة");
    } else {
      currentStatus = "خطأ: القيمة يجب أن تكون بين 1 و 300 دقيقة ❌";
      addLog("محاولة تعيين وقت غير صالح: " + String(minutes) + " دقيقة");
    }
  }
  
  server.sendHeader("Location", "/", true);
  server.send(303);
}
// ---------------------------------------------------------
void setup() {
  Serial.begin(115200);

  // إعداد المنافذ
  pinMode(relayPin, OUTPUT);
  pinMode(powerPin, OUTPUT);
  pinMode(highSensorPin, INPUT_PULLUP);
  pinMode(lowSensorPin, INPUT_PULLUP);

  digitalWrite(powerPin, HIGH); 
  digitalWrite(relayPin, HIGH); // إيقاف الدينمو (حسب نوع المرحل لديك)

  // الاتصال بالواي فاي
  Serial.println();
  Serial.print("جاري الاتصال بـ ");
  Serial.println(ssid);
  WiFi.begin(ssid, password);

  while (WiFi.status() != WL_CONNECTED) {
    delay(500);
    Serial.print(".");
  }

  Serial.println("\nتم الاتصال بنجاح!");
  Serial.print("IP Address: ");
  Serial.println(WiFi.localIP()); // اطبع هذا الـ IP لتدخل عليه من هاتفك

  // توجيه المتصفح לדالة handleRoot عند طلب الصفحة الرئيسية
  server.on("/", handleRoot);
  server.on("/reset", handleReset);
  server.on("/manual-on", handleManualOn);
  server.on("/manual-off", handleManualOff);
  server.on("/set-timeout", handleSetTimeout);
  server.begin();
  Serial.println("سيرفر الويب يعمل الآن...");
  addLog("تم تشغيل النظام بنجاح");

}

// ---------------------------------------------------------
void loop() {
  // 1. الاستماع لطلبات المتصفح (هذه الدالة يجب أن تعمل دائماً بدون توقف)
  server.handleClient();

  // 2. فحص مستوى الماء كل ثانيتين فقط (بدون إيقاف السيرفر)
  if (millis() - lastSensorRead >= 2000) {
    lastSensorRead = millis();

    // إذا كان هناك خطأ، لا تقم بشيء سوى تحديث النص
    if (systemError) {
      currentStatus = "⚠️ طوارئ: الدينمو عمل لساعة دون امتلاء. النظام مغلق.";
      return; 
    }

    // --- حماية الأسلاك وقراءة الحساسات ---
    digitalWrite(powerPin, LOW);
    delay(10); // تأخير بسيط جداً لا يؤثر على السيرفر
    int highWater = digitalRead(highSensorPin);
    int lowWater = digitalRead(lowSensorPin);
    digitalWrite(powerPin, HIGH);

    // --- منطق التشغيل والإيقاف ---
    if (highWater == LOW) {
      if (isPumping) {
        digitalWrite(relayPin, HIGH); 
        isPumping = false;
        manualMode = false;
        currentStatus = "الخزان ممتلئ تماماً 🟢";
        addLog("تم إيقاف الدينمو - الخزان ممتلئ");
        Serial.println("تم إيقاف الدينمو");
      } else {
        currentStatus = "الخزان ممتلئ 🟢";
      }
    } 
    else if (lowWater == HIGH) {
      if (!isPumping && !manualMode) {
        digitalWrite(relayPin, LOW); 
        isPumping = true;
        pumpStartTime = millis(); 
        currentStatus = "الخزان شبه فارغ - جاري التعبئة 🔄";
        addLog("تم تشغيل الدينمو تلقائياً");
        Serial.println("تم تشغيل الدينمو");
      }
    } else {
      // الماء في المنتصف
      if (isPumping) {
        if (manualMode) currentStatus = "تشغيل يدوي - جاري التعبئة 🔄";
        else currentStatus = "جاري التعبئة (تجاوز النصف) 🔄";
      }
      else currentStatus = "الخزان في وضع الاستهلاك (ممتلئ جزئياً) 🔵";
    }
  }

  // --- 3. مؤقت الأمان (حارس النظام) ---
  if (isPumping && !systemError) {
    if (millis() - pumpStartTime >= maxPumpTime) {
      digitalWrite(relayPin, HIGH); // إطفاء
      isPumping = false;
      manualMode = false;
      systemError = true; 
      currentStatus = "⚠️ طوارئ: تجاوز الوقت الآمن. النظام مقفل.";
      addLog("طوارئ: إيقاف الدينمو لحمايته!");
      Serial.println("طوارئ: إيقاف الدينمو لحمايته!");
    }
  }
  
  // --- 4. فحص اتصال الواي فاي ---
  if (WiFi.status() != WL_CONNECTED) {
    static unsigned long lastReconnectAttempt = 0;
    if (millis() - lastReconnectAttempt >= 30000) { // محاولة إعادة الاتصال كل 30 ثانية
      lastReconnectAttempt = millis();
      Serial.println("محاولة إعادة الاتصال بالواي فاي...");
      WiFi.reconnect();
      addLog("محاولة إعادة الاتصال بالواي فاي");
    }
  }
}