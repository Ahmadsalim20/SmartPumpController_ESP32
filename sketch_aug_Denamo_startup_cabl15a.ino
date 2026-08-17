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
  html += "* { box-sizing: border-box; margin: 0; padding: 0; }";
  html += "body { font-family: -apple-system, BlinkMacSystemFont, 'Segoe UI', sans-serif; background: #eef2f7; padding: 16px; min-height: 100vh; color: #1e293b; display: flex; flex-direction: column; align-items: center; }";
  html += ".container { width: 100%; max-width: 400px; display: flex; flex-direction: column; gap: 12px; }";
  html += "h1 { font-size: 20px; font-weight: 800; color: #1e3a5f; text-align: center; margin: 8px 0 4px; }";
  
  // شريط الحالة العلوي
  html += ".mode-bar { display: flex; align-items: center; justify-content: space-between; background: white; padding: 12px 16px; border-radius: 14px; box-shadow: 0 1px 3px rgba(0,0,0,0.05); }";
  html += ".mode-title { font-size: 13px; color: #64748b; font-weight: 600; }";
  html += ".mode-badge { font-size: 12px; font-weight: 700; padding: 4px 12px; border-radius: 20px; text-transform: uppercase; display: flex; align-items: center; gap: 6px; }";
  html += ".mode-badge.auto { background: #dcfce7; color: #15803d; border: 1.5px solid #bbf7d0; }";
  html += ".mode-badge.manual { background: #dbeafe; color: #1d4ed8; border: 1.5px solid #bfdbfe; }";
  html += ".badge-dot { width: 6px; height: 6px; border-radius: 50%; }";
  html += ".auto .badge-dot { background: #16a34a; animation: pulse 2s infinite; }";
  html += ".manual .badge-dot { background: #2563eb; animation: pulse 1.5s infinite; }";
  html += "@keyframes pulse { 0%, 100% { opacity: 1; } 50% { opacity: 0.3; } }";

  // بطاقة الحالة البطل (Hero Status)
  html += ".status-card { background: white; border-radius: 18px; padding: 20px; text-align: center; box-shadow: 0 4px 6px -1px rgba(0,0,0,0.05), 0 2px 4px -1px rgba(0,0,0,0.03); border: 2px solid transparent; transition: all 0.3s ease; }";
  if (systemError) {
    html += ".status-card { background: #fef2f2; border-color: #fca5a5; }";
  } else if (isPumping) {
    html += ".status-card { background: #eff6ff; border-color: #93c5fd; }";
  } else {
    html += ".status-card { background: #f0fdf4; border-color: #86efac; }";
  }
  html += ".status-text { font-size: 18px; font-weight: 700; margin-bottom: 4px; }";
  if (systemError) html += ".status-text { color: #dc2626; }";
  else if (isPumping) html += ".status-text { color: #1d4ed8; }";
  else html += ".status-text { color: #15803d; }";

  // تفاصيل التشغيل
  html += ".pump-info { display: flex; justify-content: center; align-items: center; gap: 16px; margin-top: 14px; padding-top: 14px; border-top: 1px solid #e2e8f0; }";
  html += ".info-item { text-align: center; flex: 1; }";
  html += ".info-label { font-size: 10px; color: #94a3b8; font-weight: 700; margin-bottom: 2px; }";
  html += ".info-val { font-size: 14px; font-weight: 700; color: #334155; }";
  html += ".info-val.active { color: #16a34a; }";
  html += ".info-val.inactive { color: #64748b; }";
  html += ".info-val.timer { color: #2563eb; font-variant-numeric: tabular-nums; }";
  html += ".divider { width: 1px; height: 28px; background: #e2e8f0; }";

  // بطاقة التحكم (Controls Card)
  html += ".card { background: white; border-radius: 18px; padding: 18px; box-shadow: 0 4px 6px -1px rgba(0,0,0,0.05); display: flex; flex-direction: column; gap: 12px; }";
  html += ".card-label { font-size: 11px; font-weight: 800; color: #94a3b8; text-transform: uppercase; letter-spacing: 0.5px; margin-bottom: 2px; }";

  // الأزرار
  html += ".btn { padding: 13px; border: none; border-radius: 12px; font-size: 15px; font-weight: 700; cursor: pointer; transition: all 0.15s ease; text-decoration: none; display: flex; align-items: center; justify-content: center; gap: 6px; }";
  html += ".btn:active { transform: scale(0.97); opacity: 0.85; }";
  html += ".btn-success { background: #16a34a; color: white; }";
  html += ".btn-success:hover { background: #15803d; }";
  html += ".btn-danger { background: #dc2626; color: white; }";
  html += ".btn-danger:hover { background: #b91c1c; }";
  html += ".btn-secondary { background: #f1f5f9; color: #475569; border: 1.5px solid #e2e8f0; }";
  html += ".btn-secondary:hover { background: #e2e8f0; }";
  html += ".btn-warning { background: #d97706; color: white; }";
  html += ".btn-warning:hover { background: #b45309; }";
  html += ".btn-primary { background: #2563eb; color: white; }";
  html += ".btn-primary:hover { background: #1d4ed8; }";

  html += ".controls-grid { display: grid; grid-template-columns: 1fr 1fr; gap: 10px; }";

  // تايمر التحكم اليدوي
  html += ".timer-box { background: #eff6ff; border: 2.5px solid #93c5fd; border-radius: 14px; padding: 14px; margin-top: 4px; display: flex; flex-direction: column; gap: 10px; }";
  html += ".timer-box-title { font-size: 12px; font-weight: 800; color: #1d4ed8; display: flex; align-items: center; gap: 6px; }";
  html += ".timer-countdown { font-size: 32px; font-weight: 900; color: #1d4ed8; text-align: center; margin: 4px 0; font-variant-numeric: tabular-nums; letter-spacing: 2px; }";
  html += ".timer-input-group { display: flex; gap: 8px; align-items: center; }";
  html += ".timer-input { flex: 1; padding: 10px; border: 1.5px solid #bfdbfe; border-radius: 9px; font-size: 15px; text-align: center; background: white; }";
  html += ".timer-input:focus { border-color: #2563eb; outline: none; }";
  html += ".timer-desc { font-size: 10px; color: #64748b; text-align: center; }";

  // التبويب الذكي (Tabs) للتفاصيل والسجلات
  html += ".tabs-card { background: white; border-radius: 18px; padding: 14px; box-shadow: 0 4px 6px -1px rgba(0,0,0,0.05); }";
  html += ".tabs-header { display: flex; background: #f1f5f9; padding: 4px; border-radius: 12px; margin-bottom: 12px; }";
  html += ".tab-btn { flex: 1; padding: 8px; text-align: center; font-size: 13px; font-weight: 700; border-radius: 9px; cursor: pointer; color: #64748b; border: none; background: transparent; transition: all 0.2s ease; }";
  html += ".tab-btn.active { background: white; color: #1e3a5f; box-shadow: 0 2px 4px rgba(0,0,0,0.05); }";
  html += ".tab-content { display: none; }";
  html += ".tab-content.active { display: block; }";

  // سجل العمليات
  html += ".log-list { display: flex; flex-direction: column; gap: 4px; max-height: 140px; overflow-y: auto; }";
  html += ".log-item { font-size: 12px; color: #475569; padding: 7px 10px; background: #f8fafc; border-radius: 8px; text-align: right; border-right: 3.5px solid #cbd5e1; }";

  // الإعدادات
  html += ".settings-group { display: flex; flex-direction: column; gap: 8px; }";
  html += ".settings-row { display: flex; align-items: center; justify-content: space-between; gap: 12px; }";
  html += ".settings-label { font-size: 13px; color: #475569; font-weight: 600; }";
  html += ".settings-input { width: 75px; padding: 8px; border: 1.5px solid #e2e8f0; border-radius: 9px; font-size: 14px; text-align: center; }";
  html += ".settings-input:focus { border-color: #2563eb; outline: none; }";
  html += ".settings-desc { font-size: 10px; color: #94a3b8; text-align: center; margin-top: 4px; }";

  html += "</style></head><body>";
  html += "<div class='container'>";
  html += "<h1>مراقبة الخزان</h1>";

  // شريط الحالة والوضع العلوي
  html += "<div class='mode-bar'>";
  html += "<span class='mode-title'>وضع التشغيل</span>";
  if (manualMode) {
    html += "<div class='mode-badge manual'><div class='badge-dot'></div>يدوي</div>";
  } else {
    html += "<div class='mode-badge auto'><div class='badge-dot'></div>تلقائي</div>";
  }
  html += "</div>";

  // بطاقة الحالة البطل
  html += "<div class='status-card'>";
  html += "<div class='status-text' id='statusText'>" + currentStatus + "</div>";
  html += "<div class='pump-info' id='pumpRow'>";
  
  if (isPumping && !systemError) {
    long elapsed = (millis() - pumpStartTime) / 1000;
    String elS = (elapsed % 60 < 10 ? "0" : "") + String(elapsed % 60);
    html += "<div class='info-item'><div class='info-label'>الدينمو</div><div class='info-val active'>يعمل</div></div>";
    html += "<div class='divider'></div>";
    html += "<div class='info-item'><div class='info-label'>وقت التشغيل</div><div class='info-val'>" + String(elapsed / 60) + ":" + elS + "</div></div>";
    
    if (manualTimerActive) {
      long rem = ((long)manualTimerDuration - (long)(millis() - pumpStartTime)) / 1000;
      if (rem < 0) rem = 0;
      String remS = (rem % 60 < 10 ? "0" : "") + String(rem % 60);
      html += "<div class='divider'></div>";
      html += "<div class='info-item'><div class='info-label'>المتبقي</div><div class='info-val timer' id='statusTimer'>" + String(rem / 60) + ":" + remS + "</div></div>";
    }
  } else {
    html += "<div class='info-item'><div class='info-label'>الدينمو</div><div class='info-val inactive'>متوقف</div></div>";
  }
  
  html += "</div></div>";

  // بطاقة التحكم
  if (systemError) {
    html += "<div class='card'>";
    html += "<span class='card-label'>إجراء أمان مطلوب</span>";
    html += "<a href='/reset' class='btn btn-danger' style='padding:15px; font-size:16px;'>إعادة ضبط الطوارئ</a>";
    html += "</div>";
  } else {
    html += "<div class='card'>";
    html += "<span class='card-label'>التحكم في المحرك</span>";
    
    // أزرار تشغيل وإيقاف
    html += "<div class='controls-grid'>";
    html += "<a href='/manual-on' class='btn btn-success'>تشغيل</a>";
    html += "<a href='/manual-off' class='btn btn-danger'>إيقاف</a>";
    html += "</div>";
    
    // أزرار التحكم بالنظام
    html += "<div class='controls-grid'>";
    html += "<a href='/toggle-mode' class='btn btn-secondary'>";
    html += manualMode ? "الوضع التلقائي" : "الوضع اليدوي";
    html += "</a>";
    html += "<a href='/reset' class='btn btn-secondary'>تصفير</a>";
    html += "</div>";

    // تايمر الوضع اليدوي
    if (manualMode) {
      html += "<div class='timer-box'>";
      html += "<div class='timer-box-title'>⏱️ تايمر الإيقاف التلقائي</div>";
      if (manualTimerActive && isPumping) {
        long rem = ((long)manualTimerDuration - (long)(millis() - pumpStartTime)) / 1000;
        if (rem < 0) rem = 0;
        String remS = (rem % 60 < 10 ? "0" : "") + String(rem % 60);
        html += "<div class='timer-countdown' id='timerBig'>" + String(rem / 60) + ":" + remS + "</div>";
        html += "<a href='/cancel-timer' class='btn btn-warning'>إلغاء التايمر</a>";
      } else {
        html += "<form action='/set-manual-timer' method='GET' style='margin:0;'>";
        html += "<div class='timer-input-group'>";
        html += "<input type='number' name='min' class='timer-input' min='1' max='" + String(maxPumpTime / 60000UL) + "' placeholder='دقائق' required>";
        html += "<button type='submit' class='btn btn-primary' style='padding: 10px 16px;'>تفعيل</button>";
        html += "</div></form>";
        html += "<div class='timer-desc'>الحد الأقصى: " + String(maxPumpTime / 60000UL) + " دقيقة (وقت الطوارئ)</div>";
      }
      html += "</div>";
    }
    html += "</div>";
  }

  html += "<div class='tabs-card'>";
  html += "<div class='tabs-header'>";
  html += "<button class='tab-btn active' onclick=\"openTab(event, 'logTab')\">📋 سجل العمليات</button>";
  html += "<button class='tab-btn' onclick=\"openTab(event, 'settingsTab')\">⚙️ الإعدادات</button>";
  html += "</div>";

  // تبويب السجل
  html += "<div id='logTab' class='tab-content active'>";
  html += "<div class='log-list' id='logList'>";
  for (int i = 0; i < 10; i++) {
    int idx = (logIndex - 1 - i + 10) % 10;
    if (operationLog[idx] != "") {
      html += "<div class='log-item'>" + operationLog[idx] + "</div>";
    }
  }
  html += "</div></div>";

  // تبويب الإعدادات
  html += "<div id='settingsTab' class='tab-content'>";
  html += "<form action='/set-timeout' method='GET' class='settings-group'>";
  html += "<div class='settings-row'>";
  html += "<span class='settings-label'>حد وقت الطوارئ</span>";
  html += "<input type='number' name='minutes' class='settings-input' min='1' max='300' value='" + String(maxPumpTime / 60000UL) + "' required>";
  html += "</div>";
  html += "<button type='submit' class='btn btn-primary' style='padding: 10px;'>حفظ الإعدادات</button>";
  html += "</form>";
  html += "<div class='settings-desc'>النطاق المسموح به: من 1 إلى 300 دقيقة</div>";
  html += "</div>";

  html += "</div>"; // tabs-card

  html += "</div>"; // container

  // JavaScript للتحديث التلقائي والتبويب
  html += "<script>";
  html += "function openTab(evt, tabName) {";
  html += "  var i, tabcontent, tablinks;";
  html += "  tabcontent = document.getElementsByClassName('tab-content');";
  html += "  for (i = 0; i < tabcontent.length; i++) tabcontent[i].classList.remove('active');";
  html += "  tablinks = document.getElementsByClassName('tab-btn');";
  html += "  for (i = 0; i < tablinks.length; i++) tablinks[i].classList.remove('active');";
  html += "  document.getElementById(tabName).classList.add('active');";
  html += "  evt.currentTarget.classList.add('active');";
  html += "}";

  html += "function updateData() {";
  html += "  fetch('/status').then(r => r.json()).then(data => {";
  html += "    var st = document.getElementById('statusText'); if(st) st.textContent = data.status;";
  html += "    var pr = document.getElementById('pumpRow'); if(pr) pr.innerHTML = data.pumpRow;";
  html += "    var ll = document.getElementById('logList'); if(ll) ll.innerHTML = data.logs;";
  
  html += "    var tb = document.getElementById('timerBig');";
  html += "    if (tb && data.timerRemaining >= 0) {";
  html += "      var m = Math.floor(data.timerRemaining / 60);";
  html += "      var s = data.timerRemaining % 60;";
  html += "      tb.textContent = m + ':' + (s < 10 ? '0' : '') + s;";
  html += "    }";
  
  html += "    var tc = document.getElementById('statusTimer');";
  html += "    if (tc && data.timerRemaining >= 0) {";
  html += "      var m = Math.floor(data.timerRemaining / 60);";
  html += "      var s = data.timerRemaining % 60;";
  html += "      tc.textContent = m + ':' + (s < 10 ? '0' : '') + s;";
  html += "    }";
  html += "  });";
  html += "}";
  html += "setInterval(updateData, 1000);";
  html += "</script>";
  html += "</body></html>";

  server.send(200, "text/html", html);
}

