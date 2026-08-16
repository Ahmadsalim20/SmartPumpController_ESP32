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
const int warningSensorPin = 12; // D6: سلك التحذير (قبل النفاد)
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

// --- تايمر الوضع اليدوي ---
bool   manualTimerActive  = false;   // هل التايمر اليدوي مُفعَّل؟
unsigned long manualTimerDuration = 0; // مدة التايمر اليدوي بالميلي ثانية

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
  html += "<title>مراقبة الخزان</title>";
  html += "<style>";
  html += "* { box-sizing: border-box; }";
  html += "body { font-family: -apple-system, BlinkMacSystemFont, 'Segoe UI', sans-serif; text-align: center; background: #f0f4f8; padding: 20px; min-height: 100vh; margin: 0; color: #333; }";
  html += ".container { max-width: 420px; margin: 0 auto; }";
  html += ".card { background: white; padding: 24px; border-radius: 16px; box-shadow: 0 2px 10px rgba(0,0,0,0.09); margin-bottom: 14px; }";
  html += "h1 { color: #1e3a5f; margin: 0 0 16px 0; font-size: 22px; font-weight: 700; }";
  html += ".mode-bar { display: flex; align-items: center; justify-content: center; gap: 10px; background: #f8fafc; border: 2px solid #e2e8f0; border-radius: 12px; padding: 12px 18px; margin-bottom: 16px; }";
  html += ".mode-label { font-size: 13px; color: #94a3b8; font-weight: 500; }";
  html += ".mode-badge { font-size: 15px; font-weight: 700; padding: 5px 16px; border-radius: 20px; letter-spacing: 0.3px; }";
  html += ".mode-auto { background: #dcfce7; color: #15803d; border: 1.5px solid #86efac; }";
  html += ".mode-manual { background: #dbeafe; color: #1d4ed8; border: 1.5px solid #93c5fd; }";
  html += ".mode-icon { font-size: 20px; }";
  html += ".status { font-size: 17px; font-weight: 600; margin: 14px 0; padding: 18px; border-radius: 12px; line-height: 1.5; }";
  html += ".pump-info { font-size: 15px; color: #64748b; margin: 12px 0; padding: 10px; background: #f8fafc; border-radius: 8px; }";
  html += ".controls { margin: 16px 0; display: grid; grid-template-columns: 1fr 1fr; gap: 10px; }";
  html += ".btn { padding: 13px; border: none; border-radius: 10px; font-size: 15px; font-weight: 600; cursor: pointer; transition: all 0.2s; text-decoration: none; display: block; text-align: center; }";
  html += ".btn:hover { opacity: 0.88; transform: translateY(-1px); }";
  html += ".btn:active { transform: translateY(0); }";
  html += ".btn-primary { background: #2563eb; color: white; }";
  html += ".btn-danger { background: #dc2626; color: white; }";
  html += ".btn-success { background: #16a34a; color: white; }";
  html += ".btn-warning { background: #d97706; color: white; }";
  html += ".log-section { background: white; padding: 18px; border-radius: 16px; margin-bottom: 14px; text-align: right; max-height: 150px; overflow-y: auto; }";
  html += ".log-title { font-weight: 700; color: #1e293b; margin-bottom: 10px; font-size: 14px; }";
  html += ".log-item { font-size: 13px; color: #64748b; padding: 5px 0; border-bottom: 1px solid #f1f5f9; }";
  html += ".settings-section { background: white; padding: 18px; border-radius: 16px; margin-bottom: 14px; }";
  html += ".settings-title { font-weight: 700; color: #1e293b; margin-bottom: 12px; font-size: 14px; }";
  html += ".input-group { display: flex; gap: 10px; justify-content: center; align-items: center; }";
  html += ".input-field { padding: 10px; border: 1.5px solid #e2e8f0; border-radius: 8px; font-size: 15px; width: 80px; text-align: center; }";
  html += ".input-field:focus { border-color: #2563eb; outline: none; }";
  html += ".timer-card { background: #eff6ff; border: 2px solid #93c5fd; border-radius: 14px; padding: 16px; margin-top: 14px; }";
  html += ".timer-title { font-weight: 700; color: #1d4ed8; font-size: 14px; margin-bottom: 10px; }";
  html += ".timer-countdown { font-size: 22px; font-weight: 800; color: #1d4ed8; margin: 8px 0; letter-spacing: 1px; }";
  html += ".timer-note { font-size: 11px; color: #64748b; margin-top: 6px; }";
  
  if (systemError) {
    html += ".status { background: #fee2e2; color: #dc2626; border: 1.5px solid #fca5a5; }";
  } else if (isPumping) {
    html += ".status { background: #eff6ff; color: #1d4ed8; border: 1.5px solid #93c5fd; }";
  } else {
    html += ".status { background: #f0fdf4; color: #15803d; border: 1.5px solid #86efac; }";
  }
  
  html += "</style></head><body>";
  
  html += "<div class='container'>";
  html += "<div class='card'>";
  html += "<h1>مراقبة الخزان</h1>";
  
  // شريط الوضع (تلقائي / يدوي) - واضح ومستقل
  html += "<div class='mode-bar'>";
  html += "<span class='mode-label'>وضع التشغيل:</span>";
  if (manualMode) {
    html += "<span class='mode-icon'>🖐</span>";
    html += "<span class='mode-badge mode-manual'>يدوي</span>";
  } else {
    html += "<span class='mode-icon'>⚙️</span>";
    html += "<span class='mode-badge mode-auto'>تلقائي</span>";
  }
  html += "</div>";

  html += "<div class='status'>" + currentStatus + "</div>";
  
  html += "<div class='pump-info' id='pumpInfo'>";
  if (isPumping && !systemError) {
    long elapsed = (millis() - pumpStartTime) / 1000;
    html += "الدينمو: <b>يعمل ⚡</b><br>";
    html += "وقت التشغيل: " + String(elapsed / 60) + " دقيقة و " + String(elapsed % 60) + " ثانية";
    if (manualTimerActive) {
      long remaining = ((long)manualTimerDuration - (long)(millis() - pumpStartTime)) / 1000;
      if (remaining < 0) remaining = 0;
      html += "<br><span style='color:#1d4ed8;font-weight:700;'>⏱ متبقي: " + String(remaining / 60) + ":" + (remaining % 60 < 10 ? "0" : "") + String(remaining % 60) + "</span>";
    }
  } else {
    html += "الدينمو: <b>متوقف 🛑</b>";
  }
  html += "</div>";

  // --- تايمر الوضع اليدوي ---
  if (manualMode && !systemError) {
    html += "<div class='timer-card'>";
    html += "<div class='timer-title'>⏱ تايمر الإيقاف التلقائي</div>";
    if (manualTimerActive && isPumping) {
      long remaining = ((long)manualTimerDuration - (long)(millis() - pumpStartTime)) / 1000;
      if (remaining < 0) remaining = 0;
      html += "<div class='timer-countdown' id='timerCountdown'>" + String(remaining / 60) + ":" + (remaining % 60 < 10 ? "0" : "") + String(remaining % 60) + "</div>";
      html += "<div style='font-size:12px;color:#64748b;margin-bottom:8px;'>يتوقف الدينمو تلقائياً عند الوصول للصفر</div>";
      html += "<a href='/cancel-timer' class='btn btn-warning' style='padding:8px;font-size:13px;'>إلغاء التايمر</a>";
    } else {
      html += "<form action='/set-manual-timer' method='GET' style='margin:0;'>";
      html += "<div class='input-group'>";
      html += "<input type='number' name='min' class='input-field' min='1' max='" + String(maxPumpTime / 60000UL) + "' placeholder='دقائق' required>";
      html += "<span style='color:#64748b;font-size:13px;'>دقيقة</span>";
      html += "<button type='submit' class='btn btn-primary' style='padding:10px 16px;font-size:13px;'>تفعيل</button>";
      html += "</div></form>";
      html += "<div class='timer-note'>الحد الأقصى: " + String(maxPumpTime / 60000UL) + " دقيقة (وقت الطوارئ)</div>";
    }
    html += "</div>";
  }
  
  // أزرار التحكم
  html += "<div class='controls'>";
  if (systemError) {
    html += "<a href='/reset' class='btn btn-danger'>إعادة ضبط الطوارئ</a>";
  } else {
    html += "<a href='/toggle-mode' class='btn btn-primary'>";
    html += manualMode ? "الوضع التلقائي" : "الوضع اليدوي";
    html += "</a>";
    html += "<a href='/manual-on' class='btn btn-success'>تشغيل</a>";
    html += "<a href='/manual-off' class='btn btn-warning'>إيقاف</a>";
    html += "<a href='/reset' class='btn btn-danger'>تصفير</a>";
  }
  html += "</div>";
  html += "</div>";
  
  // سجل العمليات
  html += "<div class='log-section'>";
  html += "<div class='log-title'>سجل العمليات</div>";
  for (int i = 0; i < 10; i++) {
    int idx = (logIndex - 1 - i + 10) % 10;
    if (operationLog[idx] != "") {
      html += "<div class='log-item'>" + operationLog[idx] + "</div>";
    }
  }
  html += "</div>";
  
  // قسم الإعدادات (وقت الطوارئ)
  html += "<div class='settings-section'>";
  html += "<div class='settings-title'>وقت الطوارئ</div>";
  html += "<div class='input-group'>";
  html += "<form action='/set-timeout' method='GET'>";
  html += "<input type='number' name='minutes' class='input-field' min='1' max='300' value='" + String(maxPumpTime / 60000UL) + "' required>";
  html += "<span style='color: #64748b;'>دقائق</span>";
  html += "<button type='submit' class='btn btn-primary' style='padding: 10px 20px;'>حفظ</button>";
  html += "</form>";
  html += "</div>";
  html += "<div style='font-size: 12px; color: #94a3b8; margin-top: 10px;'>الحد الأقصى: 300 دقيقة</div>";
  html += "</div>";
  
  html += "</div></body></html>";
  
  // إضافة JavaScript للتحديث التلقائي
  html += "<script>";
  html += "function updateData() {";
  html += "  fetch('/status').then(r => r.json()).then(data => {";
  html += "    document.querySelector('.status').textContent = data.status;";
  html += "    document.getElementById('pumpInfo').innerHTML = data.pumpInfo;";
  html += "    document.querySelector('.log-section').innerHTML = '<div class=\"log-title\">📋 سجل العمليات:</div>' + data.logs;";
  html += "    var cd = document.getElementById('timerCountdown');";
  html += "    if (cd && data.timerRemaining >= 0) {";
  html += "      var m = Math.floor(data.timerRemaining / 60);";
  html += "      var s = data.timerRemaining % 60;";
  html += "      cd.textContent = m + ':' + (s < 10 ? '0' : '') + s;";
  html += "    }";
  html += "  });";
  html += "}";
  html += "setInterval(updateData, 1000);";
  html += "</script>";

  server.send(200, "text/html", html);
}

