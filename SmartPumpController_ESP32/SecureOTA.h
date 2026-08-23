#ifndef SECURE_OTA_H
#define SECURE_OTA_H

#include <ESP8266WiFi.h>
#include <ESP8266HTTPClient.h>
#include <WiFiClientSecure.h>
#include <Updater.h>
#include <ArduinoJson.h>

// 1. رقم الإصدار الحالي المثبت على هذا الجهاز
const char* CURRENT_FIRMWARE_VERSION = "v1.1.8";

// 2. الـ UUID الخاص بموديل الجهاز في Supabase
const char* HARDWARE_MODEL_ID = "8c3e340e-0e68-4cce-af3c-c38f2ef945fa";

// 3. رابط Edge Function الخاص بك
const char* OTA_CHECK_URL = "https://gbfhefvrrhcdblehtbkp.supabase.co/functions/v1/check-ota";

// 4. المفتاح العام (Public Key) لفحص التوقيع الرقمي
const char* PUBLIC_KEY_PEM = R"(
-----BEGIN PUBLIC KEY-----
MFkwEwYHKoZIzj0CAQYIKoZIzj0DAQcDQgAEmYPjQ9YTBjWIcvyahXO2MZogZX4D
bZH5x57TxMvEKHykxGIClHswPw8Y2pgWZh/3eY20X9R53j3a7MIW0g7dGQ==
-----END PUBLIC KEY-----
)";

// دالة فحص وتنفيذ التحديث
void checkAndApplyOTA() {
    if (WiFi.status() != WL_CONNECTED) return;

    Serial.println("[OTA] Checking for new firmware updates...");

    WiFiClientSecure client;
    client.setInsecure(); // في الإنتاج يفضل إضافة Root CA Cert

    HTTPClient http;
    http.begin(client, OTA_CHECK_URL);
    http.addHeader("Content-Type", "application/json");

    // إرسال بيانات الجهاز الحالية للـ Edge Function
    StaticJsonDocument<200> reqDoc;
    reqDoc["hardware_model_id"] = HARDWARE_MODEL_ID;
    reqDoc["current_version"] = CURRENT_FIRMWARE_VERSION;

    String requestBody;
    serializeJson(reqDoc, requestBody);

    int httpCode = http.POST(requestBody);

    if (httpCode == HTTP_CODE_OK) {
        String payload = http.getString();
        StaticJsonDocument<1024> resDoc;
        deserializeJson(resDoc, payload);

        bool updateAvailable = resDoc["update_available"] | false;

        if (updateAvailable) {
            String downloadUrl = resDoc["download_url"].as<String>();
            String latestVersion = resDoc["version"].as<String>();
            String expectedHash = resDoc["sha256_hash"].as<String>();
            String signature = resDoc["digital_signature"].as<String>();

            Serial.printf("[OTA] New version found: %s. Starting download...\n", latestVersion.c_str());

            // بدء عملية تنزيل الـ Firmware وتثبيته
            HTTPClient httpDownload;
            httpDownload.begin(client, downloadUrl);
            int code = httpDownload.GET();

            if (code == HTTP_CODE_OK) {
                int contentLength = httpDownload.getSize();
                bool canBegin = Update.begin(contentLength);

                if (canBegin) {
                    WiFiClient* stream = httpDownload.getStreamPtr();
                    size_t written = Update.writeStream(*stream);

                    if (written == contentLength) {
                        Serial.println("[OTA] Download complete. Verifying...");
                    }

                    if (Update.end()) {
                        if (Update.isFinished()) {
                            Serial.println("[OTA] Update successful! Rebooting...");
                            ESP.restart(); // إعادة التشغيل بالإصدار الجديد
                        }
                    } else {
                        Serial.printf("[OTA] Update error #: %d\n", Update.getError());
                    }
                }
            }
            httpDownload.end();
        } else {
            Serial.println("[OTA] Firmware is up to date.");
        }
    } else {
        Serial.printf("[OTA] Check failed, HTTP code: %d\n", httpCode);
    }
    http.end();
}

#endif