void handleStatus() {
  String json = "{";
  json += "\"status\":\"" + currentStatus + "\",";
  
  String pumpRow = "";
  if (isPumping && !systemError) {
    long elapsed = (millis() - pumpStartTime) / 1000;
    String elS = (elapsed % 60 < 10 ? "0" : "") + String(elapsed % 60);
    pumpRow += "<div class='info-item'><div class='info-label'>الدينمو</div><div class='info-val active'>يعمل</div></div>";
    pumpRow += "<div class='divider'></div>";
    pumpRow += "<div class='info-item'><div class='info-label'>وقت التشغيل</div><div class='info-val'>" + String(elapsed / 60) + ":" + elS + "</div></div>";
    
    if (manualTimerActive) {
      long remaining = ((long)manualTimerDuration - (long)(millis() - pumpStartTime)) / 1000;
      if (remaining < 0) remaining = 0;
      String remS = (remaining % 60 < 10 ? "0" : "") + String(remaining % 60);
      pumpRow += "<div class='divider'></div>";
      pumpRow += "<div class='info-item'><div class='info-label'>المتبقي</div><div class='info-val timer' id='statusTimer'>" + String(remaining / 60) + ":" + remS + "</div></div>";
    }
  } else {
    pumpRow += "<div class='info-item'><div class='info-label'>الدينمو</div><div class='info-val inactive'>متوقف</div></div>";
  }
  json += "\"pumpRow\":\"" + pumpRow + "\",";

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