// دالة API لإرجاع البيانات بصيغة JSON
void handleStatus() {
  String json = "{";
  json += "\"status\":\"" + currentStatus + "\",";
  
  String pumpInfo;
  if (isPumping && !systemError) {
    long elapsed = (millis() - pumpStartTime) / 1000;
    pumpInfo = "الدينمو: <b>يعمل ⚡</b><br>وقت التشغيل: " + String(elapsed / 60) + " دقيقة و " + String(elapsed % 60) + " ثانية";
    if (manualTimerActive) {
      long remaining = ((long)manualTimerDuration - (long)(millis() - pumpStartTime)) / 1000;
      if (remaining < 0) remaining = 0;
      String rem_s = (remaining % 60 < 10 ? "0" : "") + String(remaining % 60);
      pumpInfo += "<br><span style='color:#1d4ed8;font-weight:700;'>⏱ متبقي: " + String(remaining / 60) + ":" + rem_s + "</span>";
    }
  } else {
    pumpInfo = "الدينمو: <b>متوقف 🛑</b>";
  }
  json += "\"pumpInfo\":\"" + pumpInfo + "\",";

  // وقت التايمر المتبقي (ثانية)
  long timerRemaining = -1;
  if (manualTimerActive && isPumping) {
    timerRemaining = ((long)manualTimerDuration - (long)(millis() - pumpStartTime)) / 1000;
    if (timerRemaining < 0) timerRemaining = 0;
  }
  json += "\"timerRemaining\":" + String(timerRemaining) + ",";
  
  String logs = "";
  for (int i = 0; i < 10; i++) {
    int idx = (logIndex - 1 - i + 10) % 10;
    if (operationLog[idx] != "") {
      logs += "<div class='log-item'>" + operationLog[idx] + "</div>";
    }
  }
  json += "\"logs\":\"" + logs + "\"";
  json += "}";
  
  server.send(200, "application/json", json);
}
// دالة إعادة ضبط النظام
void handleReset() {
  systemError = false;   // إلغاء حالة الطوارئ
  isPumping = false;     // إيقاف حالة الضخ
  manualMode = false;    // إلغاء الوضع اليدوي
  digitalWrite(relayPin, HIGH); // التأكد من إطفاء الدينمو للأمان
  currentStatus = "تم إعادة ضبط النظام";
  addLog("تم إعادة ضبط النظام");
  
  server.sendHeader("Location", "/", true);
  server.send(303);
}

