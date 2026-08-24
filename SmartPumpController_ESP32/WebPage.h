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
// دالة بناء صفحة الـ HTML المضمونة لـ ESP8266
// ---------------------------------------------------------
void handleRoot() {
  String sysTime = "--:--:--";
  time_t nowT = time(nullptr);
  if (nowT > 1000000000ULL) {
    struct tm* ti = localtime(&nowT);
    char buf[20];
    sprintf(buf, "%02d:%02d:%02d", ti->tm_hour, ti->tm_min, ti->tm_sec);
    sysTime = String(buf);
  }

  String html;
  html.reserve(10000); // ⚡ حجز مسبق في الذاكرة لمنع تجزئة الـ RAM وانقطاع الاستجابة على ESP8266

  html = "<!DOCTYPE html><html lang='ar' dir='rtl'><head><meta charset='UTF-8'>";
  html += "<meta name='viewport' content='width=device-width,initial-scale=1,maximum-scale=1,user-scalable=no'>";
  html += "<title>SmartPump IoT Controller</title>";
  html += "<link rel='preconnect' href='https://fonts.googleapis.com'>";
  html += "<link rel='preconnect' href='https://fonts.gstatic.com' crossorigin>";
  html += "<link href='https://fonts.googleapis.com/css2?family=Tajawal:wght@500;700&display=swap' rel='stylesheet'>";
  html += "<style>";
  html += ":root{--bg:#f0f2f5;--card:#ffffff;--border:#e2e6ea;--accent:#1a56db;--green:#2f9e44;--red:#e03131;--amber:#e67700;--txt:#1f2937;--txt-m:#6b7280;--inp:#f8f9fb;}";
  html += "*{box-sizing:border-box;margin:0;padding:0;font-family:'Tajawal',sans-serif;}";
  html += "body{background:var(--bg);color:var(--txt);min-height:100vh;display:flex;justify-content:center;padding:12px 12px 76px;}";
  html += ".app{width:100%;max-width:440px;display:flex;flex-direction:column;gap:12px;}";
  html += ".hdr,.card,.bar{background:var(--card);border:1px solid var(--border);border-radius:14px;padding:14px;box-shadow:0 1px 4px rgba(0,0,0,0.04);}";
  html += ".hdr{display:flex;align-items:center;justify-content:space-between;}";
  html += ".brand{display:flex;align-items:center;gap:8px;font-size:16px;font-weight:700;}";
  html += ".clock{font-size:13px;font-weight:700;color:var(--accent);background:rgba(26,86,219,0.08);padding:4px 8px;border-radius:6px;border:1px solid rgba(26,86,219,0.15);}";
  html += ".bar{display:flex;align-items:center;justify-content:space-between;padding:10px 14px;}";
  html += ".badge{font-size:12px;font-weight:700;padding:4px 10px;border-radius:20px;}";
  html += ".badge.auto{background:rgba(47,158,68,0.12);color:var(--green);}";
  html += ".badge.manual{background:rgba(26,86,219,0.12);color:var(--accent);}";
  html += ".badge.err{background:rgba(224,49,49,0.12);color:var(--red);}";
  html += ".hero{position:relative;overflow:hidden;}";
  html += ".hero::before{content:'';position:absolute;top:0;right:0;width:100%;height:4px;}";
  if (systemError)    html += ".hero::before{background:var(--red);}";
  else if (isPumping) html += ".hero::before{background:var(--accent);}";
  else                html += ".hero::before{background:var(--green);}";
  html += ".status{font-size:16px;font-weight:700;margin-bottom:12px;}";
  if (systemError)    html += ".status{color:var(--red);}";
  else if (isPumping) html += ".status{color:var(--accent);}";
  else                html += ".status{color:var(--green);}";
  html += ".grid{display:grid;grid-template-columns:repeat(3,1fr);gap:6px;padding-top:10px;border-top:1px solid var(--border);}";
  html += ".tile{background:var(--inp);border:1px solid var(--border);border-radius:8px;padding:8px;text-align:center;}";
  html += ".lbl{font-size:10px;color:var(--txt-m);margin-bottom:2px;}";
  html += ".val{font-size:12px;font-weight:700;}";
  html += ".val.on{color:var(--green);}.val.timer{color:var(--accent);}";
  html += ".sw-box{display:flex;background:var(--inp);border:1px solid var(--border);padding:3px;border-radius:8px;gap:4px;margin-bottom:10px;}";
  html += ".sw-btn{flex:1;padding:7px;text-align:center;font-size:12px;font-weight:700;border-radius:6px;text-decoration:none;color:var(--txt-m);}";
  html += ".sw-btn.active{background:white;color:var(--accent);box-shadow:0 1px 3px rgba(0,0,0,0.1);}";
  html += ".btn{display:flex;align-items:center;justify-content:center;padding:10px;border:none;border-radius:8px;font-size:13px;font-weight:700;cursor:pointer;text-decoration:none;}";
  html += ".btn-acc{background:var(--accent);color:white;}";
  html += ".btn-grn{background:var(--green);color:white;}";
  html += ".btn-red{background:var(--red);color:white;}";
  html += ".btn-amb{background:var(--amber);color:white;}";
  html += ".btn-out{background:var(--inp);color:var(--txt);border:1px solid var(--border);}";
  html += ".p-grid{display:grid;grid-template-columns:repeat(4,1fr);gap:4px;margin-top:6px;}";
  html += ".p-btn{background:white;border:1px solid var(--border);color:var(--txt);padding:6px 2px;font-size:11px;font-weight:700;border-radius:6px;text-align:center;text-decoration:none;}";
  html += ".tab-nav{position:fixed;bottom:0;left:0;right:0;width:100%;max-width:440px;margin:0 auto;background:rgba(255,255,255,0.96);backdrop-filter:blur(12px);border-top:1px solid var(--border);padding:6px 8px;z-index:9999;display:flex;gap:4px;box-shadow:0 -2px 10px rgba(0,0,0,0.06);}";
  html += ".tab-link{flex:1;padding:6px 2px;text-align:center;font-size:11px;font-weight:700;color:var(--txt-m);border:none;background:transparent;border-radius:8px;cursor:pointer;display:flex;flex-direction:column;align-items:center;gap:2px;}";
  html += ".tab-link.active{background:rgba(26,86,219,0.08);color:var(--accent);}";
  html += ".tab-icon, .tab-icon svg{pointer-events:none;}";
  html += ".tab-pane{display:none;}.tab-pane.active{display:block;}";
  html += ".log-list{display:flex;flex-direction:column;gap:4px;max-height:200px;overflow-y:auto;}";
  html += ".log-item{font-size:12px;padding:6px 10px;background:var(--inp);border-radius:6px;border-right:3px solid var(--accent);display:flex;justify-content:space-between;}";
  html += ".fg{margin-bottom:10px;}.fl{display:block;font-size:11px;color:var(--txt-m);margin-bottom:4px;}.fi{width:100%;padding:8px 10px;background:var(--inp);border:1px solid var(--border);border-radius:6px;font-size:12px;outline:none;}";
  html += "</style></head><body>";

  html += "<div class='app'>";
  // 1. الهيدر
  html += "<div class='hdr'><div class='brand'>";
  html += "<svg width='20' height='20' viewBox='0 0 24 24' fill='none' stroke='var(--accent)' stroke-width='2.2'><path d='M12 2.69l5.66 5.66a8 8 0 1 1-11.31 0z'></path></svg>";
  html += "<span>متحكم المضخة الذكي</span></div>";
  html += "<div class='clock' id='sysTimeLabel'>" + sysTime + "</div></div>";

  // 2. حالة الاتصال
  html += "<div class='bar'>";
  if (systemError) {
    html += "<div class='badge err'>طوارئ (مقفل)</div>";
  } else if (manualMode) {
    html += "<div class='badge manual'>الوضع اليدوي</div>";
  } else {
    html += "<div class='badge auto'>الوضع التلقائي</div>";
  }
  if (WiFi.status() == WL_CONNECTED) {
    html += "<div style='font-size:11px;color:var(--txt-m);'>متصل: " + wifiSSID + "</div>";
  } else {
    html += "<div style='font-size:11px;color:var(--amber);'>نقطة بث AP</div>";
  }
  html += "</div>";

  // 3. شريط التنقل السفلي المثبت
  html += "<div class='tab-nav'>";
  html += "<button type='button' class='tab-link active' onclick=\"openTab(this,'dashboardTab')\"><span class='tab-icon'><svg width='18' height='18' viewBox='0 0 24 24' fill='none' stroke='currentColor' stroke-width='2'><path d='M3 9l9-7 9 7v11a2 2 0 0 1-2 2H5a2 2 0 0 1-2-2z'></path><polyline points='9 22 9 12 15 12 15 22'></polyline></svg></span>الرئيسية</button>";
  html += "<button type='button' class='tab-link' onclick=\"openTab(this,'logTab')\"><span class='tab-icon'><svg width='18' height='18' viewBox='0 0 24 24' fill='none' stroke='currentColor' stroke-width='2'><line x1='8' y1='6' x2='21' y2='6'></line><line x1='8' y1='12' x2='21' y2='12'></line><line x1='8' y1='18' x2='21' y2='18'></line><line x1='3' y1='6' x2='3.01' y2='6'></line><line x1='3' y1='12' x2='3.01' y2='12'></line><line x1='3' y1='18' x2='3.01' y2='18'></line></svg></span>السجل</button>";
  html += "<button type='button' class='tab-link' onclick=\"openTab(this,'wifiTab')\"><span class='tab-icon'><svg width='18' height='18' viewBox='0 0 24 24' fill='none' stroke='currentColor' stroke-width='2'><path d='M5 12.55a11 11 0 0 1 14.08 0'></path><path d='M1.42 9a16 16 0 0 1 21.16 0'></path><path d='M8.53 16.11a6 6 0 0 1 6.95 0'></path><line x1='12' y1='20' x2='12.01' y2='20'></line></svg></span>الواي فاي</button>";
  html += "<button type='button' class='tab-link' onclick=\"openTab(this,'settingsTab')\"><span class='tab-icon'><svg width='18' height='18' viewBox='0 0 24 24' fill='none' stroke='currentColor' stroke-width='2'><circle cx='12' cy='12' r='3'></circle><path d='M19.4 15a1.65 1.65 0 0 0 .33 1.82l.06.06a2 2 0 0 1 0 2.83 2 2 0 0 1-2.83 0l-.06-.06a1.65 1.65 0 0 0-1.82-.33 1.65 1.65 0 0 0-1 1.51V21a2 2 0 0 1-2 2 2 2 0 0 1-2-2v-.09A1.65 1.65 0 0 0 9 19.4a1.65 1.65 0 0 0-1.82.33l-.06.06a2 2 0 0 1-2.83 0 2 2 0 0 1 0-2.83l.06-.06a1.65 1.65 0 0 0 .33-1.82 1.65 1.65 0 0 0-1.51-1H3a2 2 0 0 1-2-2 2 2 0 0 1 2-2h.09A1.65 1.65 0 0 0 4.6 9a1.65 1.65 0 0 0-.33-1.82l-.06-.06a2 2 0 0 1 0-2.83 2 2 0 0 1 2.83 0l.06.06a1.65 1.65 0 0 0 1.82.33H9a1.65 1.65 0 0 0 1-1.51V3a2 2 0 0 1 2-2 2 2 0 0 1 2 2v.09a1.65 1.65 0 0 0 1 1.51 1.65 1.65 0 0 0 1.82-.33l.06-.06a2 2 0 0 1 2.83 0 2 2 0 0 1 0 2.83l-.06.06a1.65 1.65 0 0 0-.33 1.82V9a1.65 1.65 0 0 0 1.51 1H21a2 2 0 0 1 2 2 2 2 0 0 1-2 2h-.09a1.65 1.65 0 0 0-1.51 1z'></path></svg></span>الإعدادات</button>";
  html += "</div>";

  // Tab 1: الرئيسية
  html += "<div id='dashboardTab' class='tab-pane active'>";
  html += "<div class='card hero'>";
  html += "<div style='font-size:11px;color:var(--txt-m);font-weight:700;margin-bottom:4px;'>حالة النظام الحالية</div>";
  html += "<div class='status' id='statusText'>" + currentStatus + "</div>";
  html += "<div class='grid' id='pumpRow'>";
  if (isPumping && !systemError) {
    long el = (millis() - pumpStartTime) / 1000;
    String elS = (el % 60 < 10 ? "0" : "") + String(el % 60);
    html += "<div class='tile'><div class='lbl'>الدينمو</div><div class='val on'>يعمل</div></div>";
    html += "<div class='tile'><div class='lbl'>مدة التشغيل</div><div class='val timer'>" + String(el/60) + ":" + elS + "</div></div>";
    if (manualTimerActive) {
      long rem = ((long)manualTimerDuration-(long)(millis()-pumpStartTime))/1000;
      if(rem<0)rem=0;
      String remS=(rem%60<10?"0":"")+String(rem%60);
      html += "<div class='tile'><div class='lbl'>المتبقي</div><div class='val timer' id='statusTimer'>" + String(rem/60) + ":" + remS + "</div></div>";
    } else {
      html += "<div class='tile'><div class='lbl'>المؤقت</div><div class='val'>مفتوح</div></div>";
    }
  } else {
    html += "<div class='tile'><div class='lbl'>الدينمو</div><div class='val'>متوقف</div></div>";
    html += "<div class='tile'><div class='lbl'>مدة التشغيل</div><div class='val'>--:--</div></div>";
    html += "<div class='tile'><div class='lbl'>المؤقت</div><div class='val'>غير مفعّل</div></div>";
  }
  html += "</div></div>";

  html += "<div class='card' style='margin-top:10px;'>";
  html += "<div style='font-size:11px;font-weight:700;color:var(--txt-m);margin-bottom:8px;'>التحكم والتشغيل</div>";
  html += "<div class='sw-box'>";
  if (manualMode) {
    html += "<a href='/toggle-mode' class='sw-btn'>تلقائي</a>";
    html += "<div class='sw-btn active'>يدوي</div>";
  } else {
    html += "<div class='sw-btn active'>تلقائي</div>";
    html += "<a href='/toggle-mode' class='sw-btn'>يدوي</a>";
  }
  html += "</div>";

  if (systemError) {
    html += "<a href='/reset' class='btn btn-red' style='width:100%;'>إعادة ضبط وتصفير النظام</a>";
  } else {
    if (manualMode) {
      if (isPumping) {
        html += "<a href='/manual-off' class='btn btn-red' style='width:100%;margin-bottom:8px;'>إيقاف الدينمو يدوياً</a>";
      } else {
        html += "<a href='/manual-on' class='btn btn-grn' style='width:100%;margin-bottom:8px;'>تشغيل الدينمو يدوياً</a>";
      }
      html += "<div style='background:var(--inp);border:1px solid var(--border);border-radius:8px;padding:8px;'>";
      html += "<div style='font-size:11px;font-weight:700;margin-bottom:4px;'>تايمر الإيقاف السريع:</div>";
      html += "<div class='p-grid'>";
      html += "<a href='/set-manual-timer?min=5' class='p-btn'>5 دقائق</a>";
      html += "<a href='/set-manual-timer?min=10' class='p-btn'>10 دقائق</a>";
      html += "<a href='/set-manual-timer?min=15' class='p-btn'>15 دقيقة</a>";
      html += "<a href='/set-manual-timer?min=30' class='p-btn'>30 دقيقة</a>";
      html += "</div>";
      if (manualTimerActive && isPumping) {
        html += "<a href='/cancel-timer' class='btn btn-amb' style='width:100%;margin-top:6px;padding:6px;'>إلغاء التايمر اليدوي</a>";
      } else {
        html += "<form action='/set-manual-timer' method='GET' style='display:flex;gap:4px;margin-top:6px;'>";
        html += "<input type='number' name='min' class='fi' min='1' max='" + String(maxPumpTime/60000UL) + "' placeholder='دقائق مخصصة' required>";
        html += "<button type='submit' class='btn btn-acc' style='white-space:nowrap;padding:6px 12px;'>تفعيل</button>";
        html += "</form>";
      }
      html += "</div>";
    }
    html += "<a href='/reset' class='btn btn-out' style='width:100%;margin-top:8px;'>تصفير حالة النظام</a>";
  }
  html += "</div></div>";

  // Tab 2: السجل
  html += "<div id='logTab' class='tab-pane'>";
  html += "<div class='card'><div style='font-size:11px;font-weight:700;color:var(--txt-m);margin-bottom:8px;'>سجل الأحداث الأخيرة</div>";
  html += "<div class='log-list' id='logList'>";
  for (int i = 0; i < 10; i++) {
    int idx = (logIndex-1-i+10)%10;
    if (operationLog[idx] != "") {
      html += "<div class='log-item'><span>" + operationLog[idx] + "</span><span style='font-size:10px;color:var(--txt-m);'>أحدث</span></div>";
    }
  }
  html += "</div></div></div>";

  // Tab 3: الواي فاي
  html += "<div id='wifiTab' class='tab-pane'>";
  html += "<div class='card'><div style='font-size:11px;font-weight:700;color:var(--txt-m);margin-bottom:8px;'>إعدادات الواي فاي</div>";
  if (WiFi.status() == WL_CONNECTED) {
    html += "<div style='font-size:12px;color:var(--green);font-weight:700;margin-bottom:8px;'>متصل: " + wifiSSID + " (" + WiFi.localIP().toString() + ")</div>";
  } else {
    html += "<div style='font-size:12px;color:var(--amber);font-weight:700;margin-bottom:8px;'>يعمل بنقطة البث (SmartPump-Setup)</div>";
  }
  html += "<form action='/set-wifi' method='GET'>";
  html += "<div class='fg'><label class='fl'>اسم الشبكة (SSID)</label><input type='text' name='ssid' class='fi' value='" + wifiSSID + "' required></div>";
  html += "<div class='fg'><label class='fl'>كلمة المرور</label><input type='password' name='password' class='fi'></div>";
  html += "<button type='submit' class='btn btn-acc' style='width:100%;'>حفظ والاتصال</button>";
  html += "</form>";
  if (wifiSSID.length() > 0) {
    html += "<a href='/forget-wifi' class='btn btn-out' style='width:100%;margin-top:6px;color:var(--red);'>مسح الشبكة المحفوظة</a>";
  }
  html += "</div></div>";

  // Tab 4: الإعدادات
  html += "<div id='settingsTab' class='tab-pane'>";
  html += "<form action='/set-timeout' method='GET'>";
  html += "<div class='card' style='margin-bottom:10px;'>";
  html += "<div style='font-size:11px;font-weight:700;color:var(--txt-m);margin-bottom:8px;'>حماية المضخة</div>";
  html += "<div class='fg'><label class='fl'>وقت الطوارئ الأقصى (دقائق)</label><input type='number' name='minutes' class='fi' min='1' max='300' value='" + String(maxPumpTime/60000UL) + "' required></div>";
  html += "<div class='fg'><label class='fl'>كشف رفع الماء (ثواني)</label><input type='number' name='liftSec' class='fi' min='1' max='300' value='" + String(liftTimeout/1000UL) + "' required></div>";
  html += "</div>";

  html += "<div class='card'>";
  html += "<div style='font-size:11px;font-weight:700;color:var(--txt-m);margin-bottom:8px;'>وقت الهدوء</div>";
  html += "<div class='fg' style='display:flex;align-items:center;justify-content:space-between;'><span class='fl' style='margin:0;'>تفعيل وقت الهدوء</span><input type='checkbox' name='quietEnabled' value='1' style='width:18px;height:18px;' " + String(quietModeEnabled ? "checked" : "") + "></div>";
  html += "<div style='display:grid;grid-template-columns:1fr 1fr;gap:8px;'>";
  html += "<div class='fg'><label class='fl'>ساعة البدء (0-23)</label><input type='number' name='quietStart' class='fi' min='0' max='23' value='" + String(quietStartHour) + "'></div>";
  html += "<div class='fg'><label class='fl'>ساعة الانتهاء (0-23)</label><input type='number' name='quietEnd' class='fi' min='0' max='23' value='" + String(quietEndHour) + "'></div>";
  html += "</div>";
  html += "<button type='submit' class='btn btn-acc' style='width:100%;margin-top:6px;'>حفظ الإعدادات</button>";
  html += "</div></form></div>";

  html += "</div>"; // app container

  // JS Script
  html += "<script>";
  html += "function openTab(btn, tabId) {";
  html += "  var p = document.getElementsByClassName('tab-pane');";
  html += "  for(var i=0; i<p.length; i++){ p[i].style.display='none'; p[i].classList.remove('active'); }";
  html += "  var l = document.getElementsByClassName('tab-link');";
  html += "  for(var j=0; j<l.length; j++){ l[j].classList.remove('active'); }";
  html += "  var t = document.getElementById(tabId);";
  html += "  if(t){ t.style.display='block'; t.classList.add('active'); }";
  html += "  if(btn){ btn.classList.add('active'); }";
  html += "}";
  html += "function updateData() {";
  html += "  fetch('/status').then(function(r){ return r.json(); }).then(function(d){";
  html += "    var st = document.getElementById('statusText'); if(st && d.status) st.textContent = d.status;";
  html += "    var pr = document.getElementById('pumpRow'); if(pr && d.pumpRow) pr.innerHTML = d.pumpRow;";
  html += "    var ll = document.getElementById('logList'); if(ll && d.logs) ll.innerHTML = d.logs;";
  html += "    var tl = document.getElementById('sysTimeLabel'); if(tl && d.systemTime) tl.textContent = d.systemTime;";
  html += "  }).catch(function(){});";
  html += "}";
  html += "setInterval(updateData, 1000);";
  html += "</script></body></html>";

  server.send(200, "text/html", html);
}

