#pragma once
#include <Arduino.h>
#include <HTTPClient.h>
#include <ArduinoJson.h>
#include <WiFi.h>
#include "config.h"

extern void appWatchdogFeed();

// ============================================================
// Wetter-Datenstruktur
// ============================================================
struct DayForecast {
    float   temp_day    = 0;
    float   temp_night  = 0;
    uint8_t icon        = 0;   // 0=Sonne,1=Wolken,2=Regen,3=Schnee,4=Gewitter,5=Nebel
    String  condition   = "";
};

struct WeatherData {
    float       current_temp   = 0;
    String      current_cond   = "";
    uint8_t     current_icon   = 0;
    DayForecast forecast[3];   // +1, +2, +3 Tage
    DayForecast today;         // Heute: Tages-Max (8-18h) + Min (alle Einträge)
    bool        valid          = false;
    uint32_t    last_fetch_ms  = 0;
    String      error          = "";
};

WeatherData weather;

struct HttpResult {
    int    code = -1;
    String body = "";
    String error = "";
};

// ============================================================
// Hilfsfunktionen
// ============================================================
static uint8_t _owmIcon(const char* code) {
    if (!code || strlen(code) < 2) return 1;
    if (code[0]=='0' && (code[1]=='1'||code[1]=='2')) return 0; // klar / leicht bew.
    if (code[0]=='0' && (code[1]=='3'||code[1]=='4')) return 1; // bewölkt
    if (code[0]=='0' && (code[1]=='9'))               return 2; // Schauer
    if (code[0]=='1' && code[1]=='0')                 return 2; // Regen
    if (code[0]=='1' && code[1]=='1')                 return 4; // Gewitter
    if (code[0]=='1' && code[1]=='3')                 return 3; // Schnee
    if (code[0]=='5' && code[1]=='0')                 return 5; // Nebel
    return 1;
}

static String _condDE(int id) {
    if (id >= 200 && id < 300) return "Gewitter";
    if (id >= 300 && id < 400) return "Niesel";
    if (id >= 500 && id < 504) return "Regen";
    if (id == 511)             return "Eisregen";
    if (id >= 511 && id < 600) return "Schauer";
    if (id >= 600 && id < 700) return "Schnee";
    if (id >= 700 && id < 800) return "Nebel";
    if (id == 800)             return "Sonnig";
    if (id == 801)             return "Leicht bew.";
    if (id == 802)             return "Bewoelkt";
    if (id >= 803)             return "Bedeckt";
    return "";
}

static String _condEN(int id) {
    if (id >= 200 && id < 300) return "Thunder";
    if (id >= 300 && id < 400) return "Drizzle";
    if (id >= 500 && id < 504) return "Rain";
    if (id == 511)             return "Sleet";
    if (id >= 511 && id < 600) return "Showers";
    if (id >= 600 && id < 700) return "Snow";
    if (id >= 700 && id < 800) return "Fog";
    if (id == 800)             return "Sunny";
    if (id == 801)             return "Lt.Cloudy";
    if (id == 802)             return "Cloudy";
    if (id >= 803)             return "Overcast";
    return "";
}

static String _cond(int id) {
    return (cfg.lang == "de") ? _condDE(id) : _condEN(id);
}

// ============================================================
// UTF-8 → Latin-1 für eigenes Umlaut-Rendering in display.h
// ä/ö/ü/Ä/Ö/Ü → Latin-1-Einzelbyte (0xE4/0xF6/0xFC/0xC4/0xD6/0xDC)
// ß → "ss" (kein darstellbares Glyph bei dieser Fontgröße)
// Alle anderen UTF-8-Multibyte-Sequenzen werden verworfen.
// ============================================================
static String _sanitizeUtf8(const String& s) {
    String out;
    out.reserve(s.length() + 4);
    const uint8_t* p = (const uint8_t*)s.c_str();
    while (*p) {
        if (p[0] == 0xC3 && p[1]) {
            switch (p[1]) {
                case 0xA4: out += (char)0xE4; p += 2; continue;  // ä
                case 0xB6: out += (char)0xF6; p += 2; continue;  // ö
                case 0xBC: out += (char)0xFC; p += 2; continue;  // ü
                case 0x84: out += (char)0xC4; p += 2; continue;  // Ä
                case 0x96: out += (char)0xD6; p += 2; continue;  // Ö
                case 0x9C: out += (char)0xDC; p += 2; continue;  // Ü
                case 0x9F: out += "ss";        p += 2; continue;  // ß
            }
        }
        if (p[0] >= 0x80) { p++; continue; }  // sonstigen Multibyte-Rest verwerfen
        out += (char)*p++;
    }
    return out;
}