// دالة تبديل الوضع (يدوي/تلقائي)
void handleToggleMode() {
  if (!systemError) {
    manualMode = !manualMode;
    currentStatus = manualMode ? "تم التبديل للوضع اليدوي" : "تم التبديل للوضع التلقائي";
    addLog(manualMode ? "تبديل للوضع اليدوي" : "تبديل للوضع التلقائي");
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
    manualTimerActive = false;  // يبدأ بدون تايمر - يُفعَّل لاحقاً إن أراد المستخدم
    manualTimerDuration = 0;
    currentStatus = "🖐 تشغيل يدوي - جاري التعبئة";
    addLog("تم التشغيل اليدوي للدينمو");
  }
  server.sendHeader("Location", "/", true);
  server.send(303);
}

// دالة تفعيل تايمر الإيقاف اليدوي
void handleSetManualTimer() {
  if (!systemError && server.hasArg("min")) {
    int requestedMin = server.arg("min").toInt();
    int maxMin = (int)(maxPumpTime / 60000UL); // الحد الأقصى = وقت الطوارئ
    if (requestedMin < 1) requestedMin = 1;
    if (requestedMin > maxMin) requestedMin = maxMin; // تقييد بالحد الأقصى
    manualTimerDuration = (unsigned long)requestedMin * 60000UL;
    manualTimerActive = true;
    // إذا كان الدينمو لا يعمل بعد، شغّله الآن
    if (!isPumping) {
      if (!manualMode) { manualMode = true; addLog("تبديل للوضع اليدوي"); }
      digitalWrite(relayPin, LOW);
      isPumping = true;
      pumpStartTime = millis();
    }
    currentStatus = "⏱ تايمر يدوي: " + String(requestedMin) + " دقيقة";
    addLog("تايمر يدوي: " + String(requestedMin) + " دقيقة");
    Serial.println("تايمر يدوي: " + String(requestedMin) + " دقيقة");
  }
  server.sendHeader("Location", "/", true);
  server.send(303);
}

