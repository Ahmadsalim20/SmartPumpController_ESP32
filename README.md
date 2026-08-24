# 🚰 SmartPump Controller

An intelligent electromechanical system designed for controlling water pumps (dynamos) and monitoring water tank levels using an **ESP8266** microcontroller with a responsive, interactive web control interface.

---

## 🌟 Key Features

* **📶 WiFi Access Point & Network Config Manager:**
  * Automatically creates a dedicated Wi-Fi Access Point named **`SmartPump-Setup`** (open network, no password required) for direct connection via smartphone or computer at `http://192.168.4.1`.
  * Allows users to input home Wi-Fi network credentials directly via the web interface, saving them to persistent **EEPROM** memory.
  * Automatically connects to the configured home network upon reboot while remaining available for direct AP setup.

* **🤖 Automatic & Manual Modes:**
  * **Auto Mode:** Automatically starts the pump when water drops to a critical level and stops immediately once the tank is full.
  * **Manual Mode:** Provides full direct control to turn the pump on or off via the web interface.

* **🛡️ Advanced Safety & Protection System:**
  * **Dry-Run Protection (Water Lift Detection):** Verifies that water reaches the outlet pipe within a configured grace period (default: 15 seconds). If no water lift is detected, the system immediately shuts down and enters an emergency lock state to prevent pump burnout.
  * **Max Runtime Timeout:** Automatically turns off the pump and enters error state if automatic pumping exceeds the maximum allowed duration (configurable from 1 to 300 minutes).
  * **Power Interruption Protection:** Stores error lock states and Wi-Fi configurations in non-volatile **EEPROM** to prevent unsafe auto-restarts upon power recovery.

* **🌙 Quiet Hours:**
  * Allows setting prohibited pumping hours (e.g., 10:00 PM to 6:00 AM) to suppress automatic pumping during quiet periods, with automated time synchronization via internet NTP (GMT+3).

* **⏱️ Manual Shutdown Timer:**
  * Option to set a countdown timer in minutes for manual pumping, automatically shutting off the pump when expired.

* **📱 Responsive & Real-time Web UI:**
  * High-quality responsive design optimized for mobile phones, tablets, and desktop browsers.
  * Real-time status updates via AJAX without requiring page reloads.
  * System event log displaying the latest 10 operational events.
  * Flexible Settings tab to configure Wi-Fi credentials, emergency timeouts, quiet hours, and lift detection limits directly from the browser.

---

## 🔌 Pinouts & Hardware Connections

| ESP8266 Pin | Programmed GPIO | Function / Sensor | Description |
| :--- | :--- | :--- | :--- |
| **D1** | `GPIO 5` | Pump Relay (`relayPin`) | Controls pump activation / shutdown |
| **D2** | `GPIO 4` | High Level Sensor (`highSensorPin`) | Pumping stop point (Tank Full) |
| **D5** | `GPIO 14` | Low Level Sensor (`lowSensorPin`) | Pumping start point (Critical Low) |
| **D6** | `GPIO 12` | Warning Sensor (`warningSensorPin`) | Early notification before water depletes |
| **D7** | `GPIO 13` | Water Lift Sensor (`liftSensorPin`) | Positioned at pipe outlet to verify water flow |
| **D3** | `GPIO 0` | Sensor Power Supply (`powerPin`) | Pulsed power supply to sensors to prevent chemical electrolysis |
| **D4 / LED1** | `GPIO 2` | Built-in LED 1 (`ledPin1`) | System & Pump status indicator (Solid ON: Pumping, Rapid Blink: Error, Pulse: Heartbeat) |
| **D0 / LED2** | `GPIO 16` | Built-in LED 2 (`ledPin2`) | Wi-Fi Status indicator (Solid ON: Connected to Router, Slow Blink: AP Setup Mode) |

---

## 🌐 Web Routes & API Endpoints

| Endpoint | Method | Description |
| :--- | :--- | :--- |
| `/` | `GET` | Main Web Interface (HTML / CSS / JS) |
| `/status` | `GET` | Fetches system state, runtime timers, and recent event logs in `JSON` format |
| `/set-wifi` | `GET` | Saves Wi-Fi SSID and password to EEPROM (`?ssid=X&password=Y`) |
| `/forget-wifi` | `GET` | Clears saved Wi-Fi credentials from memory |
| `/toggle-mode` | `GET` | Toggles between Automatic and Manual modes |
| `/manual-on` | `GET` | Turns on the pump manually |
| `/manual-off` | `GET` | Turns off the pump manually |
| `/set-manual-timer` | `GET` | Enables a countdown manual timer (`?min=X`) |
| `/cancel-timer` | `GET` | Cancels any active manual shutdown timer |
| `/set-timeout` | `GET` | Updates emergency timeout, water lift timeout, and quiet hours |
| `/reset` | `GET` | Resets error locks and clears system alarm state |

---

## 🚀 Setup & WiFi Configuration

1. **Upload Code to ESP8266:**
   - Open [`SmartPumpController.ino`](file:///c:/ArduinoProjects/SmartPumpController/SmartPumpController.ino) in the Arduino IDE and click **Upload**.

2. **First-Time Connection (Direct AP Mode):**
   - Connect your phone or PC to the Wi-Fi network named **`SmartPump-Setup`** (no password).
   - Open a browser and navigate to: `http://192.168.4.1`

3. **Connect to Home Wi-Fi:**
   - Go to the **Settings** tab in the Web UI.
   - Enter your network SSID and Password, then click **"Save & Connect"**.
   - The controller will save credentials to EEPROM and automatically reconnect on future boots.

---

## 📜 License

This project is open-source and intended for smart water control and automation applications.