// ---------------------------------------------------------
// دالة الاستجابة اللحظية JSON
// ---------------------------------------------------------
void handleStatus() {
  String json = "{";
  json += "\"status\":\"" + currentStatus + "\",";
  
  String pumpRow = "";
  if (isPumping && !systemError) {
    long elapsed = (millis() - pumpStartTime) / 1000;
    String elS = (elapsed % 60 < 10 ? "0" : "") + String(elapsed % 60);
    pumpRow += "<div class='tile'><div class='lbl'>الدينمو</div><div class='val on'>يعمل</div></div>";
    pumpRow += "<div class='tile'><div class='lbl'>مدة التشغيل</div><div class='val timer'>" + String(elapsed / 60) + ":" + elS + "</div></div>";
    
    if (manualTimerActive) {
      long remaining = ((long)manualTimerDuration - (long)(millis() - pumpStartTime)) / 1000;
      if (remaining < 0) remaining = 0;
      String remS = (remaining % 60 < 10 ? "0" : "") + String(remaining % 60);
      pumpRow += "<div class='tile'><div class='lbl'>المتبقي</div><div class='val timer' id='statusTimer'>" + String(remaining / 60) + ":" + remS + "</div></div>";
    } else {
      pumpRow += "<div class='tile'><div class='lbl'>المؤقت</div><div class='val'>مفتوح</div></div>";
    }
  } else {
    pumpRow += "<div class='tile'><div class='lbl'>الدينمو</div><div class='val'>متوقف</div></div>";
    pumpRow += "<div class='tile'><div class='lbl'>مدة التشغيل</div><div class='val'>--:--</div></div>";
    pumpRow += "<div class='tile'><div class='lbl'>المؤقت</div><div class='val'>غير مفعّل</div></div>";
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
      logs += "<div class='log-item'><span>" + operationLog[idx] + "</span><span style='font-size:10px;color:var(--txt-m);'>أحدث</span></div>";
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
