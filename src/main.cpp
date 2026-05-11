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
#include <Wire.h>
#include <Adafruit_VEML7700.h>
#include <time.h>
#include <sys/time.h>
#include <math.h>
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
volatile bool weatherFetchActive = false;
static constexpr uint32_t WATCHDOG_TIMEOUT_MS = 120000UL;
static constexpr uint32_t POWERON_RESTART_DELAY_MS = 1500UL;
static constexpr uint32_t POWERON_RESTART_MAGIC = 0x48554237UL; // "HUB7"
static volatile uint32_t watchdogLastKickMs = 0;
static TaskHandle_t watchdogTaskHandle = nullptr;
static const char* watchdogPhase = "boot";
static bool weatherFetchDisplayPending = false;
RTC_DATA_ATTR uint32_t rtcPowerOnRestartMagic = 0;
RTC_DATA_ATTR char rtcLastWatchdog[160] = "";
RTC_DATA_ATTR char rtcLastWatchdogPhase[32] = "";
RTC_DATA_ATTR uint32_t rtcLastWatchdogAgeMs = 0;
RTC_DATA_ATTR uint32_t rtcLastWatchdogEpoch = 0;
static constexpr int PIN_FACTORY_RESET = 3;
static constexpr int I2C_PIN_SDA = 38;
static constexpr int I2C_PIN_SCL = 39;
static constexpr uint8_t I2C_ADDR_DS3231 = 0x68;
static constexpr uint8_t I2C_ADDR_VEML7700 = 0x10;
static constexpr uint8_t I2C_ADDR_BME680_0 = 0x76;
static constexpr uint8_t I2C_ADDR_BME680_1 = 0x77;
static constexpr uint32_t FACTORY_RESET_HOLD_MS = 4000UL;
static uint32_t factoryResetPressStartMs = 0;
static bool factoryResetHandled = false;
Adafruit_VEML7700 veml;
bool i2cDs3231Present = false;
bool i2cVEML7700Present = false;
bool i2cBme680Present = false;
int i2cDs3231ProbeCode = -1;
int i2cVeml7700ProbeCode = -1;
String i2cRtcDiag = "VEML=- @0x10, DS3231=- @0x68";
String i2cStatusSummary = "nicht geprueft";
String i2cBusScanSummary = "-";
int i2cBme680Address = -1;
int i2cBme680ChipId = -1;
bool rtcTimeValid = false;
String rtcTimeSource = "keine";
String rtcTimeSourceDetail = "";
uint32_t lastRtcReadMs = 0;
uint32_t lastRtcWriteMs = 0;
time_t lastNtpSyncEpoch = 0;
bool vemlOk = false;
String vemlDiag = "nicht gefunden";
float luxFilt = NAN;
float luxHold = NAN;
float luxCurrent = NAN;
unsigned long luxWarmupUntilMs = 0;
unsigned long lastLuxMs = 0;
unsigned long vemlRetryAtMs = 0;
uint8_t vemlRetryCount = 0;
unsigned long lastBrightnessApplyMs = 0;
uint8_t appliedBrightness = 0;

static void refreshI2cStatusSummary() {
    i2cStatusSummary = String("DS3231:")
                    + (i2cDs3231Present ? "OK" : "fehlt")
                    + " VEML7700:"
                    + (i2cVEML7700Present ? "OK" : "fehlt")
                    + " BME680:"
                    + (i2cBme680Present ? "OK" : "fehlt");
}

static inline uint8_t _bcdToDec(uint8_t v) { return (uint8_t)(((v >> 4) * 10) + (v & 0x0F)); }
static inline uint8_t _decToBcd(uint8_t v) { return (uint8_t)((((v / 10) & 0x0F) << 4) | (v % 10)); }
static bool applySystemTimeEpoch(time_t epoch, bool writeRtc, const String& source, const String& detail = "");

