#include <ESP8266WiFi.h>
#include <ESP8266WebServer.h>
#include <time.h>
#include <EEPROM.h>

// Include SecureOTA header file
#include "SecureOTA.h"

// --- Wi-Fi Settings (AP Mode + Router STA Connection) ---
String wifiSSID = "";        // Saved home Wi-Fi SSID
String wifiPassword = "";    // Saved home Wi-Fi password

const char* apSSID = "SmartPump-Setup"; // Access Point SSID for controller
const char* apPassword = "";            // Access Point Password (open for easy setup)

// Web server object on port 80
ESP8266WebServer server(80);

// --- Pin Definitions ---
#ifndef LED_BUILTIN
#define LED_BUILTIN 2 // GPIO2 / D4 default on ESP8266
#endif
const int ledPin = LED_BUILTIN; // Built-in LED pin (Heartbeat indicator)
const int relayPin = 5;       // D1: Controls the pump relay
const int highSensorPin = 4;  // D2: High level sensor wire (stop point)
const int lowSensorPin = 14;  // D5: Low level sensor wire (start point)
const int warningSensorPin = 12; // D6: Warning sensor wire (early warning)
const int powerPin = 0;       // D3: Sensor power supply wire at bottom
const int liftSensorPin = 13; // D7: Water lift detection sensor wire (at pipe outlet)

// --- System State Variables ---
bool isPumping = false; 
bool systemError = false; 
bool manualMode = false;
String currentStatus = "Initializing...";
String operationLog[10]; // Operation log history (last 10 events)
int logIndex = 0;

// --- Quiet Hours Variables ---
bool quietModeEnabled = false;   // Enable quiet hours
int quietStartHour = 22;         // Start hour (0-23) - default 10 PM
int quietEndHour = 6;            // End hour (0-23) - default 6 AM

// --- Timing Variables (Non-blocking millis) ---
unsigned long pumpStartTime = 0; 
unsigned long maxPumpTime = 60000UL; // Emergency timeout (ms) - web configurable (default 1 min)
const unsigned long maxAllowedTime = 300000UL; // 300 minutes max limit (protection safeguard)
unsigned long liftTimeout = 15000UL; // Water lift timeout (ms) - web configurable (default 15 sec)
unsigned long lastSensorRead = 0;            // Last sensor reading timestamp
unsigned long lastLedBlink = 0;              // Built-in LED heartbeat timestamp
const unsigned long ledBlinkInterval = 100;  // Fast heartbeat blink interval (100 ms)
bool ledState = false;                       // Built-in LED state

// --- Manual Mode Timer ---
bool   manualTimerActive  = false;   // Is manual timer active?
unsigned long manualTimerDuration = 0; // Manual timer duration in milliseconds

// Function to append event to operation log
void addLog(String message) {
  operationLog[logIndex] = message;
  logIndex = (logIndex + 1) % 10;
}

// --- EEPROM String Helper Functions ---
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

// Save settings to non-volatile EEPROM
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
  Serial.println("Settings and Wi-Fi saved to EEPROM.");
}

// Load settings from non-volatile EEPROM
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
    Serial.println("Settings loaded successfully from EEPROM.");
    
    if (systemError) {
      currentStatus = "Emergency: Restored lock state for safety (power outage during error)";
      addLog("Emergency: Lock Restored");
    }
  } else {
    Serial.println("No saved settings found. Saving default values.");
    saveSettings(); // Save default values for first boot
  }
}

// Check if current time falls within prohibited quiet hours
bool isQuietHours() {
  if (!quietModeEnabled) return false;
  
  time_t now = time(nullptr);
  if (now < 1000000000ULL) {
    return false; 
  }
  
  struct tm* timeinfo = localtime(&now);
  int currentHour = timeinfo->tm_hour;
  
  if (quietStartHour == quietEndHour) {
    return false; // Disabled
  }
  
  if (quietStartHour < quietEndHour) {
    return (currentHour >= quietStartHour && currentHour < quietEndHour);
  } else {
    return (currentHour >= quietStartHour || currentHour < quietEndHour);
  }
}

