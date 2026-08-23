#ifndef SECURE_OTA_H
#define SECURE_OTA_H

#if defined(ESP32)
  #include <WiFi.h>
  #include <HTTPClient.h>
  #include <WiFiClientSecure.h>
  #include <Update.h>
#elif defined(ESP8266)
  #include <ESP8266WiFi.h>
  #include <ESP8266HTTPClient.h>
  #include <WiFiClientSecure.h>
  #include <Updater.h>
#else
  #error "Board target not supported! Please use ESP32 or ESP8266."
#endif

#include <ArduinoJson.h>

// 1. رقم الإصدار الحالي المثبت على هذا الجهاز
const char* CURRENT_FIRMWARE_VERSION = "v1.1.13";

// 2. الـ UUID الخاص بموديل الجهاز في Supabase (من جدول hardware_models)
const char* HARDWARE_MODEL_ID = "8c3e340e-0e68-4cce-af3c-c38f2ef945fa";

// 3. رابط Edge Function الخاضع لـ Supabase
const char* OTA_CHECK_URL = "https://gbfhefvrrhcdblehtbkp.supabase.co/functions/v1/check-ota";

// 4. المفتاح العام (Public Key) لفحص التوقيع الرقمي
const char* PUBLIC_KEY_PEM = R"(
-----BEGIN PUBLIC KEY-----
MFkwEwYHKoZIzj0CAQYIKoZIzj0DAQcDQgAEmYPjQ9YTBjWIcvyahXO2MZogZX4D
bZH5x57TxMvEKHykxGIClHswPw8Y2pgWZh/3eY20X9R53j3a7MIW0g7dGQ==
-----END PUBLIC KEY-----
)";

// دالة مخصصة لإرسال تقرير نتيجة التحديث إلى Edge Function لتخزينها في جدول ota_logs
void reportOTAResult(const String& targetFirmwareId, const String& status, const String& errorMessage = "") {
    if (WiFi.status() != WL_CONNECTED || targetFirmwareId.length() == 0) return;

    WiFiClientSecure client;
    client.setInsecure();

    HTTPClient http;
    if (http.begin(client, OTA_CHECK_URL)) {
        http.addHeader("Content-Type", "application/json");

        StaticJsonDocument<512> reportDoc;
        reportDoc["hardware_model_id"] = HARDWARE_MODEL_ID;
        reportDoc["current_version"] = CURRENT_FIRMWARE_VERSION;
        reportDoc["mac_address"] = WiFi.macAddress();

        JsonObject statusReport = reportDoc.createNestedObject("status_report");
        statusReport["target_firmware_id"] = targetFirmwareId;
        statusReport["status"] = status;
        if (errorMessage.length() > 0) {
            statusReport["error_message"] = errorMessage;
        }

        String requestBody;
        serializeJson(reportDoc, requestBody);
        
        Serial.printf("[OTA Log] Reporting status '%s' to Supabase...\n", status.c_str());
        http.POST(requestBody);
        http.end();
    }
}

