/*
  HUB75 NTP Flip Clock
  ESP32-S3 WROOM N16R8 + 74HCT245 + 64×32 HUB75 LED-Matrix

  Changelog: siehe CHANGELOG.md
*/

#include <Arduino.h>
#include <esp_system.h>
#include <WiFi.h>
#include <ESPmDNS.h>
#include <LittleFS.h>
#include <time.h>
#include <cstring>

#include "config.h"
#include "display.h"
#include "weather.h"
#include "webserver.h"

// ============================================================
// Globale Zustände
// ============================================================
bool     ntpSynced         = false;
uint32_t lastNtpSync       = 0;
uint32_t lastWeatherFetch  = 0;
uint32_t lastReconnectMs   = 0;
bool     wifiConnected     = false;
String   lastStatusMsg     = "";
static constexpr uint32_t WATCHDOG_TIMEOUT_MS = 120000UL;
static volatile uint32_t watchdogLastKickMs = 0;
static TaskHandle_t watchdogTaskHandle = nullptr;
static const char* watchdogPhase = "boot";
RTC_DATA_ATTR char rtcLastWatchdog[160] = "";
RTC_DATA_ATTR char rtcLastWatchdogPhase[32] = "";
RTC_DATA_ATTR uint32_t rtcLastWatchdogAgeMs = 0;
RTC_DATA_ATTR uint32_t rtcLastWatchdogEpoch = 0;

static void watchdogSetPhase(const char* phase) {
    watchdogPhase = phase;
}

static void resetRuntimeState() {
    apModeActive = false;
    otaRunning = false;
    restartPending = false;
    factoryResetPending = false;
    webUpdateOk = false;
    webUpdateError = "";
    ntpSynced = false;
    lastNtpSync = 0;
    lastWeatherFetch = 0;
    lastReconnectMs = 0;
    wifiConnected = false;
    lastStatusMsg = "";
    watchdogPhase = "boot";
    watchdogTaskHandle = nullptr;
    watchdogLastKickMs = millis();
}

static void watchdogRecordHang(const char* phase, uint32_t ageMs) {
    time_t now = time(nullptr);
    rtcLastWatchdogEpoch = (now > 1700000000L) ? (uint32_t)now : 0;
    snprintf(rtcLastWatchdog, sizeof(rtcLastWatchdog),
             "Phase '%s' nach %lu ms ohne Heartbeat",
             phase ? phase : "?",
             (unsigned long)ageMs);
    snprintf(rtcLastWatchdogPhase, sizeof(rtcLastWatchdogPhase), "%s",
             phase ? phase : "?");
    rtcLastWatchdogAgeMs = ageMs;
}

void appWatchdogFeed() {
    watchdogLastKickMs = millis();
}

static void setupWatchdog() {
    watchdogLastKickMs = millis();
    BaseType_t rc = xTaskCreatePinnedToCore(
        [](void*) {
            for (;;) {
                uint32_t age = millis() - watchdogLastKickMs;
                if (age > WATCHDOG_TIMEOUT_MS) {
                    watchdogRecordHang(watchdogPhase, age);
                    Serial.printf("[WDT] Kein Heartbeat seit %lu ms in Phase '%s', Neustart...\n",
                                  (unsigned long)age,
                                  watchdogPhase ? watchdogPhase : "?");
                    delay(100);
                    ESP.restart();
                }
                vTaskDelay(pdMS_TO_TICKS(1000));
            }
        },
        "appWDT",
        2048,
        nullptr,
        1,
        &watchdogTaskHandle,
        0);
    if (rc != pdPASS || watchdogTaskHandle == nullptr) {
        watchdogTaskHandle = nullptr;
        Serial.println("[WDT] Monitor konnte nicht gestartet werden");
        return;
    }
    Serial.printf("[WDT] Monitor aktiv (%lu ms)\n", (unsigned long)WATCHDOG_TIMEOUT_MS);
}

const char* resetReasonName(esp_reset_reason_t reason) {
    switch (reason) {
        case ESP_RST_UNKNOWN:   return "UNKNOWN";
        case ESP_RST_POWERON:   return "POWERON";
        case ESP_RST_EXT:       return "EXT";
        case ESP_RST_SW:        return "SW";
        case ESP_RST_PANIC:     return "PANIC";
        case ESP_RST_INT_WDT:   return "INT_WDT";
        case ESP_RST_TASK_WDT:  return "TASK_WDT";
        case ESP_RST_WDT:       return "WDT";
        case ESP_RST_DEEPSLEEP: return "DEEPSLEEP";
        case ESP_RST_BROWNOUT:  return "BROWNOUT";
        case ESP_RST_SDIO:      return "SDIO";
        default:                return "OTHER";
    }
}

