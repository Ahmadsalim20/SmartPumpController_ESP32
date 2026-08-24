#ifndef WEB_PAGE_H
#define WEB_PAGE_H

#include <Arduino.h>
#include <ESP8266WebServer.h>
#include <ESP8266WiFi.h>
#include <time.h>
#include "Config.h"
#include "SystemState.h"

extern ESP8266WebServer server;

// ---------------------------------------------------------
// دالة بناء صفحة الـ HTML (واجهة المستخدم)
// ---------------------------------------------------------
void handleRoot() {
  // وقت النظام
  String sysTime = "--:--:--";
  time_t nowT = time(nullptr);
  if (nowT > 1000000000ULL) {
    struct tm* ti = localtime(&nowT);
    char buf[20];
    sprintf(buf, "%02d:%02d:%02d", ti->tm_hour, ti->tm_min, ti->tm_sec);
    sysTime = String(buf);
  }

  String html = "<!DOCTYPE html><html lang='ar' dir='rtl'>";
  html += "<head><meta charset='UTF-8'><meta name='viewport' content='width=device-width,initial-scale=1,user-scalable=no'>";
  html += "<title>SmartPump Controller</title><style>";

  // نظام الألوان
  html += ":root{--blue:#1a56db;--blue-dk:#1343b5;--blue-lt:#e8effd;";
  html += "--green:#2f9e44;--green-lt:#ebfbee;";
  html += "--red:#e03131;--red-lt:#fff5f5;";
  html += "--amber:#e67700;--amber-lt:#fff9db;";
  html += "--g50:#f8f9fb;--g100:#f0f2f5;--g200:#e2e6ea;";
  html += "--g400:#9aa3ae;--g600:#4b5563;--g800:#1f2937;";
  html += "--sh:0 1px 4px rgba(0,0,0,.08);--r:14px;--rs:9px;}";

  html += "*{box-sizing:border-box;margin:0;padding:0;}";
  html += "body{font-family:-apple-system,BlinkMacSystemFont,'Segoe UI',Tahoma,sans-serif;";
  html += "background:var(--g100);min-height:100vh;display:flex;flex-direction:column;align-items:center;padding:12px 12px 28px;}";
  html += ".page{width:100%;max-width:390px;display:flex;flex-direction:column;gap:10px;}";

  // رأس الصفحة
  html += ".hdr{display:flex;align-items:center;justify-content:space-between;padding:8px 2px 2px;}";
  html += ".hdr-title{font-size:17px;font-weight:800;color:var(--g800);}";
  html += ".hdr-time{font-size:13px;font-weight:600;color:var(--g400);font-variant-numeric:tabular-nums;}";

  // شريط الوضع والسوتش
  html += ".mode-bar{display:flex;align-items:center;justify-content:space-between;background:white;border-radius:var(--r);padding:10px 14px;box-shadow:var(--sh);}";
  html += ".mode-info{display:flex;align-items:center;gap:8px;}";
  html += ".mdot{width:8px;height:8px;border-radius:50%;flex-shrink:0;}";
  html += ".mdot.auto{background:var(--green);animation:blink 2.5s infinite;}";
  html += ".mdot.manual{background:var(--blue);animation:blink 1.8s infinite;}";
  html += ".mdot.err{background:var(--red);}";
  html += "@keyframes blink{0%,100%{opacity:1}50%{opacity:.25}}";
  html += ".mlabel{font-size:13px;font-weight:700;color:var(--g800);}";
  html += ".switch-link{display:flex;align-items:center;gap:8px;text-decoration:none;}";
  html += ".switch-lbl{font-size:12px;font-weight:700;}";
  html += ".switch-lbl.auto{color:var(--green);}.switch-lbl.manual{color:var(--blue);}.switch-lbl.err{color:var(--red);}";
  html += ".switch-lbl.on{color:var(--green);}.switch-lbl.off{color:var(--g400);}";
  html += ".switch-box{width:44px;height:24px;border-radius:12px;position:relative;transition:.25s ease;display:flex;align-items:center;padding:2px;}";
  html += ".switch-box.auto{background:#c3fae8;}";
  html += ".switch-box.manual{background:#d0ebff;}";
  html += ".switch-box.err{background:#ffe3e3;cursor:not-allowed;}";
  html += ".switch-box.on{background:#c3fae8;}";
  html += ".switch-box.off{background:var(--g200);}";
  html += ".switch-thumb{width:20px;height:20px;border-radius:50%;transition:.25s ease;box-shadow:0 1px 3px rgba(0,0,0,.2);}";
  html += ".switch-box.auto .switch-thumb{background:var(--green);transform:translateX(0);}";
  html += ".switch-box.manual .switch-thumb{background:var(--blue);transform:translateX(-20px);}";
  html += ".switch-box.err .switch-thumb{background:var(--red);transform:translateX(0);}";
  html += ".switch-box.on .switch-thumb{background:var(--green);transform:translateX(-20px);}";
  html += ".switch-box.off .switch-thumb{background:white;transform:translateX(0);}";
  html += ".pump-ctrl-box{display:flex;align-items:center;justify-content:space-between;background:var(--g50);border:1.5px solid var(--g200);border-radius:var(--rs);padding:10px 12px;}";

  // بطاقة الحالة
  html += ".s-card{background:white;border-radius:var(--r);padding:16px;box-shadow:var(--sh);border-right:4px solid var(--g200);}";
  if (systemError)    html += ".s-card{border-right-color:var(--red);background:var(--red-lt);}";
  else if (isPumping) html += ".s-card{border-right-color:var(--blue);background:var(--blue-lt);}";
  else                html += ".s-card{border-right-color:var(--green);background:var(--green-lt);}";
  html += ".s-text{font-size:15px;font-weight:700;margin-bottom:10px;}";
  if (systemError)    html += ".s-text{color:var(--red);}";
  else if (isPumping) html += ".s-text{color:var(--blue);}";
  else                html += ".s-text{color:var(--green);}";
  html += ".s-meta{display:flex;border-top:1px solid var(--g200);padding-top:10px;}";
  html += ".s-item{flex:1;text-align:center;}";
  html += ".s-item+.s-item{border-right:1px solid var(--g200);}";
  html += ".s-lbl{font-size:10px;color:var(--g400);font-weight:600;margin-bottom:3px;letter-spacing:.3px;}";
  html += ".s-val{font-size:14px;font-weight:700;color:var(--g800);}";
  html += ".s-val.on{color:var(--green);}.s-val.off{color:var(--g400);}.s-val.tk{color:var(--blue);font-variant-numeric:tabular-nums;}";

  // بطاقة التحكم
  html += ".c-card{background:white;border-radius:var(--r);padding:14px;box-shadow:var(--sh);display:flex;flex-direction:column;gap:10px;}";
  html += ".sec-lbl{font-size:10px;font-weight:800;color:var(--g400);text-transform:uppercase;letter-spacing:.6px;}";
  html += ".btn-row{display:grid;grid-template-columns:1fr 1fr;gap:8px;}";
  html += ".btn{display:flex;align-items:center;justify-content:center;padding:11px 8px;border:none;border-radius:var(--rs);";
  html += "font-size:14px;font-weight:700;cursor:pointer;text-decoration:none;transition:.15s;}";
  html += ".btn:active{transform:scale(.97);opacity:.85;}";
  html += ".btn.blue{background:var(--blue);color:white;}.btn.blue:hover{background:var(--blue-dk);}";
  html += ".btn.red{background:var(--red);color:white;}.btn.red:hover{background:#c92a2a;}";
  html += ".btn.ghost{background:var(--g50);color:var(--g600);border:1.5px solid var(--g200);}.btn.ghost:hover{background:var(--g200);}";
  html += ".btn.amber{background:var(--amber);color:white;}.btn.amber:hover{background:#d46b08;}";
  html += ".btn.full{grid-column:1/-1;width:100%;}";

  // تايمر
  html += ".tmr{background:var(--blue-lt);border:1.5px solid #c1d4f7;border-radius:var(--rs);padding:12px;}";
  html += ".tmr-title{font-size:11px;font-weight:800;color:var(--blue);margin-bottom:8px;}";
  html += ".tmr-big{font-size:30px;font-weight:900;color:var(--blue);text-align:center;letter-spacing:3px;font-variant-numeric:tabular-nums;margin:4px 0 8px;}";
  html += ".tmr-row{display:flex;gap:8px;align-items:center;}";
  html += ".tmr-inp{flex:1;padding:9px;border:1.5px solid #c1d4f7;border-radius:var(--rs);font-size:14px;text-align:center;background:white;outline:none;}";
  html += ".tmr-inp:focus{border-color:var(--blue);}";
  html += ".tmr-hint{font-size:10px;color:var(--g400);text-align:center;margin-top:5px;}";

  // تبويبات
  html += ".tabs{background:white;border-radius:var(--r);padding:12px;box-shadow:var(--sh);}";
  html += ".tabs-nav{display:flex;background:var(--g100);padding:3px;border-radius:var(--rs);margin-bottom:12px;}";
  html += ".tab-btn{flex:1;padding:7px;text-align:center;font-size:13px;font-weight:700;border-radius:7px;cursor:pointer;color:var(--g400);border:none;background:transparent;transition:.2s;}";
  html += ".tab-btn.active{background:white;color:var(--g800);box-shadow:0 1px 4px rgba(0,0,0,.06);}";
  html += ".tab-pane{display:none;}.tab-pane.active{display:block;}";

  // سجل
  html += ".log-list{display:flex;flex-direction:column;gap:4px;max-height:150px;overflow-y:auto;}";
  html += ".log-item{font-size:12px;color:var(--g600);padding:6px 10px;background:var(--g50);border-radius:6px;border-right:3px solid var(--g200);}";

  // إعدادات
  html += ".set-section{margin-bottom:14px;}";
  html += ".set-title{font-size:10px;font-weight:800;color:var(--blue);text-transform:uppercase;letter-spacing:.5px;";
  html += "padding-bottom:5px;border-bottom:1.5px solid var(--blue-lt);margin-bottom:9px;}";
  html += ".set-row{display:flex;align-items:center;justify-content:space-between;padding:5px 0;}";
  html += ".set-lbl{font-size:13px;color:var(--g600);font-weight:600;}";
  html += ".set-inp{width:80px;padding:7px;border:1.5px solid var(--g200);border-radius:var(--rs);font-size:13px;text-align:center;outline:none;}";
  html += ".set-inp:focus{border-color:var(--blue);}";
  html += ".set-hint{font-size:10px;color:var(--g400);text-align:center;margin-top:6px;}";

  html += "</style></head><body><div class='page'>";

  // رأس الصفحة
  html += "<div class='hdr'><div class='hdr-title'>متحكم مضخة المياه الذكي</div>";
  html += "<div class='hdr-time' id='sysTimeLabel'>" + sysTime + "</div></div>";

  // شريط الوضع مع السوتش
  html += "<div class='mode-bar'>";
  if (systemError) {
    html += "<div class='mode-info'><div class='mdot err'></div><div class='mlabel'>حالة النظام</div></div>";
    html += "<div class='switch-link'><span class='switch-lbl err'>طوارئ (مقفل)</span><div class='switch-box err'><div class='switch-thumb'></div></div></div>";
  } else if (manualMode) {
    html += "<div class='mode-info'><div class='mdot manual'></div><div class='mlabel'>وضع التشغيل</div></div>";
    html += "<a href='/toggle-mode' class='switch-link' title='انقر للتبديل للتلقائي'>";
    html += "<span class='switch-lbl manual'>يدوي</span>";
    html += "<div class='switch-box manual'><div class='switch-thumb'></div></div></a>";
  } else {
    html += "<div class='mode-info'><div class='mdot auto'></div><div class='mlabel'>وضع التشغيل</div></div>";
    html += "<a href='/toggle-mode' class='switch-link' title='انقر للتبديل لليدوي'>";
    html += "<span class='switch-lbl auto'>تلقائي</span>";
    html += "<div class='switch-box auto'><div class='switch-thumb'></div></div></a>";
  }
  html += "</div>";

  // بطاقة الحالة
  html += "<div class='s-card'><div class='s-text' id='statusText'>" + currentStatus + "</div>";
  html += "<div class='s-meta' id='pumpRow'>";
  if (isPumping && !systemError) {
    long el = (millis() - pumpStartTime) / 1000;
    String elS = (el % 60 < 10 ? "0" : "") + String(el % 60);
    html += "<div class='s-item'><div class='s-lbl'>الدينمو</div><div class='s-val on'>يعمل</div></div>";
    html += "<div class='s-item'><div class='s-lbl'>مدة التشغيل</div><div class='s-val tk'>" + String(el/60) + ":" + elS + "</div></div>";
    if (manualTimerActive) {
      long rem = ((long)manualTimerDuration-(long)(millis()-pumpStartTime))/1000;
      if(rem<0)rem=0;
      String remS=(rem%60<10?"0":"")+String(rem%60);
      html += "<div class='s-item'><div class='s-lbl'>المتبقي</div><div class='s-val tk' id='statusTimer'>" + String(rem/60) + ":" + remS + "</div></div>";
    }
  } else {
    html += "<div class='s-item'><div class='s-lbl'>الدينمو</div><div class='s-val off'>متوقف</div></div>";
  }
  html += "</div></div>";

  // بطاقة التحكم
  html += "<div class='c-card'>";
  if (systemError) {
    html += "<div class='sec-lbl'>إجراء مطلوب</div>";
    html += "<a href='/reset' class='btn red full'>إعادة ضبط النظام</a>";
  } else {
    html += "<div class='sec-lbl'>التحكم</div>";
    if (manualMode) {
      html += "<div class='pump-ctrl-box'>";
      html += "<span style='font-size:13px;color:var(--g800);font-weight:700;'>تشغيل الدينمو</span>";
      if (isPumping) {
        html += "<a href='/manual-off' class='switch-link' title='انقر لإيقاف الدينمو'>";
        html += "<span class='switch-lbl on'>يعمل</span>";
        html += "<div class='switch-box on'><div class='switch-thumb'></div></div></a>";
      } else {
        html += "<a href='/manual-on' class='switch-link' title='انقر لتشغيل الدينمو'>";
        html += "<span class='switch-lbl off'>متوقف</span>";
        html += "<div class='switch-box off'><div class='switch-thumb'></div></div></a>";
      }
      html += "</div>";
    }
    html += "<a href='/reset' class='btn ghost full'>تصفير النظام</a>";
    if (manualMode) {
      html += "<div class='tmr'><div class='tmr-title'>تايمر الإيقاف التلقائي</div>";
      if (manualTimerActive && isPumping) {
        long rem=((long)manualTimerDuration-(long)(millis()-pumpStartTime))/1000;
        if(rem<0)rem=0;
        String remS=(rem%60<10?"0":"")+String(rem%60);
        html += "<div class='tmr-big' id='timerBig'>" + String(rem/60) + ":" + remS + "</div>";
        html += "<a href='/cancel-timer' class='btn amber'>إلغاء التايمر</a>";
      } else {
        html += "<form action='/set-manual-timer' method='GET' style='margin:0'>";
        html += "<div class='tmr-row'>";
        html += "<input type='number' name='min' class='tmr-inp' min='1' max='" + String(maxPumpTime/60000UL) + "' placeholder='دقائق' required>";
        html += "<button type='submit' class='btn blue' style='padding:9px 14px'>تفعيل</button></div></form>";
        html += "<div class='tmr-hint'>أدخل عدد الدقائق (1 - " + String(maxPumpTime/60000UL) + ")</div>";
      }
      html += "</div>";
    }
  }
  html += "</div>";

  // التبويبات
  html += "<div class='tabs'>";
  html += "<div class='tabs-nav'>";
  html += "<button class='tab-btn active' onclick=\"openTab(event,'logPane')\">سجل العمليات</button>";
  html += "<button class='tab-btn' onclick=\"openTab(event,'setPane')\">الإعدادات</button>";
  html += "</div>";

  // السجل
  html += "<div id='logPane' class='tab-pane active'><div class='log-list' id='logList'>";
  for (int i = 0; i < 10; i++) {
    int idx = (logIndex-1-i+10)%10;
    if (operationLog[idx] != "") html += "<div class='log-item'>" + operationLog[idx] + "</div>";
  }
  html += "</div></div>";

  // الإعدادات
  html += "<div id='setPane' class='tab-pane'>";

  // قسم إضافة / إعداد شبكة الواي فاي
  html += "<div class='set-section'>";
  html += "<div class='set-title'>إعداد شبكة الواي فاي (Wi-Fi)</div>";
  if (WiFi.status() == WL_CONNECTED) {
    html += "<div style='font-size:12px;color:var(--green);font-weight:700;margin-bottom:8px;'>متصل حالياً بـ: " + wifiSSID + " (" + WiFi.localIP().toString() + ")</div>";
  } else {
    html += "<div style='font-size:12px;color:var(--amber);font-weight:700;margin-bottom:8px;'>يعمل بنقطة البث المباشر (SmartPump-Setup)</div>";
  }
  html += "<form action='/set-wifi' method='GET'>";
  html += "<div class='set-row'><span class='set-lbl'>اسم الشبكة (SSID)</span>";
  html += "<input type='text' name='ssid' class='set-inp' style='width:140px;text-align:right' value='" + wifiSSID + "' placeholder='اسم الشبكة' required></div>";
  html += "<div class='set-row'><span class='set-lbl'>كلمة المرور</span>";
  html += "<input type='password' name='password' class='set-inp' style='width:140px;text-align:right' placeholder='كلمة المرور'></div>";
  html += "<button type='submit' class='btn blue full' style='margin-top:6px'>حفظ والاتصال بالشبكة</button>";
  html += "</form>";
  if (wifiSSID.length() > 0) {
    html += "<a href='/forget-wifi' class='btn ghost full' style='margin-top:6px;color:var(--red)'>مسح الشبكة المحفوظة</a>";
  }
  html += "</div>";

  // قسم إعدادات حماية المضخة ووقت الهدوء
  html += "<form action='/set-timeout' method='GET'>";
  html += "<div class='set-section'>";
  html += "<div class='set-title'>حماية المضخة</div>";
  html += "<div class='set-row'><span class='set-lbl'>وقت الطوارئ (دقائق)</span>";
  html += "<input type='number' name='minutes' class='set-inp' min='1' max='300' value='" + String(maxPumpTime/60000UL) + "' required></div>";
  html += "<div class='set-row'><span class='set-lbl'>كشف رفع الماء (ثواني)</span>";
  html += "<input type='number' name='liftSec' class='set-inp' min='1' max='300' value='" + String(liftTimeout/1000UL) + "' required></div>";
  html += "</div>";

  html += "<div class='set-section'>";
  html += "<div class='set-title'>وقت الهدوء</div>";
  html += "<div class='set-row'><span class='set-lbl'>تفعيل وقت الهدوء</span>";
  html += "<input type='checkbox' name='quietEnabled' value='1' style='width:18px;height:18px;cursor:pointer;accent-color:var(--blue)' " + String(quietModeEnabled ? "checked" : "") + "></div>";
  html += "<div class='set-row'><span class='set-lbl'>ساعة البدء (0-23)</span>";
  html += "<input type='number' name='quietStart' class='set-inp' min='0' max='23' value='" + String(quietStartHour) + "'></div>";
  html += "<div class='set-row'><span class='set-lbl'>ساعة الانتهاء (0-23)</span>";
  html += "<input type='number' name='quietEnd' class='set-inp' min='0' max='23' value='" + String(quietEndHour) + "'></div>";
  html += "</div>";

  html += "<button type='submit' class='btn blue full' style='margin-top:4px'>حفظ إعدادات النظام</button>";
  html += "<div class='set-hint'>الطوارئ: 1-300 دقيقة &nbsp;|&nbsp; الرفع: 1-300 ثانية &nbsp;|&nbsp; الهدوء: 0-23</div>";
  html += "</form></div>";

  html += "</div></div>"; // tabs + page

  // جافا سكريبت للتبويب والتحديث اللحظي
  html += "<script>";
  html += "function openTab(e, tabId) {";
  html += "  document.querySelectorAll('.tab-pane').forEach(function(p){ p.classList.remove('active'); });";
  html += "  document.querySelectorAll('.tab-btn').forEach(function(b){ b.classList.remove('active'); });";
  html += "  var target = document.getElementById(tabId);";
  html += "  if (target) target.classList.add('active');";
  html += "  if (e && e.currentTarget) e.currentTarget.classList.add('active');";
  html += "}";
  html += "function updateData() {";
  html += "  fetch('/status').then(function(r){ return r.json(); }).then(function(d){";
  html += "    var st = document.getElementById('statusText'); if (st) st.textContent = d.status;";
  html += "    var pr = document.getElementById('pumpRow'); if (pr) pr.innerHTML = d.pumpRow;";
  html += "    var ll = document.getElementById('logList'); if (ll) ll.innerHTML = d.logs;";
  html += "    var tl = document.getElementById('sysTimeLabel'); if (tl) tl.textContent = d.systemTime;";
  html += "    var tb = document.getElementById('timerBig');";
  html += "    if (tb && d.timerRemaining >= 0) {";
  html += "      var m = Math.floor(d.timerRemaining / 60);";
  html += "      var s = d.timerRemaining % 60;";
  html += "      tb.textContent = m + ':' + (s < 10 ? '0' : '') + s;";
  html += "    }";
  html += "    var tc = document.getElementById('statusTimer');";
  html += "    if (tc && d.timerRemaining >= 0) {";
  html += "      var m = Math.floor(d.timerRemaining / 60);";
  html += "      var s = d.timerRemaining % 60;";
  html += "      tc.textContent = m + ':' + (s < 10 ? '0' : '') + s;";
  html += "    }";
  html += "  }).catch(function(err){});";
  html += "}";
  html += "setInterval(updateData, 1000);";
  html += "</script></body></html>";

  server.send(200, "text/html", html);
}