static uint8_t luxToBrightness(float lux) {
    constexpr float LUX_MIN = 0.2f;
    constexpr float LUX_MAX = 235.0f;
    constexpr float GAMMA_C = 0.65f;
    if (!isfinite(lux)) return cfg.brightness;
    if (lux < LUX_MIN) lux = LUX_MIN;
    if (lux > LUX_MAX) lux = LUX_MAX;
    float x = (lux - LUX_MIN) / (LUX_MAX - LUX_MIN);
    float y = powf(x, GAMMA_C);
    int out = (int)lroundf(y * 255.0f);
    if (out < 8) out = 8;
    if (out > 255) out = 255;
    return (uint8_t)out;
}

static void rtcSetSource(const String& source, const String& detail = "") {
    rtcTimeSource = source;
    rtcTimeSourceDetail = detail;
}

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
    weatherFetchActive = false;
    weatherFetchDisplayPending = false;
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

static void weatherFetchTask(void*) {
    bool ok = fetchWeather();
    if (ok) {
        Serial.printf("[Weather] %.1f° — %s\n",
                      weather.current_temp, weather.current_cond.c_str());
    } else {
        Serial.printf("[Weather] Fehler: %s\n",
                      weather.error.isEmpty() ? "unbekannt" : weather.error.c_str());
    }
    weatherFetchActive = false;
    vTaskDelete(nullptr);
}

static bool i2cProbeAddress(uint8_t address) {
    Wire.beginTransmission(address);
    return Wire.endTransmission() == 0;
}

static int i2cProbeCode(uint8_t address) {
    Wire.beginTransmission(address);
    return (int)Wire.endTransmission();
}

static bool i2cReadReg8(uint8_t address, uint8_t reg, uint8_t& out) {
    Wire.beginTransmission(address);
    Wire.write(reg);
    if (Wire.endTransmission(false) != 0) return false;
    if (Wire.requestFrom((int)address, 1) != 1) return false;
    out = Wire.read();
    return true;
}

static bool ds3231ReadEpoch(time_t& outEpoch) {
    if (!i2cDs3231Present) return false;

    Wire.beginTransmission(I2C_ADDR_DS3231);
    Wire.write((uint8_t)0x00);
    if (Wire.endTransmission(false) != 0) return false;
    if (Wire.requestFrom((int)I2C_ADDR_DS3231, 7) != 7) return false;

    uint8_t secRaw = Wire.read();
    uint8_t minRaw = Wire.read();
    uint8_t hourRaw = Wire.read();
    Wire.read(); // day of week, ungenutzt
    uint8_t mdayRaw = Wire.read();
    uint8_t monRaw = Wire.read();
    uint8_t yearRaw = Wire.read();

    struct tm t = {};
    t.tm_sec  = _bcdToDec(secRaw & 0x7F);
    t.tm_min  = _bcdToDec(minRaw & 0x7F);
    t.tm_hour = _bcdToDec(hourRaw & 0x3F);
    t.tm_mday = _bcdToDec(mdayRaw & 0x3F);
    t.tm_mon  = _bcdToDec(monRaw & 0x1F) - 1;
    t.tm_year = _bcdToDec(yearRaw) + 100; // 2000..2099
    t.tm_isdst = -1;

    if (t.tm_year < 124 || t.tm_mon < 0 || t.tm_mon > 11 || t.tm_mday < 1 || t.tm_mday > 31) {
        return false;
    }
    outEpoch = mktime(&t);
    return outEpoch > 1700000000L;
}

static bool ds3231WriteEpoch(time_t epoch) {
    if (!i2cDs3231Present) return false;
    struct tm t;
    localtime_r(&epoch, &t);

    uint8_t dow = (uint8_t)(t.tm_wday == 0 ? 7 : t.tm_wday); // DS3231: 1..7
    uint8_t buf[8] = {
        0x00,
        _decToBcd((uint8_t)t.tm_sec),
        _decToBcd((uint8_t)t.tm_min),
        _decToBcd((uint8_t)t.tm_hour),
        _decToBcd(dow),
        _decToBcd((uint8_t)t.tm_mday),
        _decToBcd((uint8_t)(t.tm_mon + 1)),
        _decToBcd((uint8_t)(t.tm_year % 100))
    };
    Wire.beginTransmission(I2C_ADDR_DS3231);
    Wire.write(buf, sizeof(buf));
    if (Wire.endTransmission(true) != 0) return false;
    lastRtcWriteMs = millis();
    return true;
}