// ============================================================
// WiFi: Station-Modus verbinden
// ============================================================
static bool wifiConnect() {
    watchdogSetPhase("wifi-connect");
    if (cfg.sta_ssid.isEmpty()) return false;

    Serial.printf("[WiFi] Verbinde mit '%s'...\n", cfg.sta_ssid.c_str());

    WiFi.mode(WIFI_STA);
    WiFi.begin(cfg.sta_ssid.c_str(), cfg.sta_pass.c_str());

    uint32_t timeout = (uint32_t)cfg.wifi_timeout_sec * 1000UL;
    uint32_t start   = millis();

    while (WiFi.status() != WL_CONNECTED) {
        appWatchdogFeed();
        if (millis() - start > timeout) {
            Serial.println("[WiFi] Timeout.");
            return false;
        }
        delay(250);
    }

    Serial.printf("[WiFi] Verbunden. IP: %s\n", WiFi.localIP().toString().c_str());
    return true;
}

// ============================================================
// AP-Modus starten (Captive Portal)
// ============================================================
static void startAP() {
    watchdogSetPhase("ap-setup");
    apModeActive = true;

    appWatchdogFeed();
    WiFi.disconnect(true);
    delay(100);

    uint8_t mac[6];
    WiFi.macAddress(mac);
    char apSSID[20];
    snprintf(apSSID, sizeof(apSSID), "HUB75CLOCK-%02X%02X", mac[4], mac[5]);

    WiFi.mode(WIFI_AP);
    if (cfg.ap_password.isEmpty()) {
        WiFi.softAP(apSSID);
    } else {
        WiFi.softAP(apSSID, cfg.ap_password.c_str());
    }
    delay(200);

    dnsServer.start(53, "*", WiFi.softAPIP());
    appWatchdogFeed();

    Serial.printf("[AP] SSID: %s  IP: %s\n", apSSID, WiFi.softAPIP().toString().c_str());
    displayShowAP(String(apSSID), WiFi.softAPIP().toString());
}

// ============================================================
// NTP konfigurieren + synchronisieren
// ============================================================
static void ntpSync() {
    watchdogSetPhase("ntp-sync");
    configTzTime(cfg.tz.c_str(),
                 cfg.ntp1.c_str(),
                 cfg.ntp2.c_str(),
                 cfg.ntp3.c_str());

    Serial.print("[NTP] Warte auf Synchronisation...");
    struct tm t;
    uint32_t start = millis();
    while (!getLocalTime(&t, 200)) {
        appWatchdogFeed();
        if (millis() - start > 10000) {
            Serial.println(" Timeout!");
            return;
        }
        Serial.print(".");
    }
    Serial.println(" OK");
    ntpSynced    = true;
    lastNtpSync  = millis();
}

// ============================================================
// Setup
// ============================================================
void setup() {
    watchdogSetPhase("setup");
    Serial.begin(115200);
    delay(300);
    resetRuntimeState();
    setupWatchdog();
    displayPreparePins();
    Serial.printf("\n\n=== HUB75 Clock %s ===\n", FW_VERSION);
    Serial.printf("[Boot] Reset-Grund: %s (%d)\n",
                  resetReasonName(esp_reset_reason()), (int)esp_reset_reason());

    // LittleFS
    if (!LittleFS.begin(true)) {
        Serial.println("[FS] LittleFS-Fehler!");
    } else {
        configLoad();
        Serial.println("[FS] Config geladen.");
    }

    // Display mit Double-Buffering
    displayInit();
    colorsInit();

    // Startmeldung in Back-Buffer schreiben und zeigen
    dma_display->clearScreen();
    dma_display->setTextSize(1);
    dma_display->setTextColor(C_WARN);
    dma_display->setCursor(0, 1);
    dma_display->print("HUB75 Clock");
    dma_display->setTextColor(C_INFO);
    dma_display->setCursor(0, 9);
    dma_display->print("v" FW_VERSION);
    dma_display->setTextColor(C_DATE);
    dma_display->setCursor(0, 17);
    dma_display->print("Verbinde...");
    dma_display->flipDMABuffer();  // zeigen

    // WiFi verbinden
    watchdogSetPhase("wifi-connect");
    wifiConnected = wifiConnect();

    if (wifiConnected) {
        apModeActive = false;

        if (MDNS.begin("hub75clock")) {
            Serial.println("[mDNS] hub75clock.local");
        }

        watchdogSetPhase("ntp-sync");
        ntpSync();
        webServerSetup();

        if (cfg.ota_enabled) {
            otaSetup();
        }

        // IP als scrollender Text anzeigen
        watchdogSetPhase("display-scroll");
        String scrollMsg = WiFi.localIP().toString() + String("   hub75clock.local");
        displayScrollText(scrollMsg, C_INFO, 35);

        // Wetter wird im normalen Loop abgerufen, damit der Start nicht an
        // einem blockierenden Netzwerk- oder API-Problem hängen bleiben kann.
        lastWeatherFetch = 0;

    } else {
        startAP();
        webServerSetup();
    }

    // Beide DMA-Buffer + alle statischen Display-Zustände zurücksetzen.
    // ESP.restart() löscht RAM NICHT — ohne Reset bleiben alte digitDirty/flip
    // Werte erhalten → Ghosting nach Software-Neustart.
    displayReset();
    displayRampBrightness(cfg.brightness);

    Serial.println("[Setup] Fertig.");
}