// ---------------------------------------------------------
// HTML Page Handler (Web UI)
// ---------------------------------------------------------
void handleRoot() {
  // System time
  String sysTime = "--:--:--";
  time_t nowT = time(nullptr);
  if (nowT > 1000000000ULL) {
    struct tm* ti = localtime(&nowT);
    char buf[20];
    sprintf(buf, "%02d:%02d:%02d", ti->tm_hour, ti->tm_min, ti->tm_sec);
    sysTime = String(buf);
  }

  String html = "<!DOCTYPE html><html lang='en' dir='ltr'>";
  html += "<head><meta charset='UTF-8'><meta name='viewport' content='width=device-width,initial-scale=1,user-scalable=no'>";
  html += "<title>SmartPump Controller</title><style>";

  // Color system
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

  // Page header
  html += ".hdr{display:flex;align-items:center;justify-content:space-between;padding:8px 2px 2px;}";
  html += ".hdr-title{font-size:17px;font-weight:800;color:var(--g800);}";
  html += ".hdr-time{font-size:13px;font-weight:600;color:var(--g400);font-variant-numeric:tabular-nums;}";

  // Mode bar & switch
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
  html += ".switch-box.auto .switch-thumb{background:var(--green);transform:translateX(20px);}";
  html += ".switch-box.manual .switch-thumb{background:var(--blue);transform:translateX(0);}";
  html += ".switch-box.err .switch-thumb{background:var(--red);transform:translateX(0);}";
  html += ".switch-box.on .switch-thumb{background:var(--green);transform:translateX(20px);}";
  html += ".switch-box.off .switch-thumb{background:white;transform:translateX(0);}";
  html += ".pump-ctrl-box{display:flex;align-items:center;justify-content:space-between;background:var(--g50);border:1.5px solid var(--g200);border-radius:var(--rs);padding:10px 12px;}";

  // Status card
  html += ".s-card{background:white;border-radius:var(--r);padding:16px;box-shadow:var(--sh);border-left:4px solid var(--g200);}";
  if (systemError)    html += ".s-card{border-left-color:var(--red);background:var(--red-lt);}";
  else if (isPumping) html += ".s-card{border-left-color:var(--blue);background:var(--blue-lt);}";
  else                html += ".s-card{border-left-color:var(--green);background:var(--green-lt);}";
  html += ".s-text{font-size:15px;font-weight:700;margin-bottom:10px;}";
  if (systemError)    html += ".s-text{color:var(--red);}";
  else if (isPumping) html += ".s-text{color:var(--blue);}";
  else                html += ".s-text{color:var(--green);}";
  html += ".s-meta{display:flex;border-top:1px solid var(--g200);padding-top:10px;}";
  html += ".s-item{flex:1;text-align:center;}";
  html += ".s-item+.s-item{border-left:1px solid var(--g200);}";
  html += ".s-lbl{font-size:10px;color:var(--g400);font-weight:600;margin-bottom:3px;letter-spacing:.3px;}";
  html += ".s-val{font-size:14px;font-weight:700;color:var(--g800);}";
  html += ".s-val.on{color:var(--green);}.s-val.off{color:var(--g400);}.s-val.tk{color:var(--blue);font-variant-numeric:tabular-nums;}";

  // Control card
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

  // Timer
  html += ".tmr{background:var(--blue-lt);border:1.5px solid #c1d4f7;border-radius:var(--rs);padding:12px;}";
  html += ".tmr-title{font-size:11px;font-weight:800;color:var(--blue);margin-bottom:8px;}";
  html += ".tmr-big{font-size:30px;font-weight:900;color:var(--blue);text-align:center;letter-spacing:3px;font-variant-numeric:tabular-nums;margin:4px 0 8px;}";
  html += ".tmr-row{display:flex;gap:8px;align-items:center;}";
  html += ".tmr-inp{flex:1;padding:9px;border:1.5px solid #c1d4f7;border-radius:var(--rs);font-size:14px;text-align:center;background:white;outline:none;}";
  html += ".tmr-inp:focus{border-color:var(--blue);}";
  html += ".tmr-hint{font-size:10px;color:var(--g400);text-align:center;margin-top:5px;}";

  // Tabs
  html += ".tabs{background:white;border-radius:var(--r);padding:12px;box-shadow:var(--sh);}";
  html += ".tabs-nav{display:flex;background:var(--g100);padding:3px;border-radius:var(--rs);margin-bottom:12px;}";
  html += ".tab-btn{flex:1;padding:7px;text-align:center;font-size:13px;font-weight:700;border-radius:7px;cursor:pointer;color:var(--g400);border:none;background:transparent;transition:.2s;}";
  html += ".tab-btn.active{background:white;color:var(--g800);box-shadow:0 1px 4px rgba(0,0,0,.06);}";
  html += ".tab-pane{display:none;}.tab-pane.active{display:block;}";

  // Log list
  html += ".log-list{display:flex;flex-direction:column;gap:4px;max-height:150px;overflow-y:auto;}";
  html += ".log-item{font-size:12px;color:var(--g600);padding:6px 10px;background:var(--g50);border-radius:6px;border-left:3px solid var(--g200);}";

  // Settings
  html += ".set-section{margin-bottom:14px;}";
  html += ".set-title{font-size:10px;font-weight:800;color:var(--blue);text-transform:uppercase;letter-spacing:.5px;";
  html += "padding-bottom:5px;border-bottom:1.5px solid var(--blue-lt);margin-bottom:9px;}";
  html += ".set-row{display:flex;align-items:center;justify-content:space-between;padding:5px 0;}";
  html += ".set-lbl{font-size:13px;color:var(--g600);font-weight:600;}";
  html += ".set-inp{width:80px;padding:7px;border:1.5px solid var(--g200);border-radius:var(--rs);font-size:13px;text-align:center;outline:none;}";
  html += ".set-inp:focus{border-color:var(--blue);}";
  html += ".set-hint{font-size:10px;color:var(--g400);text-align:center;margin-top:6px;}";

  html += "</style></head><body><div class='page'>";

  // Header
  html += "<div class='hdr'><div class='hdr-title'>SmartPump Controller</div>";
  html += "<div class='hdr-time' id='sysTimeLabel'>" + sysTime + "</div></div>";

  // Mode bar with switch
  html += "<div class='mode-bar'>";
  if (systemError) {
    html += "<div class='mode-info'><div class='mdot err'></div><div class='mlabel'>System Status</div></div>";
    html += "<div class='switch-link'><span class='switch-lbl err'>Emergency (Locked)</span><div class='switch-box err'><div class='switch-thumb'></div></div></div>";
  } else if (manualMode) {
    html += "<div class='mode-info'><div class='mdot manual'></div><div class='mlabel'>Operating Mode</div></div>";
    html += "<a href='/toggle-mode' class='switch-link' title='Click to switch to Auto'>";
    html += "<span class='switch-lbl manual'>Manual</span>";
    html += "<div class='switch-box manual'><div class='switch-thumb'></div></div></a>";
  } else {
    html += "<div class='mode-info'><div class='mdot auto'></div><div class='mlabel'>Operating Mode</div></div>";
    html += "<a href='/toggle-mode' class='switch-link' title='Click to switch to Manual'>";
    html += "<span class='switch-lbl auto'>Auto</span>";
    html += "<div class='switch-box auto'><div class='switch-thumb'></div></div></a>";
  }
  html += "</div>";

  // Status card
  html += "<div class='s-card'><div class='s-text' id='statusText'>" + currentStatus + "</div>";
  html += "<div class='s-meta' id='pumpRow'>";
  if (isPumping && !systemError) {
    long el = (millis() - pumpStartTime) / 1000;
    String elS = (el % 60 < 10 ? "0" : "") + String(el % 60);
    html += "<div class='s-item'><div class='s-lbl'>Pump</div><div class='s-val on'>RUNNING</div></div>";
    html += "<div class='s-item'><div class='s-lbl'>Runtime</div><div class='s-val tk'>" + String(el/60) + ":" + elS + "</div></div>";
    if (manualTimerActive) {
      long rem = ((long)manualTimerDuration-(long)(millis()-pumpStartTime))/1000;
      if(rem<0)rem=0;
      String remS=(rem%60<10?"0":"")+String(rem%60);
      html += "<div class='s-item'><div class='s-lbl'>Remaining</div><div class='s-val tk' id='statusTimer'>" + String(rem/60) + ":" + remS + "</div></div>";
    }
  } else {
    html += "<div class='s-item'><div class='s-lbl'>Pump</div><div class='s-val off'>STOPPED</div></div>";
  }
  html += "</div></div>";

  // Control card
  html += "<div class='c-card'>";
  if (systemError) {
    html += "<div class='sec-lbl'>Action Required</div>";
    html += "<a href='/reset' class='btn red full'>Reset System</a>";
  } else {
    html += "<div class='sec-lbl'>Control</div>";
    if (manualMode) {
      html += "<div class='pump-ctrl-box'>";
      html += "<span style='font-size:13px;color:var(--g800);font-weight:700;'>Pump Switch</span>";
      if (isPumping) {
        html += "<a href='/manual-off' class='switch-link' title='Click to turn off pump'>";
        html += "<span class='switch-lbl on'>RUNNING</span>";
        html += "<div class='switch-box on'><div class='switch-thumb'></div></div></a>";
      } else {
        html += "<a href='/manual-on' class='switch-link' title='Click to turn on pump'>";
        html += "<span class='switch-lbl off'>STOPPED</span>";
        html += "<div class='switch-box off'><div class='switch-thumb'></div></div></a>";
      }
      html += "</div>";
    }
    html += "<a href='/reset' class='btn ghost full'>Reset System</a>";
    if (manualMode) {
      html += "<div class='tmr'><div class='tmr-title'>Auto Shutdown Timer</div>";
      if (manualTimerActive && isPumping) {
        long rem=((long)manualTimerDuration-(long)(millis()-pumpStartTime))/1000;
        if(rem<0)rem=0;
        String remS=(rem%60<10?"0":"")+String(rem%60);
        html += "<div class='tmr-big' id='timerBig'>" + String(rem/60) + ":" + remS + "</div>";
        html += "<a href='/cancel-timer' class='btn amber'>Cancel Timer</a>";
      } else {
        html += "<form action='/set-manual-timer' method='GET' style='margin:0'>";
        html += "<div class='tmr-row'>";
        html += "<input type='number' name='min' class='tmr-inp' min='1' max='" + String(maxPumpTime/60000UL) + "' placeholder='Minutes' required>";
        html += "<button type='submit' class='btn blue' style='padding:9px 14px'>Enable</button></div></form>";
        html += "<div class='tmr-hint'>Enter duration in minutes (1 - " + String(maxPumpTime/60000UL) + ")</div>";
      }
      html += "</div>";
    }
  }
  html += "</div>";

  // Tabs
  html += "<div class='tabs'>";
  html += "<div class='tabs-nav'>";
  html += "<button class='tab-btn active' onclick=\"openTab(event,'logPane')\">Operation Log</button>";
  html += "<button class='tab-btn' onclick=\"openTab(event,'setPane')\">Settings</button>";
  html += "</div>";

  // Log list
  html += "<div id='logPane' class='tab-pane active'><div class='log-list' id='logList'>";
  for (int i = 0; i < 10; i++) {
    int idx = (logIndex-1-i+10)%10;
    if (operationLog[idx] != "") html += "<div class='log-item'>" + operationLog[idx] + "</div>";
  }
  html += "</div></div>";

  // Settings
  html += "<div id='setPane' class='tab-pane'>";

  // Wi-Fi Setup Section
  html += "<div class='set-section'>";
  html += "<div class='set-title'>Wi-Fi Configuration</div>";
  if (WiFi.status() == WL_CONNECTED) {
    html += "<div style='font-size:12px;color:var(--green);font-weight:700;margin-bottom:8px;'>Currently Connected: " + wifiSSID + " (" + WiFi.localIP().toString() + ")</div>";
  } else {
    html += "<div style='font-size:12px;color:var(--amber);font-weight:700;margin-bottom:8px;'>Running in Direct AP Mode (SmartPump-Setup)</div>";
  }
  html += "<form action='/set-wifi' method='GET'>";
  html += "<div class='set-row'><span class='set-lbl'>Network Name (SSID)</span>";
  html += "<input type='text' name='ssid' class='set-inp' style='width:140px;text-align:left' value='" + wifiSSID + "' placeholder='SSID' required></div>";
  html += "<div class='set-row'><span class='set-lbl'>Password</span>";
  html += "<input type='password' name='password' class='set-inp' style='width:140px;text-align:left' placeholder='Password'></div>";
  html += "<button type='submit' class='btn blue full' style='margin-top:6px'>Save & Connect</button>";
  html += "</form>";
  if (wifiSSID.length() > 0) {
    html += "<a href='/forget-wifi' class='btn ghost full' style='margin-top:6px;color:var(--red)'>Clear Saved Network</a>";
  }
  html += "</div>";

  // Pump Protection & Quiet Hours Settings
  html += "<form action='/set-timeout' method='GET'>";
  html += "<div class='set-section'>";
  html += "<div class='set-title'>Pump Protection</div>";
  html += "<div class='set-row'><span class='set-lbl'>Emergency Timeout (min)</span>";
  html += "<input type='number' name='minutes' class='set-inp' min='1' max='300' value='" + String(maxPumpTime/60000UL) + "' required></div>";
  html += "<div class='set-row'><span class='set-lbl'>Water Lift Timeout (sec)</span>";
  html += "<input type='number' name='liftSec' class='set-inp' min='1' max='300' value='" + String(liftTimeout/1000UL) + "' required></div>";
  html += "</div>";

  html += "<div class='set-section'>";
  html += "<div class='set-title'>Quiet Hours</div>";
  html += "<div class='set-row'><span class='set-lbl'>Enable Quiet Hours</span>";
  html += "<input type='checkbox' name='quietEnabled' value='1' style='width:18px;height:18px;cursor:pointer;accent-color:var(--blue)' " + String(quietModeEnabled ? "checked" : "") + "></div>";
  html += "<div class='set-row'><span class='set-lbl'>Start Hour (0-23)</span>";
  html += "<input type='number' name='quietStart' class='set-inp' min='0' max='23' value='" + String(quietStartHour) + "'></div>";
  html += "<div class='set-row'><span class='set-lbl'>End Hour (0-23)</span>";
  html += "<input type='number' name='quietEnd' class='set-inp' min='0' max='23' value='" + String(quietEndHour) + "'></div>";
  html += "</div>";

  html += "<button type='submit' class='btn blue full' style='margin-top:4px'>Save System Settings</button>";
  html += "<div class='set-hint'>Emergency: 1-300 min &nbsp;|&nbsp; Lift: 1-300 sec &nbsp;|&nbsp; Quiet: 0-23</div>";
  html += "</form></div>";

  html += "</div></div>"; // tabs + page

  // JavaScript for tab switching and real-time status updates
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
    pumpRow += "<div class='s-item'><div class='s-lbl'>Pump</div><div class='s-val on'>RUNNING</div></div>";
    pumpRow += "<div class='s-item'><div class='s-lbl'>Runtime</div><div class='s-val tk'>" + String(elapsed / 60) + ":" + elS + "</div></div>";
    
    if (manualTimerActive) {
      long remaining = ((long)manualTimerDuration - (long)(millis() - pumpStartTime)) / 1000;
      if (remaining < 0) remaining = 0;
      String remS = (remaining % 60 < 10 ? "0" : "") + String(remaining % 60);
      pumpRow += "<div class='s-item'><div class='s-lbl'>Remaining</div><div class='s-val tk' id='statusTimer'>" + String(remaining / 60) + ":" + remS + "</div></div>";
    }
  } else {
    pumpRow += "<div class='s-item'><div class='s-lbl'>Pump</div><div class='s-val off'>STOPPED</div></div>";
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

void handleReset() {
  systemError = false;
  isPumping = false;
  manualMode = false;
  digitalWrite(relayPin, HIGH);
  currentStatus = "System reset completed";
  addLog("System Reset");
  saveSettings();
  
  server.sendHeader("Location", "/", true);
  server.send(303);
}

// Toggle mode handler (Manual / Auto)
void handleToggleMode() {
  if (!systemError) {
    manualMode = !manualMode;
    currentStatus = manualMode ? "Manual Mode" : "Auto Mode";
    addLog(manualMode ? "Switched to Manual Mode" : "Switched to Auto Mode");
    saveSettings();
  }
  server.sendHeader("Location", "/", true);
  server.send(303);
}

// Manual turn on handler
void handleManualOn() {
  if (!systemError) {
    if (!manualMode) {
      manualMode = true;
      addLog("Switched to Manual Mode");
    }
    digitalWrite(relayPin, LOW);
    isPumping = true;
    pumpStartTime = millis();
    manualTimerActive = false;
    manualTimerDuration = 0;
    currentStatus = "Pumping manually...";
    addLog("Manual pumping started");
    saveSettings();
  }
  server.sendHeader("Location", "/", true);
  server.send(303);
}

// Manual shutdown timer handler
void handleSetManualTimer() {
  if (!systemError && server.hasArg("min")) {
    int requestedMin = server.arg("min").toInt();
    int maxMin = (int)(maxPumpTime / 60000UL);
    if (requestedMin < 1) requestedMin = 1;
    if (requestedMin > maxMin) requestedMin = maxMin;
    manualTimerDuration = (unsigned long)requestedMin * 60000UL;
    manualTimerActive = true;
    if (!isPumping) {
      if (!manualMode) { manualMode = true; addLog("Switched to Manual Mode"); }
      digitalWrite(relayPin, LOW);
      isPumping = true;
      pumpStartTime = millis();
    }
    currentStatus = "Manual pump - Stopping in " + String(requestedMin) + " min";
    addLog("Manual timer set: " + String(requestedMin) + " min");
    Serial.println("Manual timer set: " + String(requestedMin) + " min");
    saveSettings();
  }
  server.sendHeader("Location", "/", true);
  server.send(303);
}

// Cancel manual timer handler
void handleCancelTimer() {
  manualTimerActive = false;
  manualTimerDuration = 0;
  currentStatus = "Pumping manually (Timer canceled)";
  addLog("Manual timer canceled");
  server.sendHeader("Location", "/", true);
  server.send(303);
}

// Manual turn off handler
void handleManualOff() {
  if (!systemError) {
    digitalWrite(relayPin, HIGH);
    isPumping = false;
    manualTimerActive = false;
    manualTimerDuration = 0;
    currentStatus = "Pump stopped";
    addLog("Manual pump stop");
  }
  server.sendHeader("Location", "/", true);
  server.send(303);
}

// Update Wi-Fi settings handler
void handleSetWiFi() {
  if (server.hasArg("ssid")) {
    wifiSSID = server.arg("ssid");
    wifiSSID.trim();
    wifiPassword = server.arg("password");
    saveSettings();
    addLog("Saved Wi-Fi: " + wifiSSID);
    currentStatus = "Connecting to " + wifiSSID + "...";
    
    WiFi.disconnect();
    WiFi.begin(wifiSSID.c_str(), wifiPassword.c_str());
  }
  server.sendHeader("Location", "/", true);
  server.send(303);
}

// Clear saved Wi-Fi network handler
void handleForgetWiFi() {
  wifiSSID = "";
  wifiPassword = "";
  saveSettings();
  WiFi.disconnect();
  currentStatus = "Saved Wi-Fi network cleared";
  addLog("Cleared Wi-Fi network");
  server.sendHeader("Location", "/", true);
  server.send(303);
}

// Update timeout & quiet hours handler
void handleSetTimeout() {
  bool updated = false;
  String statusMsg = "";

  if (server.hasArg("minutes")) {
    int minutes = server.arg("minutes").toInt();
    if (minutes >= 1 && minutes <= 300) {
      maxPumpTime = (unsigned long)minutes * 60000UL;
      statusMsg += "Emergency timeout updated to " + String(minutes) + " min. ";
      addLog("Updated emergency timeout: " + String(minutes) + " min");
      updated = true;
    }
  }

  if (server.hasArg("liftSec")) {
    int seconds = server.arg("liftSec").toInt();
    if (seconds >= 1 && seconds <= 300) {
      liftTimeout = (unsigned long)seconds * 1000UL;
      statusMsg += "Water lift timeout updated to " + String(seconds) + " sec. ";
      addLog("Updated water lift timeout: " + String(seconds) + " sec");
      updated = true;
    }
  }

  if (server.hasArg("minutes")) {
    bool newEnabled = server.hasArg("quietEnabled");
    if (newEnabled != quietModeEnabled) {
      quietModeEnabled = newEnabled;
      updated = true;
      addLog(quietModeEnabled ? "Quiet hours enabled" : "Quiet hours disabled");
    }
  }

  if (server.hasArg("quietStart")) {
    int startH = server.arg("quietStart").toInt();
    if (startH >= 0 && startH <= 23 && startH != quietStartHour) {
      quietStartHour = startH;
      updated = true;
      addLog("Quiet hours start: " + String(startH) + ":00");
    }
  }

  if (server.hasArg("quietEnd")) {
    int endH = server.arg("quietEnd").toInt();
    if (endH >= 0 && endH <= 23 && endH != quietEndHour) {
      quietEndHour = endH;
      updated = true;
      addLog("Quiet hours end: " + String(endH) + ":00");
    }
  }

  if (updated) {
    if (statusMsg == "") {
      statusMsg = "Settings saved successfully";
    }
    currentStatus = statusMsg;
    saveSettings();
  } else {
    currentStatus = "Error updating settings";
  }

  server.sendHeader("Location", "/", true);
  server.send(303);
}

// ---------------------------------------------------------
void setup() {
  Serial.begin(115200);

  // Load settings from EEPROM
  loadSettings();

  // Configure pin modes
  pinMode(ledPin, OUTPUT);
  pinMode(relayPin, OUTPUT);
  pinMode(powerPin, OUTPUT);
  pinMode(highSensorPin, INPUT_PULLUP);
  pinMode(lowSensorPin, INPUT_PULLUP);
  pinMode(warningSensorPin, INPUT_PULLUP);
  pinMode(liftSensorPin, INPUT_PULLUP);

  digitalWrite(ledPin, HIGH); // Off initially (active LOW on ESP8266)
  digitalWrite(powerPin, HIGH); 
  digitalWrite(relayPin, HIGH);

  // Enable dual Wi-Fi mode (AP + STA)
  WiFi.mode(WIFI_AP_STA);
  
  // Start controller Access Point
  WiFi.softAP(apSSID, apPassword);
  Serial.print("Controller Access Point started (AP): ");
  Serial.println(apSSID);
  Serial.print("AP IP Address: ");
  Serial.println(WiFi.softAPIP());

  // Try connecting to saved home Wi-Fi network if available
  if (wifiSSID.length() > 0) {
    Serial.print("Attempting connection to network: ");
    Serial.println(wifiSSID);
    WiFi.begin(wifiSSID.c_str(), wifiPassword.c_str());

    unsigned long startAttempt = millis();
    while (WiFi.status() != WL_CONNECTED && millis() - startAttempt < 10000) {
      delay(500);
      Serial.print(".");
    }

    if (WiFi.status() == WL_CONNECTED) {
      Serial.println("\nConnected to Wi-Fi successfully!");
      Serial.print("IP Address (Station): ");
      Serial.println(WiFi.localIP());
      addLog("Wi-Fi connected: " + wifiSSID);

      // Start NTP time sync now that WiFi is connected
      configTime(3 * 3600, 0, "pool.ntp.org", "time.nist.gov");

      // Check for OTA firmware update after Wi-Fi is connected
      checkAndApplyOTA();
    } else {
      Serial.println("\nCould not connect to saved network. Operating in AP mode only.");
      addLog("Wi-Fi connection failed");
    }
  } else {
    Serial.println("No saved Wi-Fi network detected. Connect to SmartPump-Setup to configure.");
    addLog("AP Mode started");
  }

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
  Serial.println("Web server started successfully...");
  addLog("System Started");
}

// ---------------------------------------------------------
void loop() {
  // 0. Non-blocking built-in LED heartbeat blink (indicates ESP is running)
  if (millis() - lastLedBlink >= ledBlinkInterval) {
    lastLedBlink = millis();
    ledState = !ledState;
    digitalWrite(ledPin, ledState ? LOW : HIGH); // ESP8266 built-in LED is active LOW
  }

  // 1. Handle HTTP client requests
  server.handleClient();

  // 2. Read sensors every 2 seconds
  if (millis() - lastSensorRead >= 2000) {
    lastSensorRead = millis();

    // If system error exists, do nothing to preserve lock state
    if (systemError) {
      return;
    }

    // Check automatic pumping pause during quiet hours
    if (isPumping && !manualMode && isQuietHours()) {
      digitalWrite(relayPin, HIGH);
      isPumping = false;
      currentStatus = "Pumping paused (Quiet Hours)";
      addLog("Paused: Quiet Hours");
      Serial.println("Pump stopped due to Quiet Hours");
      return;
    }

    // Read sensors
    digitalWrite(powerPin, LOW);
    delay(10);
    int highWater = digitalRead(highSensorPin);
    int lowWater = digitalRead(lowSensorPin);
    int warningWater = digitalRead(warningSensorPin);
    int liftWater = digitalRead(liftSensorPin);
    digitalWrite(powerPin, HIGH);

    // Dry-run / Water lift failure protection (Auto mode only)
    if (isPumping && !manualMode && (millis() - pumpStartTime >= liftTimeout)) {
      if (liftWater == HIGH) {
        digitalWrite(relayPin, HIGH);
        isPumping = false;
        manualTimerActive = false;
        manualMode = false;
        systemError = true;
        currentStatus = "Error: Water lift failure - Locked";
        addLog("Error: Water lift failure");
        Serial.println("Error: Water lift failure");
        saveSettings();
        return;
      }
    }

    // Pump control logic
    if (highWater == LOW) {
      // Tank is full
      if (isPumping) {
        digitalWrite(relayPin, HIGH);
        isPumping = false;
        manualTimerActive = false;
        manualTimerDuration = 0;
        currentStatus = manualMode ? "Tank full - Pump stopped" : "Tank Full";
        addLog("Pump stopped - Tank full");
        Serial.println("Pump stopped - Tank full");
      } else {
        currentStatus = "Tank Full";
      }
    }
    else if (lowWater == HIGH) {
      // Critical low level
      if (!isPumping && !manualMode) {
        if (isQuietHours()) {
          currentStatus = "Critical level - Deferred (Quiet Hours)";
        } else {
          digitalWrite(relayPin, LOW);
          isPumping = true;
          pumpStartTime = millis();
          currentStatus = "Pumping automatically...";
          addLog("Auto pumping started");
          Serial.println("Auto pumping started");
        }
      } else if (isPumping) {
        currentStatus = manualMode ? "Pumping manually..." : "Pumping automatically...";
      } else if (manualMode) {
        currentStatus = "Critical level - Turn on pump";
      }
    }
    else if (warningWater == HIGH) {
      if (isPumping) {
        currentStatus = manualMode ? "Pumping manually..." : "Pumping automatically...";
      } else {
        currentStatus = "Warning: Water level low";
      }
    } else {
      if (isPumping) {
        currentStatus = manualMode ? "Pumping manually..." : "Pumping automatically...";
      } else {
        currentStatus = "Water level normal";
      }
    }
  }

  // 3. Manual mode timer check
  if (isPumping && manualTimerActive && !systemError) {
    if (millis() - pumpStartTime >= manualTimerDuration) {
      digitalWrite(relayPin, HIGH);
      isPumping = false;
      manualTimerActive = false;
      manualTimerDuration = 0;
      currentStatus = "Stopped - Timer expired";
      addLog("Timer expired");
      Serial.println("Timer expired");
    }
  }

  // 4. Maximum safety runtime timer (Auto mode only)
  if (isPumping && !manualMode && !systemError) {
    if (millis() - pumpStartTime >= maxPumpTime) {
      digitalWrite(relayPin, HIGH);
      isPumping = false;
      manualTimerActive = false;
      manualMode = false;
      systemError = true;
      currentStatus = "Error: Max runtime exceeded - Locked";
      addLog("Error: Max runtime exceeded");
      Serial.println("Error: Max runtime exceeded");
      saveSettings();
    }
  }
  
  // 5. Wi-Fi reconnection attempt if disconnected
  if (wifiSSID.length() > 0 && WiFi.status() != WL_CONNECTED) {
    static unsigned long lastReconnectAttempt = 0;
    if (millis() - lastReconnectAttempt >= 30000) {
      lastReconnectAttempt = millis();
      Serial.println("Attempting Wi-Fi reconnection...");
      WiFi.begin(wifiSSID.c_str(), wifiPassword.c_str());
    }
  }
}