static String weatherDisplayTextToUtf8(const String& s) {
    String out;
    out.reserve(s.length() + 4);
    const uint8_t* p = (const uint8_t*)s.c_str();
    while (*p) {
        switch (*p) {
            case 0xE4: out += "\xC3\xA4"; break; // ä
            case 0xF6: out += "\xC3\xB6"; break; // ö
            case 0xFC: out += "\xC3\xBC"; break; // ü
            case 0xC4: out += "\xC3\x84"; break; // Ä
            case 0xD6: out += "\xC3\x96"; break; // Ö
            case 0xDC: out += "\xC3\x9C"; break; // Ü
            default:   out += (char)*p; break;
        }
        p++;
    }
    return out;
}

// ============================================================
// HTTP GET helper — gibt Status, Body und Fehlertext zurück
// ============================================================
static HttpResult _httpGet(const String& url) {
    HttpResult result;
    HTTPClient http;
    http.setTimeout(8000);
    if (!http.begin(url)) {
        result.error = "HTTP begin fehlgeschlagen";
        return result;
    }

    appWatchdogFeed();
    result.code = http.GET();
    appWatchdogFeed();
    if (result.code == 200) {
        result.body = http.getString();
    } else if (result.code > 0) {
        result.error = "HTTP " + String(result.code);
    } else {
        result.error = "HTTP Fehler " + String(result.code);
    }

    http.end();
    appWatchdogFeed();
    return result;
}

static void _setWeatherError(const String& msg, bool keepValid = false) {
    weather.error = msg;
    if (!keepValid) weather.valid = false;
}

static int _dayDiff(const struct tm& nowTm, const struct tm& forecastTm) {
    struct tm nowCopy = nowTm;
    struct tm forecastCopy = forecastTm;
    nowCopy.tm_hour = 0;
    nowCopy.tm_min = 0;
    nowCopy.tm_sec = 0;
    forecastCopy.tm_hour = 0;
    forecastCopy.tm_min = 0;
    forecastCopy.tm_sec = 0;

    time_t nowDay = mktime(&nowCopy);
    time_t forecastDay = mktime(&forecastCopy);
    if (nowDay == (time_t)-1 || forecastDay == (time_t)-1) return 999;
    return (int)((forecastDay - nowDay) / 86400L);
}