// دالة إلغاء تايمر الوضع اليدوي
void handleCancelTimer() {
  manualTimerActive = false;
  manualTimerDuration = 0;
  currentStatus = "🖐 يدوي - التايمر ملغى";
  addLog("تم إلغاء التايمر اليدوي");
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
    currentStatus = "تم الإيقاف اليدوي";
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
      currentStatus = "تم تحديث وقت الطوارئ إلى " + String(minutes) + " دقيقة";
      addLog("تم تحديث وقت الطوارئ: " + String(minutes) + " دقيقة");
      Serial.println("تم تحديث وقت الطوارئ إلى " + String(minutes) + " دقيقة");
    } else {
      currentStatus = "خطأ: القيمة يجب أن تكون بين 1 و 300 دقيقة";
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
  pinMode(warningSensorPin, INPUT_PULLUP);

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
  server.on("/status", handleStatus);
  server.on("/reset", handleReset);
  server.on("/toggle-mode", handleToggleMode);
  server.on("/manual-on", handleManualOn);
  server.on("/manual-off", handleManualOff);
  server.on("/set-timeout", handleSetTimeout);
  server.on("/set-manual-timer", handleSetManualTimer);
  server.on("/cancel-timer", handleCancelTimer);
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
      currentStatus = "طوارئ: تجاوز الوقت الآمن - النظام مقفل";
      return;
    }

    // --- حماية الأسلاك وقراءة الحساسات ---
    digitalWrite(powerPin, LOW);
    delay(10); // تأخير بسيط جداً لا يؤثر على السيرفر
    int highWater = digitalRead(highSensorPin);
    int lowWater = digitalRead(lowSensorPin);
    int warningWater = digitalRead(warningSensorPin);
    digitalWrite(powerPin, HIGH);

    // --- منطق التشغيل والإيقاف ---
    if (highWater == LOW) {
      // وصل الماء للسلك العلوي → الخزان ممتلئ
      if (isPumping && !manualMode) {
        digitalWrite(relayPin, HIGH);
        isPumping = false;
        currentStatus = "الخزان ممتلئ - تم إيقاف الدينمو";
        addLog("تم إيقاف الدينمو - الخزان ممتلئ");
        Serial.println("تم إيقاف الدينمو - الخزان ممتلئ");
      } else if (isPumping && manualMode) {
        currentStatus = "⚠️ الخزان ممتلئ - يُنصح بالإيقاف";
      } else {
        currentStatus = "✅ الخزان ممتلئ";
      }
    }
    else if (warningWater == HIGH) {
      // الماء نزل تحت سلك التحذير → مستوى منخفض
      if (isPumping) {
        if (manualMode) currentStatus = "🖐 يدوي - جاري التعبئة | المستوى منخفض";
        else currentStatus = "⚡ جاري التعبئة | المستوى منخفض";
      } else {
        currentStatus = "⚠️ تحذير: مستوى الماء منخفض";
      }
    }
    else if (lowWater == HIGH) {
      // الماء نزل تحت السلك السفلي → تشغيل تلقائي
      if (!isPumping && !manualMode) {
        digitalWrite(relayPin, LOW);
        isPumping = true;
        pumpStartTime = millis();
        currentStatus = "🔴 مستوى حرج - تم تشغيل الدينمو تلقائياً";
        addLog("تم تشغيل الدينمو - مستوى حرج");
        Serial.println("تم تشغيل الدينمو - مستوى حرج");
      } else if (isPumping) {
        if (manualMode) currentStatus = "🖐 يدوي - جاري التعبئة | المستوى حرج";
        else currentStatus = "⚡ جاري التعبئة | المستوى حرج";
      } else if (manualMode) {
        currentStatus = "🔴 المستوى حرج - قم بتشغيل الدينمو";
      }
    } else {
      // الماء بين السلك السفلي وسلك التحذير → مستوى طبيعي
      if (isPumping) {
        if (manualMode) currentStatus = "🖐 يدوي - جاري التعبئة | المستوى متوسط";
        else currentStatus = "⚡ جاري التعبئة | المستوى متوسط";
      } else {
        currentStatus = "✅ المستوى طبيعي";
      }
    }
  }

  // --- 3. تايمر الوضع اليدوي (يتوقف عند انتهاء الوقت المحدد) ---
  if (isPumping && manualTimerActive && !systemError) {
    if (millis() - pumpStartTime >= manualTimerDuration) {
      digitalWrite(relayPin, HIGH);
      isPumping = false;
      manualTimerActive = false;
      manualTimerDuration = 0;
      currentStatus = "✅ انتهى تايمر الوضع اليدوي - تم إيقاف الدينمو";
      addLog("انتهى التايمر اليدوي - تم الإيقاف التلقائي");
      Serial.println("انتهى التايمر اليدوي - تم الإيقاف التلقائي");
    }
  }

  // -غ4. مؤقت الأمان (حارس النظام - يعمل دائماً بغض النظر عن الوضع) ---
  if (isPumping && !systemError) {
    if (millis() - pumpStartTime >= maxPumpTime) {
      digitalWrite(relayPin, HIGH);
      isPumping = false;
      manualTimerActive = false;
      manualMode = false;
      systemError = true;
      currentStatus = "طوارئ: تجاوز الوقت الآمن - النظام مقفل";
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
