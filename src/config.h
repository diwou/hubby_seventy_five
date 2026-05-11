#pragma once
#include <Arduino.h>
#include <LittleFS.h>
#include <ArduinoJson.h>
#include <version_generated.h>

// ============================================================
// Firmware-Version
// ============================================================
#ifndef FW_VERSION
#define FW_VERSION APP_VERSION_BASE
#endif

#define CFG_FILE "/cfg.json"
static const char* DEFAULT_WEB_PASSWORD = "hub75clock";
static const char* DEFAULT_COLOR_DIGIT  = "#FFC832";
static const char* DEFAULT_COLOR_DATE   = "#64A0FF";
static const char* DEFAULT_COLOR_TEMP   = "#FF8C28";
static const char* DEFAULT_COLOR_WARN   = "#FFDC00";
static const char* DEFAULT_COLOR_INFO   = "#A0A0A0";
static const char* DEFAULT_COLOR_SEC    = "#C88C14";
static const char* DEFAULT_COLOR_SEC5   = "#00DC46";
static const char* DEFAULT_COLOR_SEC10  = "#50DCFF";

// ============================================================
// Konfigurationsstruktur
// ============================================================
struct Config {
    // WiFi
    String  sta_ssid          = "";
    String  sta_pass          = "";

    // NTP
    String  ntp1              = "pool.ntp.org";
    String  ntp2              = "time.google.com";
    String  ntp3              = "time.cloudflare.com";
    uint16_t ntp_interval_min = 15;

    // Zeitzone (POSIX-String)
    String  tz                = "CET-1CEST,M3.5.0,M10.5.0/3";

    // Wetter (OpenWeatherMap)
    String  owm_apikey        = "";
    String  owm_city          = "";      // Stadtname ODER leer wenn Koordinaten
    float   owm_lat           = 0.0;
    float   owm_lon           = 0.0;
    bool    owm_use_coords    = false;   // true = lat/lon statt Stadt
    uint8_t owm_unit          = 0;       // 0=Celsius, 1=Fahrenheit
    uint16_t owm_interval_min = 30;      // Wetter-Aktualisierungsintervall

    // Anzeige
    uint8_t brightness        = 90;      // 0-255
    String  lang              = "de";    // "de" oder "en"
    uint8_t bottom_cycle_sec  = 6;       // Wechselintervall untere Zeile (s)
    uint8_t scroll_speed_ms   = 40;      // Scroll-Geschwindigkeit (ms/Pixel, 30-200)
    uint8_t time_12h          = 0;       // 0=24h, 1=12h (AM/PM)
    uint8_t date_format       = 0;       // 0=Mo TT.MM.JJ  1=Mo TT.MM  2=TT.MM.JJJJ  3=MM/TT/JJ (US)
    uint8_t anim_mode         = 0;       // 0=Slide-Up, 1=Cross-Dissolve
    uint8_t seconds_mode      = 0;       // 0=Bar, 1=Pulse, 2=Dots, 3=HH:MM:SS
    bool    forecast_scroll_up = true;   // Vorhersage-Kacheln von unten einscrollen
    uint8_t display_panel_cols = 2;      // aktuell im Web-UI: 1 oder 2 Panels nebeneinander
    uint8_t display_panel_rows = 1;      // Vorbereitung fuer spaeteres 2x2-Layout
    uint8_t display_panel_map  = 0;      // 0=linear, 1=zigzag/serpentine (fuer spaeteres 2x2)

    // Farben (hex #RRGGBB)
    String  color_digit     = DEFAULT_COLOR_DIGIT; // Ziffern Gold/Amber
    String  color_date      = DEFAULT_COLOR_DATE;  // Datum-Text
    String  color_temp      = DEFAULT_COLOR_TEMP;  // Temperatur
    String  color_warn      = DEFAULT_COLOR_WARN;  // Wochentag/Warnung
    String  color_info      = DEFAULT_COLOR_INFO;  // Grau/Info
    String  color_sec       = DEFAULT_COLOR_SEC;   // Sekunden-Balken
    String  color_sec5      = DEFAULT_COLOR_SEC5;  // 5s-Marke
    String  color_sec10     = DEFAULT_COLOR_SEC10; // 10s-Marke

    // OTA
    bool    ota_enabled       = true;
    String  ota_password      = "";
    String  web_password      = DEFAULT_WEB_PASSWORD;

    // System
    uint16_t wifi_timeout_sec = 30;
    String   ap_password      = "";      // leer = offener AP
};

Config cfg;

static bool _isHexColor(const String& value) {
    if (value.length() != 7 || value[0] != '#') return false;
    for (size_t i = 1; i < 7; i++) {
        char c = value[i];
        bool isNum = (c >= '0' && c <= '9');
        bool isHexLower = (c >= 'a' && c <= 'f');
        bool isHexUpper = (c >= 'A' && c <= 'F');
        if (!isNum && !isHexLower && !isHexUpper) return false;
    }
    return true;
}

