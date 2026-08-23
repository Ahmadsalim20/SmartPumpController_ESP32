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
    #if defined(ESP8266)
    client.setBufferSizes(2048, 512);
    #endif

    HTTPClient http;
    http.setTimeout(15000);
    if (http.begin(client, OTA_CHECK_URL)) {
        http.addHeader("Content-Type", "application/json");

        DynamicJsonDocument reportDoc(512);
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
    while (time(nullptr) < 100000 && ntpRetries < 15) {
        delay(300);
        ntpRetries++;
    }

    if (time(nullptr) < 100000) {
        Serial.println("[OTA] Warning: NTP time sync may have failed, attempting anyway...");
    } else {
        Serial.println("[OTA] NTP time synced successfully.");
    }

    bool updateAvailable = false;
    String downloadUrl, latestVersion, expectedHash, signature, targetFirmwareId;

    Serial.printf("[OTA] Free heap before check: %u bytes\n", ESP.getFreeHeap());

    // --- المرحلة الأولى: فحص التحديث داخل نطاق مغلق لتحرير ذاكرة SSL فور الانتهاء ---
    {
        WiFiClientSecure client;
        client.setInsecure();
        #if defined(ESP8266)
        client.setBufferSizes(2048, 512);
        #endif

        HTTPClient http;
        http.setTimeout(15000);

        if (http.begin(client, OTA_CHECK_URL)) {
            http.addHeader("Content-Type", "application/json");

            DynamicJsonDocument reqDoc(300);
            reqDoc["hardware_model_id"] = HARDWARE_MODEL_ID;
            reqDoc["current_version"] = CURRENT_FIRMWARE_VERSION;
            reqDoc["mac_address"] = WiFi.macAddress();

            String requestBody;
            serializeJson(reqDoc, requestBody);
            Serial.printf("[OTA] Sending: %s\n", requestBody.c_str());

            int httpCode = http.POST(requestBody);
            if (httpCode == HTTP_CODE_OK) {
                String payload = http.getString();
                Serial.printf("[OTA] Response: %s\n", payload.c_str());

                DynamicJsonDocument resDoc(1024);
                DeserializationError error = deserializeJson(resDoc, payload);
                if (!error) {
                    updateAvailable = resDoc["update_available"] | false;
                    if (updateAvailable) {
                        downloadUrl = resDoc["download_url"].as<String>();
                        latestVersion = resDoc["version"].as<String>();
                        expectedHash = resDoc["sha256_hash"].as<String>();
                        signature = resDoc["digital_signature"].as<String>();
                        targetFirmwareId = resDoc["firmware_id"].as<String>();
                    }
                } else {
                    Serial.printf("[OTA] JSON parse failed: %s\n", error.c_str());
                }
            } else {
                Serial.printf("[OTA] Check failed, HTTP code: %d\n", httpCode);
            }
            http.end();
        }
    } // ⚡ تدمير كائن client التابع للمرحلة الأولى وتفريغ 16KB من الذاكرة فوراً!

    Serial.printf("[OTA] Free heap after check: %u bytes\n", ESP.getFreeHeap());

    if (!updateAvailable) {
        Serial.println("[OTA] Firmware is up to date.");
        return;
    }

    Serial.printf("[OTA] New version found: %s\n", latestVersion.c_str());
    Serial.printf("[OTA] Free heap before download: %u bytes\n", ESP.getFreeHeap());
    Serial.println("[OTA] Starting firmware download...");

    bool downloadSuccess = false;
    String downloadErrorReason = "";

    // --- المرحلة الثانية: تنزيل وتثبيت الفيرموير داخل نطاق مغلق مستقل ---
    {
        HTTPClient httpDownload;
        httpDownload.setTimeout(30000);

        bool beginSuccess = false;
        WiFiClient* stream = nullptr;

        #if defined(ESP8266)
        WiFiClient plainClient;
        WiFiClientSecure dlClient;
        if (downloadUrl.startsWith("http://")) {
            Serial.println("[OTA] Using HTTP (No TLS memory overhead)");
            beginSuccess = httpDownload.begin(plainClient, downloadUrl);
        } else {
            Serial.println("[OTA] Using HTTPS (BearSSL)");
            dlClient.setInsecure();
            // ملاحظة: تم إزالة setBufferSizes(2048, 512) لأن Supabase ترسل حزم TLS بحجم 16KB
            // وتحديد البفر بـ 2048 كان يسبب انقطاع التنزيل عند 53KB
            dlClient.setBufferSizes(4096, 512); 
            beginSuccess = httpDownload.begin(dlClient, downloadUrl);
        }
        #else
        WiFiClientSecure dlClient;
        dlClient.setInsecure();
        beginSuccess = httpDownload.begin(dlClient, downloadUrl);
        #endif

        if (beginSuccess) {
            int code = httpDownload.GET();
            if (code == HTTP_CODE_OK) {
                int contentLength = httpDownload.getSize();
                Serial.printf("[OTA] Firmware size: %d bytes\n", contentLength);

                if (contentLength > 0 && Update.begin(contentLength)) {
                    Serial.println("[OTA] Flashing firmware...");
                    stream = httpDownload.getStreamPtr();
                    stream->setTimeout(15000);

                    uint8_t* buff = (uint8_t*)malloc(1024);
                    if (buff) {
                        size_t totalWritten = 0;
                        unsigned long lastProgressLog = 0;
                        unsigned long lastReadTime = millis();

                        while (totalWritten < (size_t)contentLength) {
                            size_t sizeAvailable = stream->available();
                            if (sizeAvailable > 0) {
                                int c = stream->readBytes(buff, min(sizeAvailable, (size_t)1024));
                                if (c > 0) {
                                    size_t written = Update.write(buff, c);
                                    if (written != (size_t)c) {
                                        Serial.printf("[OTA] Flash write failed at byte %d\n", totalWritten);
                                        break;
                                    }
                                    totalWritten += written;
                                    lastReadTime = millis();

                                    if (totalWritten - lastProgressLog >= 51200 || totalWritten == (size_t)contentLength) {
                                        lastProgressLog = totalWritten;
                                        Serial.printf("[OTA] Progress: %d / %d bytes (%d%%) | Heap: %u bytes\n", totalWritten, contentLength, (totalWritten * 100) / contentLength, ESP.getFreeHeap());
                                    }
                                }
                            } else {
                                if (millis() - lastReadTime > 25000) {
                                    Serial.println("[OTA] Download timeout: No data received for 25 seconds.");
                                    break;
                                }
                                delay(10);
                            }
                        }

                        free(buff);

                        if (totalWritten == (size_t)contentLength) {
                            if (Update.end() && Update.isFinished()) {
                                downloadSuccess = true;
                            } else {
                                downloadErrorReason = "Update end failed";
                            }
                        } else {
                            Update.end(false);
                            downloadErrorReason = "Write mismatch: " + String(totalWritten) + "/" + String(contentLength);
                        }
                    } else {
                        downloadErrorReason = "OOM for download buffer";
                    }
                } else {
                    downloadErrorReason = (contentLength <= 0) ? "Invalid content length" : "Not enough flash space";
                }
            } else {
                downloadErrorReason = "Download HTTP error " + String(code);
            }
            httpDownload.end();
        } else {
            downloadErrorReason = "Download connection failed";
        }
    } // ⚡ تدمير كائنات الاتصال وتفريغ الذاكرة قبل إرسال التقرير

    Serial.printf("[OTA] Free heap after download: %u bytes\n", ESP.getFreeHeap());

    // --- المرحلة الثالثة: إرسال التقرير النهائي وإعادة التشغيل ---
    if (downloadSuccess) {
        Serial.println("[OTA] ✅ Update successful! Reporting & rebooting...");
        reportOTAResult(targetFirmwareId, "success");
        delay(500);
        ESP.restart();
    } else {
        Serial.printf("[OTA] Update failed: %s\n", downloadErrorReason.c_str());
        reportOTAResult(targetFirmwareId, "failed", downloadErrorReason);
    }
}

#endif