bool rtcGetEpoch(time_t& outEpoch) {
    return ds3231ReadEpoch(outEpoch);
}

bool rtcSetEpoch(time_t epoch) {
    if (!ds3231WriteEpoch(epoch)) return false;
    return applySystemTimeEpoch(epoch, false, "DS3231 aktiv", "RTC manuell gesetzt");
}

static bool applySystemTimeEpoch(time_t epoch, bool writeRtc, const String& source, const String& detail) {
    if (writeRtc && !ds3231WriteEpoch(epoch)) return false;
    struct timeval tv = { .tv_sec = epoch, .tv_usec = 0 };
    settimeofday(&tv, nullptr);
    rtcTimeValid = true;
    rtcSetSource(source, detail);
    return true;
}

static bool rtcRefreshSystemTime(bool force = false) {
    if (!i2cDs3231Present) return false;
    uint32_t nowMs = millis();
    if (!force && (nowMs - lastRtcReadMs) < 1000UL) return rtcTimeValid;

    time_t rtcEpoch = 0;
    if (!ds3231ReadEpoch(rtcEpoch)) {
        rtcTimeValid = false;
        rtcSetSource("keine", "DS3231 Zeit ungueltig");
        return false;
    }

    struct timeval tv = { .tv_sec = rtcEpoch, .tv_usec = 0 };
    settimeofday(&tv, nullptr);
    rtcTimeValid = true;
    rtcSetSource("DS3231 aktiv", "RTC ist Zeitquelle");
    lastRtcReadMs = nowMs;
    return true;
}

static String i2cScanSummary() {
    String list;
    for (uint8_t addr = 0x03; addr <= 0x77; addr++) {
        if (!i2cProbeAddress(addr)) continue;
        char buf[7];
        snprintf(buf, sizeof(buf), "0x%02X", addr);
        if (!list.isEmpty()) list += " ";
        list += buf;
    }
    return list.isEmpty() ? "-" : list;
}