// ============================================================
// Loop
// ============================================================
void loop() {
    watchdogSetPhase("loop");
    // Neustart sicher aus dem Haupt-Loop (nicht aus AsyncWebServer-Callback)
    if (factoryResetPending || restartPending) {
        watchdogSetPhase("restart");
        appWatchdogFeed();
        delay(300);
        ESP.restart();
    }

    if (cfg.ota_enabled && wifiConnected) {
        watchdogSetPhase("ota-handle");
        ArduinoOTA.handle();
    }
    if (otaRunning) return;

    // DNS-Server im AP-Modus
    if (apModeActive) {
        watchdogSetPhase("ap-mode");
        dnsServer.processNextRequest();
        appWatchdogFeed();
        delay(5);
        return;
    }

    // WiFi-Reconnect (alle 30s)
    if (WiFi.status() != WL_CONNECTED) {
        wifiConnected = false;
        uint32_t now = millis();
        if (now - lastReconnectMs >= 30000UL) {
        lastReconnectMs = now;
        Serial.println("[WiFi] Verbindung verloren, Reconnect...");
            if (lastStatusMsg != "Reconnect..") {
                lastStatusMsg = "Reconnect..";
                dma_display->clearScreen();
                displayShowMessage(lastStatusMsg, C_WARN);
                dma_display->flipDMABuffer();
            }
            watchdogSetPhase("wifi-connect");
            wifiConnected = wifiConnect();
            if (wifiConnected) {
                watchdogSetPhase("ntp-sync");
                ntpSync();
                if (cfg.ota_enabled) otaSetup();
                lastStatusMsg = "";
            }
        }
        delay(100);
        return;
    }

    // Aktuelle Zeit holen
    struct tm t;
    bool timeOk = getLocalTime(&t, 10);

    if (!timeOk || !ntpSynced) {
        watchdogSetPhase("ntp-wait");
        if (lastStatusMsg != "NTP sync..") {
            lastStatusMsg = "NTP sync..";
            dma_display->clearScreen();
            displayShowMessage(lastStatusMsg, C_WARN);
            dma_display->flipDMABuffer();
        }
        appWatchdogFeed();
        delay(200);
        return;
    }
    lastStatusMsg = "";

    // Flip-Clock + Sekunden + Datum + Wetter zeichnen
    watchdogSetPhase("display-update");
    appWatchdogFeed();
    displayUpdate(&t);
    appWatchdogFeed();

    // NTP re-sync
    uint32_t nowMs       = millis();
    uint32_t ntpInterval = (uint32_t)cfg.ntp_interval_min * 60000UL;
    if (nowMs - lastNtpSync >= ntpInterval) {
        watchdogSetPhase("ntp-resync");
        lastNtpSync = nowMs;
        configTzTime(cfg.tz.c_str(), cfg.ntp1.c_str(),
                     cfg.ntp2.c_str(), cfg.ntp3.c_str());
        Serial.println("[NTP] Re-Sync.");
    }

    // Wetter-Aktualisierung
    uint32_t weatherInterval = (uint32_t)cfg.owm_interval_min * 60000UL;
    if (!cfg.owm_apikey.isEmpty() && wifiConnected &&
        (lastWeatherFetch == 0 || nowMs - lastWeatherFetch >= weatherInterval)) {
        lastWeatherFetch = nowMs;
        watchdogSetPhase("weather-fetch");
        appWatchdogFeed();
        fetchWeather();
        appWatchdogFeed();
        Serial.printf("[Weather] %.1f° — %s\n",
                      weather.current_temp, weather.current_cond.c_str());
    }

    watchdogSetPhase("loop-idle");
    appWatchdogFeed();
    delay(10);  // ~80 FPS: geringerer Loop-Jitter → bessere NTP-Sync-Präzision
}