static inline void _sanitizeHexColor(String& value, const char* fallback) {
    if (!_isHexColor(value)) value = fallback;
}

static inline void configSanitizeColors() {
    _sanitizeHexColor(cfg.color_digit, DEFAULT_COLOR_DIGIT);
    _sanitizeHexColor(cfg.color_date, DEFAULT_COLOR_DATE);
    _sanitizeHexColor(cfg.color_temp, DEFAULT_COLOR_TEMP);
    _sanitizeHexColor(cfg.color_warn, DEFAULT_COLOR_WARN);
    _sanitizeHexColor(cfg.color_info, DEFAULT_COLOR_INFO);
    _sanitizeHexColor(cfg.color_sec, DEFAULT_COLOR_SEC);
    _sanitizeHexColor(cfg.color_sec5, DEFAULT_COLOR_SEC5);
    _sanitizeHexColor(cfg.color_sec10, DEFAULT_COLOR_SEC10);
}

static inline void _sanitizeTextField(String& value, size_t maxLen) {
    String out;
    out.reserve(value.length());
    for (size_t i = 0; i < value.length(); i++) {
        char c = value[i];
        if ((uint8_t)c < 32 || (uint8_t)c == 127) continue;
        out += c;
        if (out.length() >= maxLen) break;
    }
    value = out;
}

static inline void configSanitize() {
    _sanitizeTextField(cfg.sta_ssid, 32);
    _sanitizeTextField(cfg.sta_pass, 64);
    _sanitizeTextField(cfg.ntp1, 64);
    _sanitizeTextField(cfg.ntp2, 64);
    _sanitizeTextField(cfg.ntp3, 64);
    _sanitizeTextField(cfg.tz, 64);
    _sanitizeTextField(cfg.owm_apikey, 64);
    _sanitizeTextField(cfg.owm_city, 64);
    _sanitizeTextField(cfg.lang, 8);
    _sanitizeTextField(cfg.ap_password, 32);
    _sanitizeTextField(cfg.web_password, 32);
    _sanitizeTextField(cfg.ota_password, 32);

    if (cfg.lang != "de" && cfg.lang != "en") cfg.lang = "de";
    if (!cfg.ap_password.isEmpty() && cfg.ap_password.length() < 8) cfg.ap_password = "";
    if (cfg.web_password.length() < 8) cfg.web_password = DEFAULT_WEB_PASSWORD;
    configSanitizeColors();
}

// ============================================================
// Persistenz (LittleFS / JSON)
// ============================================================
bool configLoad() {
    if (!LittleFS.exists(CFG_FILE)) return false;
    File f = LittleFS.open(CFG_FILE, "r");
    if (!f) return false;

    JsonDocument doc;
    DeserializationError err = deserializeJson(doc, f);
    f.close();
    if (err) return false;

    cfg.sta_ssid          = doc["sta_ssid"]          | "";
    cfg.sta_pass          = doc["sta_pass"]          | "";
    cfg.ntp1              = doc["ntp1"]              | "pool.ntp.org";
    cfg.ntp2              = doc["ntp2"]              | "time.google.com";
    cfg.ntp3              = doc["ntp3"]              | "time.cloudflare.com";
    cfg.ntp_interval_min  = doc["ntp_interval_min"]  | 15;
    cfg.tz                = doc["tz"]                | "CET-1CEST,M3.5.0,M10.5.0/3";
    cfg.owm_apikey        = doc["owm_apikey"]        | "";
    cfg.owm_city          = doc["owm_city"]          | "";
    cfg.owm_lat           = doc["owm_lat"]           | 0.0f;
    cfg.owm_lon           = doc["owm_lon"]           | 0.0f;
    cfg.owm_use_coords    = doc["owm_use_coords"]    | false;
    cfg.owm_unit          = doc["owm_unit"]          | 0;
    cfg.owm_interval_min  = doc["owm_interval_min"]  | 30;
    cfg.brightness        = doc["brightness"]        | 90;
    cfg.lang              = doc["lang"]              | "de";
    cfg.bottom_cycle_sec  = doc["bottom_cycle_sec"]  | 6;
    cfg.scroll_speed_ms   = doc["scroll_speed_ms"]   | 40;
    if (cfg.scroll_speed_ms == 80) cfg.scroll_speed_ms = 40; // alten Standard auf weicheren Lauf migrieren
    cfg.time_12h          = doc["time_12h"]          | 0;
    cfg.date_format       = doc["date_format"]       | 0;
    cfg.anim_mode         = doc["anim_mode"]         | 0;
    cfg.seconds_mode      = doc["seconds_mode"]      | 0;
    cfg.forecast_scroll_up = doc["forecast_scroll_up"] | true;
    cfg.display_panel_cols = doc["display_panel_cols"] | 2;
    cfg.display_panel_rows = doc["display_panel_rows"] | 1;
    cfg.display_panel_map  = doc["display_panel_map"]  | 0;
    if (cfg.display_panel_cols < 1) cfg.display_panel_cols = 1;
    if (cfg.display_panel_cols > 2) cfg.display_panel_cols = 2;
    if (cfg.display_panel_rows < 1) cfg.display_panel_rows = 1;
    if (cfg.display_panel_rows > 1) cfg.display_panel_rows = 1; // 2x2 ist vorbereitet, aber noch nicht aktiv
    if (cfg.display_panel_map > 1) cfg.display_panel_map = 0;
    cfg.ota_enabled       = doc["ota_enabled"]       | true;
    cfg.ota_password      = doc["ota_password"]      | "";
    cfg.web_password      = doc["web_password"]      | DEFAULT_WEB_PASSWORD;
    cfg.wifi_timeout_sec  = doc["wifi_timeout_sec"]  | 30;
    cfg.ap_password       = doc["ap_password"]       | "";
    cfg.color_digit       = doc["color_digit"]       | DEFAULT_COLOR_DIGIT;
    cfg.color_date        = doc["color_date"]        | DEFAULT_COLOR_DATE;
    cfg.color_temp        = doc["color_temp"]        | DEFAULT_COLOR_TEMP;
    cfg.color_warn        = doc["color_warn"]        | DEFAULT_COLOR_WARN;
    cfg.color_info        = doc["color_info"]        | DEFAULT_COLOR_INFO;
    cfg.color_sec         = doc["color_sec"]         | DEFAULT_COLOR_SEC;
    cfg.color_sec5        = doc["color_sec5"]        | DEFAULT_COLOR_SEC5;
    cfg.color_sec10       = doc["color_sec10"]       | DEFAULT_COLOR_SEC10;
    configSanitize();
    return true;
}