static void detectI2cDevices() {
    i2cDs3231ProbeCode = i2cProbeCode(I2C_ADDR_DS3231);
    i2cVeml7700ProbeCode = i2cProbeCode(I2C_ADDR_VEML7700);
    bool hasDs3231 = (i2cDs3231ProbeCode == 0);
    bool hasVEML7700 = (i2cVeml7700ProbeCode == 0);
    i2cRtcDiag = String("VEML=") + i2cVeml7700ProbeCode
              + " @0x10, DS3231=" + i2cDs3231ProbeCode + " @0x68";
    i2cBme680Address = -1;
    i2cBme680ChipId = -1;

    bool bme76 = i2cProbeAddress(I2C_ADDR_BME680_0);
    bool bme77 = i2cProbeAddress(I2C_ADDR_BME680_1);
    bool hasBme680 = bme76 || bme77;
    if (bme76) i2cBme680Address = I2C_ADDR_BME680_0;
    else if (bme77) i2cBme680Address = I2C_ADDR_BME680_1;

    if (i2cBme680Address >= 0) {
        uint8_t chipId = 0;
        if (i2cReadReg8((uint8_t)i2cBme680Address, 0xD0, chipId)) {
            i2cBme680ChipId = chipId;
            // 0x61 = BME680/BME688
            if (chipId != 0x61) hasBme680 = false;
        } else {
            hasBme680 = false;
        }
    }

    i2cBusScanSummary = i2cScanSummary();
    i2cDs3231Present = hasDs3231;
    i2cVEML7700Present = hasVEML7700;
    i2cBme680Present = hasBme680;

    Serial.printf("[I2C] DS3231 (0x%02X): %s\n", I2C_ADDR_DS3231, hasDs3231 ? "OK" : "nicht gefunden");
    Serial.printf("[I2C] VEML7700 (0x%02X): %s\n", I2C_ADDR_VEML7700, hasVEML7700 ? "OK" : "nicht gefunden");
    Serial.printf("[I2C] RTC-Diag: %s\n", i2cRtcDiag.c_str());
    Serial.printf("[I2C] BME680 (0x%02X/0x%02X): %s\n",
                  I2C_ADDR_BME680_0,
                  I2C_ADDR_BME680_1,
                  hasBme680 ? "OK" : "nicht gefunden");
    if (i2cBme680Address >= 0) {
        Serial.printf("[I2C] BME680 addr=0x%02X chip_id=0x%02X\n",
                      i2cBme680Address,
                      (i2cBme680ChipId < 0 ? 0 : i2cBme680ChipId));
    } else {
        Serial.println("[I2C] BME680 addr=-- chip_id=--");
    }
    Serial.printf("[I2C] Scan: %s\n", i2cBusScanSummary.c_str());
    if (!hasDs3231 || !hasVEML7700 || !hasBme680) {
        lastStatusMsg = "I2C Geraet fehlt";
        refreshI2cStatusSummary();
    } else {
        i2cStatusSummary = "DS3231:OK VEML7700:OK BME680:OK";
    }
    if (!i2cDs3231Present) {
        rtcTimeValid = false;
        rtcSetSource("keine", "DS3231 nicht gefunden");
    }

    vemlOk = hasVEML7700 && veml.begin();
    if (vemlOk) {
        veml.setGain(VEML7700_GAIN_2);
        veml.setIntegrationTime(VEML7700_IT_50MS);
        veml.interruptEnable(false);
        luxWarmupUntilMs = millis() + 1500;
        vemlRetryAtMs = 0;
        vemlRetryCount = 0;
        vemlDiag = i2cRtcDiag + " | VEML OK";
    } else {
        luxWarmupUntilMs = 0;
        vemlRetryAtMs = millis() + 3000;
        vemlRetryCount++;
        vemlDiag = i2cRtcDiag + " | VEML NACK";
    }
    refreshI2cStatusSummary();
}

static void retryLightSensorIfNeeded() {
    if (vemlOk) return;
    if (!vemlRetryAtMs) return;
    if ((long)(millis() - vemlRetryAtMs) < 0) return;
    if (vemlRetryCount > 10) {
        vemlRetryAtMs = 0;
        return;
    }
    bool hasVEML7700 = (i2cProbeCode(I2C_ADDR_VEML7700) == 0);
    i2cVEML7700Present = hasVEML7700;
    vemlOk = hasVEML7700 && veml.begin();
    if (vemlOk) {
        veml.setGain(VEML7700_GAIN_2);
        veml.setIntegrationTime(VEML7700_IT_50MS);
        veml.interruptEnable(false);
        luxWarmupUntilMs = millis() + 1500;
        vemlRetryAtMs = 0;
        vemlRetryCount = 0;
        vemlDiag = i2cRtcDiag + " | VEML OK";
    } else {
        vemlRetryAtMs = millis() + 3000;
        vemlRetryCount++;
        vemlDiag = i2cRtcDiag + " | VEML NACK";
    }
    refreshI2cStatusSummary();
}