void handleStatus() {
  String json = "{";
  json += "\"status\":\"" + currentStatus + "\",";
  
  String pumpRow = "";
  if (isPumping && !systemError) {
    long elapsed = (millis() - pumpStartTime) / 1000;
    String elS = (elapsed % 60 < 10 ? "0" : "") + String(elapsed % 60);
    pumpRow += "<div class='s-item'><div class='s-lbl'>الدينمو</div><div class='s-val on'>يعمل</div></div>";
    pumpRow += "<div class='s-item'><div class='s-lbl'>مدة التشغيل</div><div class='s-val tk'>" + String(elapsed / 60) + ":" + elS + "</div></div>";
    
    if (manualTimerActive) {
      long remaining = ((long)manualTimerDuration - (long)(millis() - pumpStartTime)) / 1000;
      if (remaining < 0) remaining = 0;
      String remS = (remaining % 60 < 10 ? "0" : "") + String(remaining % 60);
      pumpRow += "<div class='s-item'><div class='s-lbl'>المتبقي</div><div class='s-val tk' id='statusTimer'>" + String(remaining / 60) + ":" + remS + "</div></div>";
    }
  } else {
    pumpRow += "<div class='s-item'><div class='s-lbl'>الدينمو</div><div class='s-val off'>متوقف</div></div>";
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
  json += "\"logs\":\"" + logs + "\",";

  String currentTimeStr = "--:--:--";
  time_t nowTime = time(nullptr);
  if (nowTime > 1000000000ULL) {
    struct tm* timeinfo = localtime(&nowTime);
    char buf[20];
    sprintf(buf, "%02d:%02d:%02d", timeinfo->tm_hour, timeinfo->tm_min, timeinfo->tm_sec);
    currentTimeStr = String(buf);
  }
  json += "\"systemTime\":\"" + currentTimeStr + "\"";
  json += "}";
  
  server.send(200, "application/json", json);
}

#endif
