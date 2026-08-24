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
// دالة بناء صفحة الـ HTML (واجهة لوحة تحكم IoT الاحترافية)
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
  html += "<head><meta charset='UTF-8'><meta name='viewport' content='width=device-width,initial-scale=1,maximum-scale=1,user-scalable=no'>";
  html += "<title>SmartPump IoT Dashboard</title>";
  html += "<link rel='preconnect' href='https://fonts.googleapis.com'>";
  html += "<link rel='preconnect' href='https://fonts.gstatic.com' crossorigin>";
  html += "<link href='https://fonts.googleapis.com/css2?family=Tajawal:wght@400;500;700;900&display=swap' rel='stylesheet'>";
  
  html += "<style>";
  // متسلسلة ألوان وتنسيقات أنظمة IoT المتطورة (Dark Glassmorphism Theme)
  html += ":root{";
  html += "--bg:#0b0f19;--card-bg:#151d30;--card-border:rgba(255,255,255,0.08);";
  html += "--accent:#0284c7;--accent-glow:rgba(2,132,199,0.25);";
  html += "--green:#10b981;--green-glow:rgba(16,185,129,0.25);";
  html += "--red:#f43f5e;--red-glow:rgba(244,63,94,0.25);";
  html += "--amber:#f59e0b;--amber-glow:rgba(245,158,11,0.25);";
  html += "--txt:#f8fafc;--txt-muted:#94a3b8;--input-bg:#0f172a;";
  html += "--radius:16px;--radius-sm:10px;";
  html += "}";

  html += "*{box-sizing:border-box;margin:0;padding:0;font-family:'Tajawal',-apple-system,sans-serif;}";
  html += "body{background:var(--bg);color:var(--txt);min-height:100vh;display:flex;justify-content:center;padding:12px 12px 36px;}";
  html += ".app-container{width:100%;max-width:440px;display:flex;flex-direction:column;gap:14px;}";

  // الهيدر الرئيسي والشريط العلمي
  html += ".hdr{display:flex;align-items:center;justify-content:space-between;background:var(--card-bg);border:1px solid var(--card-border);border-radius:var(--radius);padding:14px 16px;}";
  html += ".hdr-brand{display:flex;align-items:center;gap:10px;}";
  html += ".hdr-icon{width:36px;height:36px;background:rgba(2,132,199,0.15);border:1px solid var(--accent);border-radius:10px;display:flex;align-items:center;justify-content:center;font-size:18px;}";
  html += ".hdr-title{font-size:16px;font-weight:700;color:var(--txt);}";
  html += ".hdr-subtitle{font-size:11px;color:var(--txt-muted);}";
  html += ".hdr-clock{font-size:13px;font-weight:700;color:var(--accent);background:rgba(2,132,199,0.1);padding:6px 10px;border-radius:8px;font-variant-numeric:tabular-nums;border:1px solid rgba(2,132,199,0.2);}";

  // شريط حالة الاتصال والوضع
  html += ".conn-bar{display:flex;align-items:center;justify-content:space-between;background:var(--card-bg);border:1px solid var(--card-border);border-radius:var(--radius);padding:10px 14px;}";
  html += ".badge{display:inline-flex;align-items:center;gap:6px;font-size:12px;font-weight:700;padding:4px 10px;border-radius:20px;}";
  html += ".badge-dot{width:7px;height:7px;border-radius:50%;}";
  html += ".badge-auto{background:rgba(16,185,129,0.15);color:var(--green);border:1px solid var(--green-glow);}";
  html += ".badge-auto .badge-dot{background:var(--green);box-shadow:0 0 8px var(--green);}";
  html += ".badge-manual{background:rgba(2,132,199,0.15);color:var(--accent);border:1px solid var(--accent-glow);}";
  html += ".badge-manual .badge-dot{background:var(--accent);box-shadow:0 0 8px var(--accent);}";
  html += ".badge-err{background:rgba(244,63,94,0.15);color:var(--red);border:1px solid var(--red-glow);}";
  html += ".badge-err .badge-dot{background:var(--red);box-shadow:0 0 8px var(--red);}";
  
  html += ".wifi-status{font-size:11px;color:var(--txt-muted);display:flex;align-items:center;gap:6px;}";
  html += ".wifi-dot{width:6px;height:6px;border-radius:50%;}";
  html += ".wifi-dot.on{background:var(--green);box-shadow:0 0 6px var(--green);}";
  html += ".wifi-dot.off{background:var(--amber);box-shadow:0 0 6px var(--amber);}";

  // البطاقة المحورية للحالة (Hero Card)
  html += ".hero-card{background:var(--card-bg);border:1px solid var(--card-border);border-radius:var(--radius);padding:18px;position:relative;overflow:hidden;}";
  html += ".hero-card::before{content:'';position:absolute;top:0;right:0;width:100%;height:3px;}";
  if (systemError)    html += ".hero-card::before{background:var(--red);box-shadow:0 0 12px var(--red);}";
  else if (isPumping) html += ".hero-card::before{background:var(--accent);box-shadow:0 0 12px var(--accent);}";
  else                html += ".hero-card::before{background:var(--green);box-shadow:0 0 12px var(--green);}";

  html += ".hero-status{font-size:17px;font-weight:700;margin-bottom:14px;line-height:1.4;}";
  if (systemError)    html += ".hero-status{color:var(--red);}";
  else if (isPumping) html += ".hero-status{color:var(--accent);}";
  else                html += ".hero-status{color:var(--green);}";

  // مصفوفة المؤشرات السريعة (Metrics Grid)
  html += ".metrics-grid{display:grid;grid-template-columns:repeat(3,1fr);gap:8px;padding-top:12px;border-top:1px solid var(--card-border);}";
  html += ".metric-tile{background:rgba(15,23,42,0.6);border:1px solid var(--card-border);border-radius:var(--radius-sm);padding:10px;text-align:center;}";
  html += ".metric-lbl{font-size:10px;color:var(--txt-muted);font-weight:600;margin-bottom:4px;}";
  html += ".metric-val{font-size:13px;font-weight:700;color:var(--txt);}";
  html += ".metric-val.on{color:var(--green);}.metric-val.off{color:var(--txt-muted);}.metric-val.timer{color:var(--accent);font-variant-numeric:tabular-nums;}";

  // بطاقات التفاعل والتحكم
  html += ".section-card{background:var(--card-bg);border:1px solid var(--card-border);border-radius:var(--radius);padding:16px;}";
  html += ".section-hdr{display:flex;align-items:center;justify-content:space-between;margin-bottom:12px;}";
  html += ".section-title{font-size:12px;font-weight:700;color:var(--txt-muted);letter-spacing:0.5px;text-transform:uppercase;}";

  // أزرار التحكم
  html += ".mode-switch-box{display:flex;background:var(--input-bg);border:1px solid var(--card-border);padding:4px;border-radius:var(--radius-sm);gap:4px;margin-bottom:12px;}";
  html += ".mode-btn{flex:1;padding:8px;text-align:center;font-size:13px;font-weight:700;border-radius:7px;text-decoration:none;color:var(--txt-muted);transition:0.2s;}";
  html += ".mode-btn.active-auto{background:rgba(16,185,129,0.2);color:var(--green);border:1px solid var(--green-glow);}";
  html += ".mode-btn.active-manual{background:rgba(2,132,199,0.2);color:var(--accent);border:1px solid var(--accent-glow);}";

  html += ".btn{display:flex;align-items:center;justify-content:center;padding:12px 14px;border:none;border-radius:var(--radius-sm);font-size:14px;font-weight:700;cursor:pointer;text-decoration:none;transition:0.2s;}";
  html += ".btn:active{transform:scale(0.98);opacity:0.9;}";
  html += ".btn-accent{background:var(--accent);color:white;box-shadow:0 4px 12px var(--accent-glow);}";
  html += ".btn-green{background:var(--green);color:white;box-shadow:0 4px 12px var(--green-glow);}";
  html += ".btn-red{background:var(--red);color:white;box-shadow:0 4px 12px var(--red-glow);}";
  html += ".btn-amber{background:var(--amber);color:white;box-shadow:0 4px 12px var(--amber-glow);}";
  html += ".btn-outline{background:transparent;color:var(--txt-muted);border:1px solid var(--card-border);}";
  html += ".btn-outline:hover{background:rgba(255,255,255,0.05);color:var(--txt);}";
  html += ".btn-full{width:100%;}";

  // أزرار التايمر السريع
  html += ".preset-grid{display:grid;grid-template-columns:repeat(4,1fr);gap:6px;margin-top:8px;}";
  html += ".preset-btn{background:var(--input-bg);border:1px solid var(--card-border);color:var(--txt);padding:8px 4px;font-size:12px;font-weight:700;border-radius:8px;text-align:center;text-decoration:none;transition:0.15s;}";
  html += ".preset-btn:hover{border-color:var(--accent);color:var(--accent);}";

  // التبويبات التنظيمية
  html += ".tab-nav{display:flex;background:var(--card-bg);border:1px solid var(--card-border);padding:4px;border-radius:var(--radius);gap:4px;}";
  html += ".tab-link{flex:1;padding:10px 4px;text-align:center;font-size:12px;font-weight:700;color:var(--txt-muted);border:none;background:transparent;border-radius:var(--radius-sm);cursor:pointer;transition:0.2s;}";
  html += ".tab-link.active{background:rgba(2,132,199,0.15);color:var(--accent);border:1px solid var(--accent-glow);}";
  html += ".tab-content{display:none;}.tab-content.active{display:block;}";

  // القوائم والإعدادات
  html += ".log-list{display:flex;flex-direction:column;gap:6px;max-height:220px;overflow-y:auto;}";
  html += ".log-item{font-size:12px;color:var(--txt-muted);padding:8px 12px;background:var(--input-bg);border-radius:8px;border-right:3px solid var(--accent);display:flex;align-items:center;justify-content:space-between;}";

  html += ".form-group{margin-bottom:12px;}";
  html += ".form-label{display:block;font-size:12px;color:var(--txt-muted);font-weight:600;margin-bottom:6px;}";
  html += ".form-input{width:100%;padding:10px 12px;background:var(--input-bg);border:1px solid var(--card-border);border-radius:var(--radius-sm);color:var(--txt);font-size:13px;outline:none;}";
  html += ".form-input:focus{border-color:var(--accent);}";

  html += "</style></head><body>";
  html += "<div class='app-container'>";

  // 1. الهيدر وشريط الحالة
  html += "<div class='hdr'>";
  html += "<div class='hdr-brand'><div class='hdr-icon'>💧</div>";
  html += "<div><div class='hdr-title'>متحكم المضخة الذكي</div><div class='hdr-subtitle'>SmartPump IoT System</div></div></div>";
  html += "<div class='hdr-clock' id='sysTimeLabel'>" + sysTime + "</div></div>";

  // 2. شريط حالة الاتصال والوضع
  html += "<div class='conn-bar'>";
  if (systemError) {
    html += "<div class='badge badge-err'><span class='badge-dot'></span>حالة الطوارئ (مقفل)</div>";
  } else if (manualMode) {
    html += "<div class='badge badge-manual'><span class='badge-dot'></span>الوضع اليدوي</div>";
  } else {
    html += "<div class='badge badge-auto'><span class='badge-dot'></span>الوضع التلقائي</div>";
  }

  if (WiFi.status() == WL_CONNECTED) {
    html += "<div class='wifi-status'><span class='wifi-dot on'></span>متصل: " + wifiSSID + "</div>";
  } else {
    html += "<div class='wifi-status'><span class='wifi-dot off'></span>نقطة بث AP</div>";
  }
  html += "</div>";

  // 3. التبويبات التنظيمية للوحة التحكم
  html += "<div class='tab-nav'>";
  html += "<button class='tab-link active' onclick=\"openTab(event,'dashboardTab')\">الرئيسية</button>";
  html += "<button class='tab-link' onclick=\"openTab(event,'logTab')\">سجل العمليات</button>";
  html += "<button class='tab-link' onclick=\"openTab(event,'wifiTab')\">الواي فاي</button>";
  html += "<button class='tab-link' onclick=\"openTab(event,'settingsTab')\">الإعدادات</button>";
  html += "</div>";

  // ---------------------------------------------------------
  // تبويب الرئيسية (Dashboard Tab)
  // ---------------------------------------------------------
  html += "<div id='dashboardTab' class='tab-content active'>";
  
  // البطاقة المحورية للحالة
  html += "<div class='hero-card'>";
  html += "<div style='font-size:11px;color:var(--txt-muted);font-weight:700;margin-bottom:6px;'>حالة النظام الحالية</div>";
  html += "<div class='hero-status' id='statusText'>" + currentStatus + "</div>";

  html += "<div class='metrics-grid' id='pumpRow'>";
  if (isPumping && !systemError) {
    long el = (millis() - pumpStartTime) / 1000;
    String elS = (el % 60 < 10 ? "0" : "") + String(el % 60);
    html += "<div class='metric-tile'><div class='metric-lbl'>الدينمو</div><div class='metric-val on'>يعمل ⚡</div></div>";
    html += "<div class='metric-tile'><div class='metric-lbl'>مدة التشغيل</div><div class='metric-val timer'>" + String(el/60) + ":" + elS + "</div></div>";
    if (manualTimerActive) {
      long rem = ((long)manualTimerDuration-(long)(millis()-pumpStartTime))/1000;
      if(rem<0)rem=0;
      String remS=(rem%60<10?"0":"")+String(rem%60);
      html += "<div class='metric-tile'><div class='metric-lbl'>المتبقي</div><div class='metric-val timer' id='statusTimer'>" + String(rem/60) + ":" + remS + "</div></div>";
    } else {
      html += "<div class='metric-tile'><div class='metric-lbl'>المؤقت</div><div class='metric-val off'>مفتوح</div></div>";
    }
  } else {
    html += "<div class='metric-tile'><div class='metric-lbl'>الدينمو</div><div class='metric-val off'>متوقف</div></div>";
    html += "<div class='metric-tile'><div class='metric-lbl'>مدة التشغيل</div><div class='metric-val off'>--:--</div></div>";
    html += "<div class='metric-tile'><div class='metric-lbl'>المؤقت</div><div class='metric-val off'>غير مفعّل</div></div>";
  }
  html += "</div></div>";

  // قسم التحكم السريع
  html += "<div class='section-card' style='margin-top:14px;'>";
  html += "<div class='section-hdr'><div class='section-title'>لوحة التحكم والتشغيل</div></div>";

  // سويتش تبديل الوضع
  html += "<div class='mode-switch-box'>";
  if (manualMode) {
    html += "<a href='/toggle-mode' class='mode-btn'>التشغيل التلقائي</a>";
    html += "<div class='mode-btn active-manual'>التشغيل اليدوي</div>";
  } else {
    html += "<div class='mode-btn active-auto'>التشغيل التلقائي</div>";
    html += "<a href='/toggle-mode' class='mode-btn'>التشغيل اليدوي</a>";
  }
  html += "</div>";

  if (systemError) {
    html += "<a href='/reset' class='btn btn-red btn-full'>إعادة ضبط وتصفير النظام</a>";
  } else {
    if (manualMode) {
      if (isPumping) {
        html += "<a href='/manual-off' class='btn btn-red btn-full' style='margin-bottom:10px;'>إيقاف الدينمو يدوياً 🛑</a>";
      } else {
        html += "<a href='/manual-on' class='btn btn-green btn-full' style='margin-bottom:10px;'>تشغيل الدينمو يدوياً ▶</a>";
      }

      // أزرار التايمر السريع
      html += "<div style='background:var(--input-bg);border:1px solid var(--card-border);border-radius:var(--radius-sm);padding:12px;margin-top:8px;'>";
      html += "<div style='font-size:12px;font-weight:700;color:var(--txt);margin-bottom:6px;'>تايمر الإيقاف السريع:</div>";
      html += "<div class='preset-grid'>";
      html += "<a href='/set-manual-timer?min=5' class='preset-btn'>5 دقائق</a>";
      html += "<a href='/set-manual-timer?min=10' class='preset-btn'>10 دقائق</a>";
      html += "<a href='/set-manual-timer?min=15' class='preset-btn'>15 دقيقة</a>";
      html += "<a href='/set-manual-timer?min=30' class='preset-btn'>30 دقيقة</a>";
      html += "</div>";

      if (manualTimerActive && isPumping) {
        html += "<a href='/cancel-timer' class='btn btn-amber btn-full' style='margin-top:10px;padding:8px;'>إلغاء التايمر اليدوي</a>";
      } else {
        html += "<form action='/set-manual-timer' method='GET' style='display:flex;gap:6px;margin-top:10px;'>";
        html += "<input type='number' name='min' class='form-input' min='1' max='" + String(maxPumpTime/60000UL) + "' placeholder='دقائق مخصصة' required>";
        html += "<button type='submit' class='btn btn-accent' style='white-space:nowrap;padding:8px 14px;'>تفعيل</button>";
        html += "</form>";
      }
      html += "</div>";
    }
    html += "<a href='/reset' class='btn btn-outline btn-full' style='margin-top:10px;'>تصفير حالة النظام</a>";
  }
  html += "</div></div>";

  // ---------------------------------------------------------
  // تبويب سجل العمليات (Activity Log Tab)
  // ---------------------------------------------------------
  html += "<div id='logTab' class='tab-content'>";
  html += "<div class='section-card'>";
  html += "<div class='section-hdr'><div class='section-title'>سجل الأحداث الأخيرة</div></div>";
  html += "<div class='log-list' id='logList'>";
  for (int i = 0; i < 10; i++) {
    int idx = (logIndex-1-i+10)%10;
    if (operationLog[idx] != "") {
      html += "<div class='log-item'><span>" + operationLog[idx] + "</span><span style='font-size:10px;color:var(--txt-muted);'>أحدث</span></div>";
    }
  }
  html += "</div></div></div>";

  // ---------------------------------------------------------
  // تبويب إعدادات الواي فاي (Wi-Fi Settings Tab)
  // ---------------------------------------------------------
  html += "<div id='wifiTab' class='tab-content'>";
  html += "<div class='section-card'>";
  html += "<div class='section-hdr'><div class='section-title'>إعدادات شبكة الواي فاي</div></div>";
  
  if (WiFi.status() == WL_CONNECTED) {
    html += "<div style='font-size:12px;color:var(--green);font-weight:700;background:rgba(16,185,129,0.1);padding:10px;border-radius:8px;margin-bottom:12px;'>متصل حالياً بـ: " + wifiSSID + " (" + WiFi.localIP().toString() + ")</div>";
  } else {
    html += "<div style='font-size:12px;color:var(--amber);font-weight:700;background:rgba(245,158,11,0.1);padding:10px;border-radius:8px;margin-bottom:12px;'>يعمل بنقطة البث المباشر (SmartPump-Setup)</div>";
  }

  html += "<form action='/set-wifi' method='GET'>";
  html += "<div class='form-group'><label class='form-label'>اسم شبكة الواي فاي (SSID)</label>";
  html += "<input type='text' name='ssid' class='form-input' value='" + wifiSSID + "' placeholder='اسم الشبكة' required></div>";
  html += "<div class='form-group'><label class='form-label'>كلمة المرور</label>";
  html += "<input type='password' name='password' class='form-input' placeholder='كلمة المرور'></div>";
  html += "<button type='submit' class='btn btn-accent btn-full'>حفظ والاتصال بالشبكة</button>";
  html += "</form>";

  if (wifiSSID.length() > 0) {
    html += "<a href='/forget-wifi' class='btn btn-outline btn-full' style='margin-top:8px;color:var(--red);border-color:rgba(244,63,94,0.3);'>مسح الشبكة المحفوظة</a>";
  }
  html += "</div></div>";

  // ---------------------------------------------------------
  // تبويب إعدادات الحماية والهدوء (Settings Tab)
  // ---------------------------------------------------------
  html += "<div id='settingsTab' class='tab-content'>";
  html += "<form action='/set-timeout' method='GET'>";
  
  html += "<div class='section-card' style='margin-bottom:12px;'>";
  html += "<div class='section-hdr'><div class='section-title'>إعدادات حماية المضخة</div></div>";
  html += "<div class='form-group'><label class='form-label'>وقت أمان الطوارئ الأقصى (دقائق)</label>";
  html += "<input type='number' name='minutes' class='form-input' min='1' max='300' value='" + String(maxPumpTime/60000UL) + "' required></div>";
  html += "<div class='form-group'><label class='form-label'>فترة مهلة كشف رفع الماء (ثواني)</label>";
  html += "<input type='number' name='liftSec' class='form-input' min='1' max='300' value='" + String(liftTimeout/1000UL) + "' required></div>";
  html += "</div>";

  html += "<div class='section-card'>";
  html += "<div class='section-hdr'><div class='section-title'>وقت الهدوء المحظور</div></div>";
  html += "<div class='form-group' style='display:flex;align-items:center;justify-content:space-between;'>";
  html += "<span class='form-label' style='margin:0;'>تفعيل وقت الهدوء المحظور</span>";
  html += "<input type='checkbox' name='quietEnabled' value='1' style='width:20px;height:20px;accent-color:var(--accent);' " + String(quietModeEnabled ? "checked" : "") + ">";
  html += "</div>";
  html += "<div style='display:grid;grid-template-columns:1fr 1fr;gap:10px;margin-top:10px;'>";
  html += "<div class='form-group'><label class='form-label'>ساعة البدء (0-23)</label>";
  html += "<input type='number' name='quietStart' class='form-input' min='0' max='23' value='" + String(quietStartHour) + "'></div>";
  html += "<div class='form-group'><label class='form-label'>ساعة الانتهاء (0-23)</label>";
  html += "<input type='number' name='quietEnd' class='form-input' min='0' max='23' value='" + String(quietEndHour) + "'></div>";
  html += "</div>";
  html += "<button type='submit' class='btn btn-accent btn-full' style='margin-top:8px;'>حفظ جميع الإعدادات</button>";
  html += "</div>";

  html += "</form></div>";

  html += "</div>"; // app-container

  // جافا سكريبت التفاعلي للتحديث التلقائي والتبويبات
  html += "<script>";
  html += "function openTab(e, tabId) {";
  html += "  document.querySelectorAll('.tab-content').forEach(function(p){ p.classList.remove('active'); });";
  html += "  document.querySelectorAll('.tab-link').forEach(function(b){ b.classList.remove('active'); });";
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
  html += "  }).catch(function(err){});";
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
    pumpRow += "<div class='metric-tile'><div class='metric-lbl'>الدينمو</div><div class='metric-val on'>يعمل ⚡</div></div>";
    pumpRow += "<div class='metric-tile'><div class='metric-lbl'>مدة التشغيل</div><div class='metric-val timer'>" + String(elapsed / 60) + ":" + elS + "</div></div>";
    
    if (manualTimerActive) {
      long remaining = ((long)manualTimerDuration - (long)(millis() - pumpStartTime)) / 1000;
      if (remaining < 0) remaining = 0;
      String remS = (remaining % 60 < 10 ? "0" : "") + String(remaining % 60);
      pumpRow += "<div class='metric-tile'><div class='metric-lbl'>المتبقي</div><div class='metric-val timer' id='statusTimer'>" + String(remaining / 60) + ":" + remS + "</div></div>";
    } else {
      pumpRow += "<div class='metric-tile'><div class='metric-lbl'>المؤقت</div><div class='metric-val off'>مفتوح</div></div>";
    }
  } else {
    pumpRow += "<div class='metric-tile'><div class='metric-lbl'>الدينمو</div><div class='metric-val off'>متوقف</div></div>";
    pumpRow += "<div class='metric-tile'><div class='metric-lbl'>مدة التشغيل</div><div class='metric-val off'>--:--</div></div>";
    pumpRow += "<div class='metric-tile'><div class='metric-lbl'>المؤقت</div><div class='metric-val off'>غير مفعّل</div></div>";
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
      logs += "<div class='log-item'><span>" + operationLog[idx] + "</span><span style='font-size:10px;color:var(--txt-muted);'>أحدث</span></div>";
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