static void updateBrightnessFromLux(bool force = false) {
    auto applyBrightness = [&](uint8_t value, bool immediate = false) {
        if (immediate || value != appliedBrightness) {
            displaySetBrightness(value);
            appliedBrightness = value;
            lastBrightnessApplyMs = millis();
        }
    };

    if (!vemlOk || !cfg.auto_brightness) {
        applyBrightness(cfg.brightness, force);
        return;
    }
    unsigned long now = millis();
    if (!force && (long)(now - luxWarmupUntilMs) < 0) {
        return;
    }
    if (!force && now - lastLuxMs < 1200) return;
    lastLuxMs = now;

    float lux = veml.readLux();
    if (!isfinite(lux) || lux < 0.0f) {
        // Bei Lesefehlern letzte Helligkeit halten, um Flackern zu vermeiden.
        return;
    }
    luxCurrent = lux;

    if (!isfinite(luxFilt)) luxFilt = lux;
    else {
        float a = (lux > luxFilt) ? 0.55f : 0.12f;
        luxFilt = (1.0f - a) * luxFilt + a * lux;
    }

    const float FAST_DELTA = 8.0f;
    if (!isfinite(luxHold)) luxHold = luxFilt;
    else {
        float d = luxFilt - luxHold;
        if (fabsf(d) >= FAST_DELTA) luxHold = luxFilt;
        else {
            if (luxFilt > luxHold + 1.2f) luxHold = luxFilt;
            else if (luxFilt < luxHold - 1.2f) luxHold = luxFilt;
        }
    }
    uint8_t target = luxToBrightness(luxHold);
    if (target > cfg.brightness) target = cfg.brightness;
    if (force) {
        applyBrightness(target, true);
        return;
    }
    int delta = (int)target - (int)appliedBrightness;
    if (delta < 0) delta = -delta;
    if (delta >= 3 && (now - lastBrightnessApplyMs >= 1200UL)) {
        applyBrightness(target, false);
    }
}