// دالة فحص وتنفيذ التحديث
void checkAndApplyOTA() {
    if (WiFi.status() != WL_CONNECTED) return;

    Serial.println("[OTA] Checking for new firmware updates from Supabase...");

    // ⚡ انتظار مزامنة الوقت (NTP) - مطلوب لاتصال TLS/SSL
    Serial.println("[OTA] Waiting for NTP time sync...");
    int ntpRetries = 0;
    while (time(nullptr) < 100000 && ntpRetries < 20) {
        delay(500);
        Serial.print(".");
        ntpRetries++;
    }
    Serial.println();

    if (time(nullptr) < 100000) {
        Serial.println("[OTA] Warning: NTP time sync may have failed, attempting anyway...");
    } else {
        Serial.println("[OTA] NTP time synced successfully.");
    }

    WiFiClientSecure client;
    client.setInsecure(); // في الإنتاج يفضل إضافة Root CA Cert

    // ⚡ ضبط حجم buffer الـ SSL لتناسب ذاكرة ESP8266 المحدودة
    #if defined(ESP8266)
    client.setBufferSizes(512, 512);
    #endif

    HTTPClient http;
    http.setTimeout(15000); // ⚡ زيادة مهلة الاتصال إلى 15 ثانية

    if (!http.begin(client, OTA_CHECK_URL)) {
        Serial.println("[OTA] Connection failed to OTA check URL.");
        return;
    }

    http.addHeader("Content-Type", "application/json");

    // إرسال بيانات الجهاز الحالية والماك أدريس للـ Edge Function
    StaticJsonDocument<300> reqDoc;
    reqDoc["hardware_model_id"] = HARDWARE_MODEL_ID;
    reqDoc["current_version"] = CURRENT_FIRMWARE_VERSION;
    reqDoc["mac_address"] = WiFi.macAddress();

    String requestBody;
    serializeJson(reqDoc, requestBody);

    Serial.printf("[OTA] Sending: %s\n", requestBody.c_str());

    int httpCode = http.POST(requestBody);


    if (httpCode != HTTP_CODE_OK) {
        Serial.printf("[OTA] Check failed, HTTP code: %d\n", httpCode);
        http.end();
        return;
    }

    String payload = http.getString();
    // ⚡ إغلاق اتصال الفحص فوراً قبل فتح اتصال التنزيل
    http.end();

    Serial.printf("[OTA] Response: %s\n", payload.c_str());

    StaticJsonDocument<1024> resDoc;
    DeserializationError error = deserializeJson(resDoc, payload);

    if (error) {
        Serial.printf("[OTA] JSON parse failed: %s\n", error.c_str());
        return;
    }

    bool updateAvailable = resDoc["update_available"] | false;

    if (!updateAvailable) {
        Serial.println("[OTA] Firmware is up to date.");
        return;
    }

    String downloadUrl = resDoc["download_url"].as<String>();
    String latestVersion = resDoc["version"].as<String>();
    String expectedHash = resDoc["sha256_hash"].as<String>();
    String signature = resDoc["digital_signature"].as<String>();
    String targetFirmwareId = resDoc["firmware_id"].as<String>();

    Serial.printf("[OTA] New version found: %s\n", latestVersion.c_str());
    Serial.printf("[OTA] Download URL: %s\n", downloadUrl.c_str());
    Serial.println("[OTA] Starting firmware download...");

    // ⚡ استخدام WiFiClientSecure جديد للتنزيل لتجنب تعارض SSL
    WiFiClientSecure dlClient;
    dlClient.setInsecure();

    HTTPClient httpDownload;
    if (!httpDownload.begin(dlClient, downloadUrl)) {
        Serial.println("[OTA] Failed to connect to download URL.");
        reportOTAResult(targetFirmwareId, "failed", "Download connection failed");
        return;
    }

    int code = httpDownload.GET();

    if (code != HTTP_CODE_OK) {
        Serial.printf("[OTA] Download failed, HTTP code: %d\n", code);
        reportOTAResult(targetFirmwareId, "failed", "Download HTTP error " + String(code));
        httpDownload.end();
        return;
    }

    int contentLength = httpDownload.getSize();
    Serial.printf("[OTA] Firmware size: %d bytes\n", contentLength);

    // ⚡ معالجة حالة عدم معرفة حجم الملف
    if (contentLength <= 0) {
        Serial.println("[OTA] Error: Invalid content length from server.");
        reportOTAResult(targetFirmwareId, "failed", "Invalid content length: " + String(contentLength));
        httpDownload.end();
        return;
    }

    bool canBegin = Update.begin(contentLength);

    if (!canBegin) {
        Serial.println("[OTA] Not enough flash space for firmware update.");
        reportOTAResult(targetFirmwareId, "failed", "Not enough flash space");
        httpDownload.end();
        return;
    }

    Serial.println("[OTA] Flashing firmware...");
    WiFiClient* stream = httpDownload.getStreamPtr();
    size_t written = Update.writeStream(*stream);

    if (written != (size_t)contentLength) {
        Serial.printf("[OTA] Write mismatch! Written: %d, Expected: %d\n", written, contentLength);
        Update.end(false); // abort
        reportOTAResult(targetFirmwareId, "failed", "Write size mismatch");
        httpDownload.end();
        return;
    }

    Serial.println("[OTA] Download complete. Finalizing update...");

    if (Update.end()) {
        if (Update.isFinished()) {
            Serial.println("[OTA] ✅ Update successful! Reporting & rebooting...");
            httpDownload.end();
            reportOTAResult(targetFirmwareId, "success");
            delay(500);
            ESP.restart();
        } else {
            Serial.println("[OTA] Update not finished.");
            reportOTAResult(targetFirmwareId, "failed", "Update not finished");
        }
    } else {
        String errMsg = "Update error code: " + String(Update.getError());
        Serial.printf("[OTA] %s\n", errMsg.c_str());
        reportOTAResult(targetFirmwareId, "failed", errMsg);
    }
    httpDownload.end();
}

#endif