bool configSave() {
    configSanitize();
    File f = LittleFS.open(CFG_FILE, "w");
    if (!f) return false;

    JsonDocument doc;
    doc["sta_ssid"]         = cfg.sta_ssid;
    doc["sta_pass"]         = cfg.sta_pass;
    doc["ntp1"]             = cfg.ntp1;
    doc["ntp2"]             = cfg.ntp2;
    doc["ntp3"]             = cfg.ntp3;
    doc["ntp_interval_min"] = cfg.ntp_interval_min;
    doc["tz"]               = cfg.tz;
    doc["owm_apikey"]       = cfg.owm_apikey;
    doc["owm_city"]         = cfg.owm_city;
    doc["owm_lat"]          = cfg.owm_lat;
    doc["owm_lon"]          = cfg.owm_lon;
    doc["owm_use_coords"]   = cfg.owm_use_coords;
    doc["owm_unit"]         = cfg.owm_unit;
    doc["owm_interval_min"] = cfg.owm_interval_min;
    doc["brightness"]       = cfg.brightness;
    doc["lang"]             = cfg.lang;
    doc["bottom_cycle_sec"] = cfg.bottom_cycle_sec;
    doc["scroll_speed_ms"]  = cfg.scroll_speed_ms;
    doc["time_12h"]         = cfg.time_12h;
    doc["date_format"]      = cfg.date_format;
    doc["anim_mode"]           = cfg.anim_mode;
    doc["seconds_mode"]        = cfg.seconds_mode;
    doc["forecast_scroll_up"]  = cfg.forecast_scroll_up;
    doc["display_panel_cols"]  = cfg.display_panel_cols;
    doc["display_panel_rows"]  = cfg.display_panel_rows;
    doc["display_panel_map"]   = cfg.display_panel_map;
    doc["ota_enabled"]         = cfg.ota_enabled;
    doc["ota_password"]     = cfg.ota_password;
    doc["web_password"]     = cfg.web_password;
    doc["wifi_timeout_sec"] = cfg.wifi_timeout_sec;
    doc["ap_password"]      = cfg.ap_password;
    doc["color_digit"]      = cfg.color_digit;
    doc["color_date"]       = cfg.color_date;
    doc["color_temp"]       = cfg.color_temp;
    doc["color_warn"]       = cfg.color_warn;
    doc["color_info"]       = cfg.color_info;
    doc["color_sec"]        = cfg.color_sec;
    doc["color_sec5"]       = cfg.color_sec5;
    doc["color_sec10"]      = cfg.color_sec10;

    serializeJson(doc, f);
    f.close();
    return true;
}

void configReset() {
    cfg = Config();
    configSave();
}