static void startWeatherFetchAsync(uint32_t nowMs) {
    if (weatherFetchActive) return;

    lastWeatherFetch = nowMs;
    weatherFetchActive = true;
    weatherFetchDisplayPending = true;
    displayResetWeatherLine();

    BaseType_t rc = xTaskCreatePinnedToCore(
        weatherFetchTask,
        "weatherFetch",
        10240,
        nullptr,
        1,
        nullptr,
        0);
    if (rc != pdPASS) {
        weatherFetchActive = false;
        weather.valid = false;
        weather.error = "Wetter-Task konnte nicht gestartet werden";
        displayResetWeatherLine();
        Serial.println("[Weather] Task konnte nicht gestartet werden");
    }
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

static bool needsPowerOnSettleRestart(esp_reset_reason_t reason) {
    bool coldReset = (reason == ESP_RST_POWERON || reason == ESP_RST_BROWNOUT);
    if (!coldReset) {
        rtcPowerOnRestartMagic = 0;
        return false;
    }

    if (rtcPowerOnRestartMagic == POWERON_RESTART_MAGIC) return false;

    rtcPowerOnRestartMagic = POWERON_RESTART_MAGIC;
    return true;
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
    bool apOk = false;
    if (cfg.ap_password.isEmpty()) {
        apOk = WiFi.softAP(apSSID);
    } else if (cfg.ap_password.length() < 8) {
        Serial.println("[AP] AP-Passwort ist zu kurz, starte offenen Konfigurations-AP.");
        apOk = WiFi.softAP(apSSID);
    } else {
        apOk = WiFi.softAP(apSSID, cfg.ap_password.c_str());
    }
    if (!apOk) {
        Serial.println("[AP] softAP konnte nicht gestartet werden.");
        lastStatusMsg = "AP Fehler";
        displayShowMessage(lastStatusMsg, C_WARN);
        return;
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
    time_t now = time(nullptr);
    lastNtpSyncEpoch = now;
    if (i2cDs3231Present) {
        if (applySystemTimeEpoch(now, true, "DS3231 aktiv", "RTC von NTP korrigiert")) {
            rtcTimeValid = true;
        } else {
            rtcSetSource("keine", "RTC konnte nicht geschrieben werden");
        }
    } else {
        rtcSetSource("NTP aktiv", "keine RTC");
    }
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
    esp_reset_reason_t bootReason = esp_reset_reason();
    displayPreparePins();
    if (needsPowerOnSettleRestart(bootReason)) {
        Serial.printf("\n\n[Boot] %s: warte %lu ms und starte einmal neu...\n",
                      resetReasonName(bootReason),
                      (unsigned long)POWERON_RESTART_DELAY_MS);
        delay(POWERON_RESTART_DELAY_MS);
        ESP.restart();
    }

    resetRuntimeState();
    setupWatchdog();
    Serial.printf("\n\n=== HUB75 Clock %s ===\n", FW_VERSION);
    Serial.printf("[Boot] Reset-Grund: %s (%d)\n",
                  resetReasonName(bootReason), (int)bootReason);

    // LittleFS
    if (!LittleFS.begin(true)) {
        Serial.println("[FS] LittleFS-Fehler!");
    } else {
        if (configLoad()) {
            configSanitize();
            configSave();
            Serial.println("[FS] Config geladen.");
        } else {
            cfg = Config();
            configSanitize();
            if (configSave()) Serial.println("[FS] Keine gueltige Config gefunden, Defaults gespeichert.");
            else Serial.println("[FS] Keine gueltige Config gefunden, Defaults konnten nicht gespeichert werden.");
        }
    }

    pinMode(PIN_FACTORY_RESET, INPUT_PULLUP);
    Wire.begin(I2C_PIN_SDA, I2C_PIN_SCL);
    Wire.setClock(100000);
    detectI2cDevices();
    if (i2cDs3231Present) {
        rtcRefreshSystemTime(true);
    }
    updateBrightnessFromLux(true);

    // Display mit Double-Buffering
    displayInit();
    appliedBrightness = displayStartupBrightness();
    lastBrightnessApplyMs = millis();
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

    } else {
        startAP();
        webServerSetup();
    }

    // Beide DMA-Buffer + alle statischen Display-Zustände zurücksetzen.
    // ESP.restart() löscht RAM NICHT — ohne Reset bleiben alte digitDirty/flip
    // Werte erhalten → Ghosting nach Software-Neustart.
    displayReset();
    displayRampBrightness(cfg.brightness);
    updateBrightnessFromLux(true);

    Serial.println("[Setup] Fertig.");
}

// ============================================================
// Loop
// ============================================================
void loop() {
    watchdogSetPhase("loop");
    bool resetPressed = (digitalRead(PIN_FACTORY_RESET) == LOW);
    if (resetPressed) {
        if (factoryResetPressStartMs == 0) factoryResetPressStartMs = millis();
        if (!factoryResetHandled && (millis() - factoryResetPressStartMs >= FACTORY_RESET_HOLD_MS)) {
            factoryResetHandled = true;
            Serial.println("[FactoryReset] Taste gehalten -> Konfiguration wird zurueckgesetzt");
            configReset();
            displayShowMessage("Factory Reset...", C_WARN);
            factoryResetPending = true;
        }
    } else {
        factoryResetPressStartMs = 0;
        factoryResetHandled = false;
    }

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

    retryLightSensorIfNeeded();
    updateBrightnessFromLux();

    if (weatherFetchDisplayPending && !weatherFetchActive) {
        weatherFetchDisplayPending = false;
        displayResetWeatherLine();
    }

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
    bool timeOk = false;
    if (i2cDs3231Present) {
        rtcRefreshSystemTime();
        time_t nowEpoch = time(nullptr);
        localtime_r(&nowEpoch, &t);
        timeOk = rtcTimeValid;
    } else {
        timeOk = getLocalTime(&t, 10);
    }

    bool hasUsableTime = timeOk && (i2cDs3231Present ? rtcTimeValid : ntpSynced);
    if (!hasUsableTime) {
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
    if (!cfg.owm_apikey.isEmpty() && wifiConnected && !weatherFetchActive &&
        (lastWeatherFetch == 0 || nowMs - lastWeatherFetch >= weatherInterval)) {
        watchdogSetPhase("weather-fetch");
        startWeatherFetchAsync(nowMs);
    }

    watchdogSetPhase("loop-idle");
    appWatchdogFeed();
    delay(10);  // ~80 FPS: geringerer Loop-Jitter → bessere NTP-Sync-Präzision
}