// ============================================================
// Wetter abrufen (Current + 5-day Forecast)
// ============================================================
bool fetchWeather() {
    if (cfg.owm_apikey.isEmpty()) {
        _setWeatherError("Kein API-Key");
        return false;
    }
    if (WiFi.status() != WL_CONNECTED) {
        _setWeatherError("Kein WLAN");
        return false;
    }

    const String units  = (cfg.owm_unit == 0) ? "metric" : "imperial";
    const String lang   = cfg.lang;
    String locParam;

    if (cfg.owm_use_coords) {
        // Koordinaten direkt verwenden
        locParam = "lat=" + String(cfg.owm_lat, 4) + "&lon=" + String(cfg.owm_lon, 4);
    } else {
        if (cfg.owm_city.isEmpty()) {
            _setWeatherError("Kein Ort");
            return false;
        }
        // Stadtname via Geocoding API in Koordinaten auflösen, damit
        // dieselbe OWM-Station wie bei direkter lat/lon-Abfrage getroffen wird.
        String city = cfg.owm_city;
        city.replace(" ", "+");
        String geoUrl = "http://api.openweathermap.org/geo/1.0/direct?q="
                      + city + "&limit=1&appid=" + cfg.owm_apikey;
        HttpResult geoRes = _httpGet(geoUrl);
        bool geoOk = false;
        if (!geoRes.body.isEmpty()) {
            JsonDocument geoDoc;
            if (!deserializeJson(geoDoc, geoRes.body) && geoDoc.is<JsonArray>()
                && geoDoc.as<JsonArray>().size() > 0) {
                float lat = geoDoc[0]["lat"] | 0.0f;
                float lon = geoDoc[0]["lon"] | 0.0f;
                locParam = "lat=" + String(lat, 4) + "&lon=" + String(lon, 4);
                geoOk = true;
            }
        }
        if (!geoOk) {
            // Fallback: Stadtname direkt (falls Geocoding fehlschlägt)
            locParam = "q=" + city;
        }
    }

    // --- Aktuelles Wetter ---
    String url = "http://api.openweathermap.org/data/2.5/weather?" + locParam
               + "&units=" + units + "&lang=" + lang
               + "&appid=" + cfg.owm_apikey;
    HttpResult currentRes = _httpGet(url);
    if (currentRes.body.isEmpty()) {
        _setWeatherError("Current: " + currentRes.error);
        return false;
    }

    {
        JsonDocument doc;
        if (deserializeJson(doc, currentRes.body)) {
            _setWeatherError("JSON Fehler (current)");
            return false;
        }
        if (!doc["main"].is<JsonObject>() || !doc["weather"][0].is<JsonObject>()) {
            _setWeatherError("Antwort unvollstaendig (current)");
            return false;
        }
        weather.current_temp = doc["main"]["temp"].as<float>();
        weather.current_cond = _sanitizeUtf8(doc["weather"][0]["description"].as<String>());
        weather.current_icon = _owmIcon(doc["weather"][0]["icon"] | "01d");
    }

    // --- 5-Tage / 3-Stunden Forecast ---
    url = "http://api.openweathermap.org/data/2.5/forecast?" + locParam
        + "&cnt=40&units=" + units + "&lang=" + lang
        + "&appid=" + cfg.owm_apikey;
    HttpResult forecastRes = _httpGet(url);
    if (forecastRes.body.isEmpty()) {
        // Current weather war OK, Forecast nicht zwingend
        weather.valid      = true;
        weather.error      = "Forecast: " + forecastRes.error;
        weather.last_fetch_ms = millis();
        return true;
    }

    {
        JsonDocument doc;
        if (deserializeJson(doc, forecastRes.body)) {
            weather.valid      = true;
            weather.error      = "JSON Fehler (forecast)";
            weather.last_fetch_ms = millis();
            return true;
        }

        if (!doc["list"].is<JsonArray>()) {
            weather.valid      = true;
            weather.error      = "Forecast unvollstaendig";
            weather.last_fetch_ms = millis();
            return true;
        }

        // Heute ermitteln
        time_t now = time(nullptr);
        struct tm nowTm;
        localtime_r(&now, &nowTm);

        // Forecast-Tage initialisieren
        float dayMax[3]  = {-999, -999, -999};
        float nightMin[3] = { 999,  999,  999};
        int   dayCondId[3] = {800, 800, 800};
        const char* dayIcon[3] = {"01d", "01d", "01d"};
        String dayCondDesc[3] = {"", "", ""};

        // Heute
        float todayMax    = -999;
        float todayMin    =  999;
        int   todayCondId = 800;
        const char* todayIcon = "01d";
        String todayCondDesc  = "";

        JsonArray list = doc["list"];
        for (JsonObject item : list) {
            time_t dt  = item["dt"].as<time_t>();
            struct tm ft;
            localtime_r(&dt, &ft);

            // Differenz in Tagen zum heutigen Tag
            int diff = _dayDiff(nowTm, ft);
            if (diff < 0) continue;
            if (diff > 3) continue;
            int idx = diff - 1;

            float temp = item["main"]["temp"] | 0.0f;
            int   hour = ft.tm_hour;
            int   wid  = item["weather"][0]["id"] | 800;

            if (diff == 0) {
                // Heute: Max über alle Einträge (Bedingung von wärmster Stunde), Min über alle
                if (temp > todayMax) {
                    todayMax      = temp;
                    todayCondId   = wid;
                    todayCondDesc = _sanitizeUtf8(item["weather"][0]["description"].as<String>());
                    todayIcon     = item["weather"][0]["icon"] | "01d";
                }
                if (temp < todayMin) todayMin = temp;
                continue;
            }

            if (hour >= 8 && hour <= 18) {
                // Tagestemperatur (Maximum)
                if (temp > dayMax[idx]) {
                    dayMax[idx]      = temp;
                    dayCondId[idx]   = wid;
                    dayCondDesc[idx] = _sanitizeUtf8(item["weather"][0]["description"].as<String>());
                    dayIcon[idx]     = item["weather"][0]["icon"] | "01d";
                }
            }
            if (hour >= 0 && hour <= 6) {
                // Nachttemperatur (Minimum)
                if (temp < nightMin[idx]) {
                    nightMin[idx] = temp;
                }
            }
        }

        for (int i = 0; i < 3; i++) {
            weather.forecast[i].temp_day   = (dayMax[i]   > -999) ? dayMax[i]   : 0;
            weather.forecast[i].temp_night = (nightMin[i] <  999) ? nightMin[i] : 0;
            weather.forecast[i].condition  = dayCondDesc[i].isEmpty()
                                             ? _cond(dayCondId[i])
                                             : dayCondDesc[i];
            weather.forecast[i].icon       = _owmIcon(dayIcon[i]);
        }
        weather.today.temp_day   = (todayMax > -999) ? todayMax : weather.current_temp;
        weather.today.temp_night = (todayMin <  999) ? todayMin : weather.current_temp;
        weather.today.condition  = todayCondDesc.isEmpty() ? _cond(todayCondId) : todayCondDesc;
        weather.today.icon       = _owmIcon(todayIcon);
    }

    weather.valid         = true;
    weather.error         = "";
    weather.last_fetch_ms = millis();
    return true;
}

// Wochentag-Name für Forecast (+tageOffset)
String forecastDayName(int dayOffset) {
    time_t now = time(nullptr);
    now += dayOffset * 86400L;
    struct tm t;
    localtime_r(&now, &t);
    if (cfg.lang == "de") {
        const char* days[] = {"Son","Mon","Die","Mit","Don","Fre","Sam"};
        return String(days[t.tm_wday]);
    } else {
        const char* days[] = {"Sun","Mon","Tue","Wed","Thu","Fri","Sat"};
        return String(days[t.tm_wday]);
    }
}
