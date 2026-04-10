#pragma once
#include <Arduino.h>
#include <WiFi.h>
#include <ESPAsyncWebServer.h>
#include <DNSServer.h>
#include <HTTPClient.h>
#include <ArduinoJson.h>
#include <ArduinoOTA.h>
#include <LittleFS.h>
#include "lwip/apps/sntp.h"
#include "config.h"
#include "weather.h"

// ============================================================
// Instanzen
// ============================================================
AsyncWebServer  webServer(80);
DNSServer       dnsServer;
bool            apModeActive  = false;
bool            otaRunning    = false;
bool            restartPending = false;
bool            factoryResetPending = false;

// Externe Variablen (definiert in main.cpp / display.h)
extern bool     ntpSynced;
extern uint32_t lastNtpSync;
extern uint32_t lastWeatherFetch;

// Forward-Deklarationen (definiert in display.h)
void displayShowOTA(uint32_t progress, uint32_t total);
void displaySetOtaActive(bool active);
void colorsInit();
void displaySetBrightness(uint8_t brightness);

static uint8_t _clampU8(uint32_t value, uint8_t minVal, uint8_t maxVal) {
    if (value < minVal) return minVal;
    if (value > maxVal) return maxVal;
    return (uint8_t)value;
}

static uint16_t _clampU16(uint32_t value, uint16_t minVal, uint16_t maxVal) {
    if (value < minVal) return minVal;
    if (value > maxVal) return maxVal;
    return (uint16_t)value;
}

// ============================================================
// HTML — Tabbed UI
// ============================================================
static const char HTML_PAGE[] PROGMEM = R"rawhtml(<!DOCTYPE html>
<html lang="de"><head>
<meta charset="UTF-8"><meta name="viewport" content="width=device-width,initial-scale=1">
<title>HUB75 Clock</title>
<style>
*{box-sizing:border-box;margin:0;padding:0}
body{font-family:sans-serif;background:#1a1a2e;color:#eee;padding:10px}
h1{color:#f0a500;text-align:center;padding:10px 0;font-size:1.3em}
.tabs{display:flex;flex-wrap:wrap;gap:4px;margin-bottom:12px}
.tab{padding:6px 12px;background:#16213e;border:1px solid #444;border-radius:4px;cursor:pointer;font-size:.85em;color:#aaa}
.tab.active{background:#f0a500;color:#000;border-color:#f0a500;font-weight:bold}
.panel{display:none;background:#16213e;border:1px solid #333;border-radius:6px;padding:14px}
.panel.active{display:block}
label{display:block;margin-top:10px;font-size:.85em;color:#aaa}
input,select{width:100%;padding:6px 8px;margin-top:3px;background:#0f3460;border:1px solid #555;border-radius:4px;color:#eee;font-size:.9em}
input[type=checkbox]{width:auto;margin-right:6px}
.row{display:flex;gap:8px}.row>*{flex:1}
.btn{padding:8px 16px;border:none;border-radius:4px;cursor:pointer;font-size:.9em;margin-top:8px;margin-right:4px}
.btn-save{background:#f0a500;color:#000;font-weight:bold}
.btn-danger{background:#c0392b;color:#fff}
.btn-info{background:#2980b9;color:#fff}
.btn-sm{padding:5px 10px;font-size:.8em;margin-top:4px}
.status-box{background:#0f3460;border-radius:4px;padding:10px;font-size:.82em;line-height:1.9}
.status-box .lbl{color:#f0a500;font-weight:bold;display:inline-block;width:140px}
.status-box .val{color:#eee}
.geo-results{margin-top:6px;display:none}
.geo-item{padding:5px 8px;background:#0f3460;border:1px solid #333;border-radius:3px;cursor:pointer;margin-bottom:3px;font-size:.85em}
.geo-item:hover{background:#1a3c70;border-color:#f0a500}
#msg{margin-top:10px;padding:8px;border-radius:4px;display:none;text-align:center;font-size:.9em}
.ok{background:#27ae60}.err{background:#c0392b}
.timer{color:#aaa;font-size:.75em}
</style></head><body>
<h1>HUB75 Flip Clock <span id="fwVer" style="font-size:.6em;color:#888"></span></h1>
<div class="tabs">
  <div class="tab active" onclick="show('status',this)">Status</div>
  <div class="tab" onclick="show('wifi',this)">WLAN</div>
  <div class="tab" onclick="show('ntp',this)">NTP</div>
  <div class="tab" onclick="show('weather',this)">Wetter</div>
  <div class="tab" onclick="show('display',this)">Anzeige</div>
  <div class="tab" onclick="show('ota',this)">OTA</div>
</div>

<!-- STATUS -->
<div id="p-status" class="panel active">
  <div class="status-box" id="statusBox">Lade...</div>
  <p class="timer" id="statusTimer"></p>
  <div style="margin-top:8px">
    <button class="btn btn-info" onclick="loadStatus()">Aktualisieren</button>
    <button class="btn btn-info" onclick="if(confirm('Neustart?'))fetch('/api/restart').then(()=>{showMsg('Neustart...',true);setTimeout(()=>location.reload(),6000)})">Neustart</button>
    <button class="btn btn-danger" onclick="if(confirm('Factory Reset — alle Einstellungen löschen?'))fetch('/api/reset').then(()=>{showMsg('Reset...',true);setTimeout(()=>location.reload(),4000)})">Factory Reset</button>
  </div>
</div>

<!-- WLAN -->
<div id="p-wifi" class="panel">
  <label>SSID <input id="sta_ssid" maxlength="32"></label>
  <label>Passwort <input id="sta_pass" type="password" maxlength="64"></label>
  <label>AP-Passwort (leer = offener AP) <input id="ap_password" type="password" maxlength="32"></label>
  <label>WLAN Timeout (s) <input id="wifi_timeout_sec" type="number" min="10" max="120"></label>
  <button class="btn btn-save" onclick="save()">Speichern & Neustart</button>
</div>

<!-- NTP -->
<div id="p-ntp" class="panel">
  <label>NTP Server 1 <input id="ntp1" maxlength="64"></label>
  <label>NTP Server 2 <input id="ntp2" maxlength="64"></label>
  <label>NTP Server 3 <input id="ntp3" maxlength="64"></label>
  <label>Sync-Intervall (min) <input id="ntp_interval_min" type="number" min="5" max="1440"></label>
  <label>Zeitzone (POSIX-String)
    <select id="tz_preset" onchange="document.getElementById('tz').value=this.value">
      <option value="">-- Preset wählen --</option>
      <option value="CET-1CEST,M3.5.0,M10.5.0/3">Europa/Berlin (CET/CEST)</option>
      <option value="GMT0BST,M3.5.0/1,M10.5.0">Europa/London</option>
      <option value="EET-2EEST,M3.5.0/3,M10.5.0/4">Europa/Athen</option>
      <option value="UTC0">UTC</option>
      <option value="EST5EDT,M3.2.0,M11.1.0">USA/New York</option>
      <option value="CST6CDT,M3.2.0,M11.1.0">USA/Chicago</option>
      <option value="MST7MDT,M3.2.0,M11.1.0">USA/Denver</option>
      <option value="PST8PDT,M3.2.0,M11.1.0">USA/Los Angeles</option>
    </select>
    <input id="tz" maxlength="64">
  </label>
  <button class="btn btn-save" onclick="save()">Speichern</button>
</div>

<!-- WETTER -->
<div id="p-weather" class="panel">
  <label>OpenWeatherMap API-Key <input id="owm_apikey" maxlength="64"></label>
  <label><input type="checkbox" id="owm_use_coords" onchange="toggleCoords()"> Koordinaten statt Stadtname</label>
  <div id="div_city">
    <label>Stadt
      <div class="row" style="margin-top:3px">
        <input id="owm_city" maxlength="64" placeholder="z.B. Berlin">
        <button class="btn btn-info btn-sm" style="flex:0 0 auto" onclick="searchCity()">Suchen</button>
      </div>
    </label>
    <div class="geo-results" id="geoResults"></div>
  </div>
  <div id="div_coords" style="display:none">
    <div class="row">
      <label>Breitengrad (lat) <input id="owm_lat" type="number" step="0.0001"></label>
      <label>Längengrad (lon) <input id="owm_lon" type="number" step="0.0001"></label>
    </div>
  </div>
  <label>Einheit
    <select id="owm_unit">
      <option value="0">Celsius (°C)</option>
      <option value="1">Fahrenheit (°F)</option>
    </select>
  </label>
  <label>Abruf-Intervall (min) <input id="owm_interval_min" type="number" min="10" max="1440"></label>
  <button class="btn btn-save" onclick="save()">Speichern</button>
</div>

<!-- ANZEIGE -->
<div id="p-display" class="panel">
  <label>Helligkeit (0-255) <input id="brightness" type="number" min="0" max="255"></label>
  <label>Sprache
    <select id="lang">
      <option value="de">Deutsch</option>
      <option value="en">English</option>
    </select>
  </label>
  <label>Zeitformat
    <select id="time_12h">
      <option value="0">24-Stunden (13:45)</option>
      <option value="1">12-Stunden AM/PM (1:45)</option>
    </select>
  </label>
  <label>Datumsformat
    <select id="date_format">
      <option value="0">Mo TT.MM.JJ (Standard)</option>
      <option value="1">Mo TT.MM (ohne Jahr)</option>
      <option value="2">TT.MM.JJJJ (lang)</option>
      <option value="3">MM/TT/JJ (US)</option>
    </select>
  </label>
  <label>Animation
    <select id="anim_mode">
      <option value="0">Slide-Up</option>
      <option value="1">Cross-Dissolve</option>
      <option value="2">Keine</option>
    </select>
  </label>
  <label>Sekundenanzeige
    <select id="seconds_mode">
      <option value="0">Balken (Bar)</option>
      <option value="1">Wanderpunkt (Pulse)</option>
      <option value="2">Punkte (Dots)</option>
      <option value="3">HH:MM:SS</option>
    </select>
  </label>
  <label>Wetter-Wechselintervall (s) <input id="bottom_cycle_sec" type="number" min="2" max="60"></label>
  <label>Scroll-Geschwindigkeit (ms/Pixel, 30=schnell … 200=langsam) <input id="scroll_speed_ms" type="number" min="30" max="200"></label>
  <label><input type="checkbox" id="forecast_scroll_up"> Vorhersage-Kacheln von unten einscrollen</label>
  <hr style="border-color:#333;margin:12px 0">
  <p style="font-size:.85em;color:#aaa;margin-bottom:4px">Farben</p>
  <div class="row">
    <label>Ziffern <input id="color_digit" type="color"></label>
    <label>Datum <input id="color_date" type="color"></label>
  </div>
  <div class="row">
    <label>Temperatur <input id="color_temp" type="color"></label>
    <label>Wochentag <input id="color_warn" type="color"></label>
  </div>
  <div class="row">
    <label>Info/Grau <input id="color_info" type="color"></label>
    <label>Sek.-Balken <input id="color_sec" type="color"></label>
  </div>
  <div class="row">
    <label>5s-Marke <input id="color_sec5" type="color"></label>
    <label>10s-Marke <input id="color_sec10" type="color"></label>
  </div>
  <button class="btn btn-save" onclick="save()">Speichern</button>
</div>

<!-- OTA -->
<div id="p-ota" class="panel">
  <label><input type="checkbox" id="ota_enabled"> OTA-Updates aktiviert</label>
  <label>OTA-Passwort (leer = kein Passwort) <input id="ota_password" type="password" maxlength="32"></label>
  <button class="btn btn-save" onclick="save()">Speichern</button>
  <p style="margin-top:10px;font-size:.8em;color:#888">OTA-Hostname: hub75clock.local &nbsp;|&nbsp; Port: 8266</p>
</div>

<div id="msg"></div>

<script>
const fields=['sta_ssid','sta_pass','ap_password','wifi_timeout_sec','ntp1','ntp2','ntp3',
  'ntp_interval_min','tz','owm_apikey','owm_city','owm_lat','owm_lon','owm_use_coords',
  'owm_unit','owm_interval_min','brightness','lang','time_12h','date_format',
  'anim_mode','seconds_mode','bottom_cycle_sec','scroll_speed_ms','forecast_scroll_up',
  'ota_enabled','ota_password',
  'color_digit','color_date','color_temp','color_warn','color_info',
  'color_sec','color_sec5','color_sec10'];
// Select-Felder mit numerischen Werten (müssen als Int gespeichert werden)
const numSelects=new Set(['owm_unit','time_12h','date_format','anim_mode','seconds_mode']);

let statusInterval=null;

function show(id,el){
  document.querySelectorAll('.panel').forEach(p=>p.classList.remove('active'));
  document.querySelectorAll('.tab').forEach(t=>t.classList.remove('active'));
  document.getElementById('p-'+id).classList.add('active');
  el.classList.add('active');
  if(id==='status'){loadStatus();startStatusTimer();}else{stopStatusTimer();}
}

function startStatusTimer(){
  stopStatusTimer();
  let s=30;
  const t=document.getElementById('statusTimer');
  statusInterval=setInterval(()=>{
    s--;t.textContent='Auto-Refresh in '+s+'s';
    if(s<=0){s=30;loadStatus();}
  },1000);
}
function stopStatusTimer(){
  if(statusInterval){clearInterval(statusInterval);statusInterval=null;}
  document.getElementById('statusTimer').textContent='';
}

function toggleCoords(){
  const chk=document.getElementById('owm_use_coords').checked;
  document.getElementById('div_city').style.display=chk?'none':'block';
  document.getElementById('div_coords').style.display=chk?'block':'none';
}

function showMsg(text,ok){
  const m=document.getElementById('msg');
  m.textContent=text;m.className=ok?'ok':'err';m.style.display='block';
  setTimeout(()=>m.style.display='none',4000);
}

function fmtSec(s){
  if(s<0)return'jetzt';
  if(s<60)return s+'s';
  return Math.floor(s/60)+'min '+( s%60)+'s';
}

async function loadConfig(){
  const r=await fetch('/api/config');
  const d=await r.json();
  fields.forEach(f=>{
    const el=document.getElementById(f);if(!el)return;
    if(el.type==='checkbox')el.checked=!!d[f];
    else if(el.tagName==='SELECT')el.value=String(d[f]??'');
    else el.value=d[f]??'';
  });
  toggleCoords();
}

async function save(){
  const d={};
  fields.forEach(f=>{
    const el=document.getElementById(f);if(!el)return;
    if(el.type==='checkbox')d[f]=el.checked;
    else if(el.type==='number')d[f]=parseFloat(el.value)||0;
    else if(el.tagName==='SELECT'&&numSelects.has(f))d[f]=parseInt(el.value,10)||0;
    else d[f]=el.value;
  });
  const r=await fetch('/api/config',{method:'POST',headers:{'Content-Type':'application/json'},body:JSON.stringify(d)});
  const j=await r.json();
  showMsg(j.ok?'Gespeichert!':'Fehler: '+(j.error||'?'),j.ok);
}

async function loadStatus(){
  try{
    const r=await fetch('/api/status');
    const d=await r.json();
    const verEl=document.getElementById('fwVer');
    if(verEl)verEl.textContent='v'+d.firmware.replace(/^.*\s/,'');
    document.getElementById('statusBox').innerHTML=
      '<div><span class="lbl">Firmware</span><span class="val">'+d.firmware+'</span></div>'+
      '<div><span class="lbl">IP / mDNS</span><span class="val">'+d.ip+' / hub75clock.local</span></div>'+
      '<div><span class="lbl">WLAN</span><span class="val">'+d.ssid+' ('+d.rssi+' dBm)</span></div>'+
      '<div><span class="lbl">Uptime</span><span class="val">'+d.uptime+'</span></div>'+
      '<div><span class="lbl">Uhrzeit</span><span class="val">'+d.time+'</span></div>'+
      '<div><span class="lbl">NTP-Status</span><span class="val">'+d.ntp_status+'</span></div>'+
      '<div><span class="lbl">Aktiver NTP-Server</span><span class="val">'+d.ntp_active+'</span></div>'+
      '<div><span class="lbl">NTP-Server</span><span class="val">'+d.ntp_servers+'</span></div>'+
      '<div><span class="lbl">Nächster NTP-Sync</span><span class="val">'+fmtSec(d.next_ntp_sec)+'</span></div>'+
      '<div><span class="lbl">Wetter-Standort</span><span class="val">'+d.weather_loc+'</span></div>'+
      '<div><span class="lbl">Wetter</span><span class="val">'+d.weather_status+'</span></div>'+
      '<div><span class="lbl">Nächstes Wetter</span><span class="val">'+fmtSec(d.next_weather_sec)+'</span></div>'+
      '<div><span class="lbl">Freier Heap</span><span class="val">'+d.heap+' Byte</span></div>';
  }catch(e){document.getElementById('statusBox').textContent='Fehler beim Laden';}
}

async function searchCity(){
  const city=document.getElementById('owm_city').value.trim();
  const key=document.getElementById('owm_apikey').value.trim();
  if(!city||!key){showMsg('Bitte Stadt und API-Key eingeben',false);return;}
  const btn=event.target;
  btn.disabled=true;btn.textContent='...';
  try{
    const r=await fetch('/api/geocode?q='+encodeURIComponent(city)+'&apikey='+encodeURIComponent(key));
    const d=await r.json();
    const box=document.getElementById('geoResults');
    box.innerHTML='';
    if(!Array.isArray(d)||d.length===0){
      box.innerHTML='<div style="color:#c0392b;font-size:.85em">Keine Treffer gefunden.</div>';
    } else {
      d.forEach(item=>{
        const label=(item.name||'')+', '+(item.state?item.state+', ':'')+( item.country||'');
        const div=document.createElement('div');
        div.className='geo-item';
        div.textContent=label+' ('+item.lat.toFixed(3)+', '+item.lon.toFixed(3)+')';
        div.onclick=()=>{
          document.getElementById('owm_city').value=item.name;
          document.getElementById('owm_lat').value=item.lat.toFixed(6);
          document.getElementById('owm_lon').value=item.lon.toFixed(6);
          box.style.display='none';
          showMsg('Stadt gewählt: '+label,true);
        };
        box.appendChild(div);
      });
    }
    box.style.display='block';
  }catch(e){showMsg('Suche fehlgeschlagen',false);}
  btn.disabled=false;btn.textContent='Suchen';
}

loadConfig();
loadStatus();
startStatusTimer();
</script>
</body></html>)rawhtml";

// ============================================================
// Uptime-String
// ============================================================
static String _uptime() {
    uint32_t s = millis() / 1000;
    uint32_t d = s / 86400; s %= 86400;
    uint32_t h = s / 3600;  s %= 3600;
    uint32_t m = s / 60;    s %= 60;
    char buf[24];
    snprintf(buf, sizeof(buf), "%ud %02u:%02u:%02u",
             (unsigned)d, (unsigned)h, (unsigned)m, (unsigned)s);
    return String(buf);
}

// ============================================================
// Web-Server Setup
// ============================================================
void webServerSetup() {
    // HTML-Hauptseite
    webServer.on("/", HTTP_GET, [](AsyncWebServerRequest* req) {
        req->send(200, "text/html", HTML_PAGE);
    });

    // Captive-Portal-Redirects
    auto redirect = [](AsyncWebServerRequest* req) { req->redirect("/"); };
    webServer.on("/generate_204",          HTTP_GET, redirect);
    webServer.on("/hotspot-detect.html",   HTTP_GET, redirect);
    webServer.on("/connecttest.txt",       HTTP_GET, redirect);
    webServer.onNotFound(redirect);

    // GET /api/config
    webServer.on("/api/config", HTTP_GET, [](AsyncWebServerRequest* req) {
        JsonDocument doc;
        doc["sta_ssid"]         = cfg.sta_ssid;
        doc["sta_pass"]         = cfg.sta_pass;
        doc["ap_password"]      = cfg.ap_password;
        doc["wifi_timeout_sec"] = cfg.wifi_timeout_sec;
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
        doc["ota_enabled"]         = cfg.ota_enabled;
        doc["ota_password"]     = cfg.ota_password;
        doc["color_digit"]      = cfg.color_digit;
        doc["color_date"]       = cfg.color_date;
        doc["color_temp"]       = cfg.color_temp;
        doc["color_warn"]       = cfg.color_warn;
        doc["color_info"]       = cfg.color_info;
        doc["color_sec"]        = cfg.color_sec;
        doc["color_sec5"]       = cfg.color_sec5;
        doc["color_sec10"]      = cfg.color_sec10;
        String out; serializeJson(doc, out);
        req->send(200, "application/json", out);
    });

    // POST /api/config
    webServer.on("/api/config", HTTP_POST,
        [](AsyncWebServerRequest* req) {},
        nullptr,
        [](AsyncWebServerRequest* req, uint8_t* data, size_t len, size_t, size_t) {
            JsonDocument doc;
            if (deserializeJson(doc, data, len)) {
                req->send(400, "application/json", "{\"ok\":false,\"error\":\"JSON ungueltig\"}");
                return;
            }
            cfg.sta_ssid         = doc["sta_ssid"]         | cfg.sta_ssid.c_str();
            cfg.sta_pass         = doc["sta_pass"]         | cfg.sta_pass.c_str();
            cfg.ap_password      = doc["ap_password"]      | cfg.ap_password.c_str();
            cfg.wifi_timeout_sec = _clampU16(doc["wifi_timeout_sec"] | cfg.wifi_timeout_sec, 10, 120);
            cfg.ntp1             = doc["ntp1"]             | cfg.ntp1.c_str();
            cfg.ntp2             = doc["ntp2"]             | cfg.ntp2.c_str();
            cfg.ntp3             = doc["ntp3"]             | cfg.ntp3.c_str();
            cfg.ntp_interval_min = _clampU16(doc["ntp_interval_min"] | cfg.ntp_interval_min, 5, 1440);
            cfg.tz               = doc["tz"]               | cfg.tz.c_str();
            cfg.owm_apikey       = doc["owm_apikey"]       | cfg.owm_apikey.c_str();
            cfg.owm_city         = doc["owm_city"]         | cfg.owm_city.c_str();
            cfg.owm_lat          = doc["owm_lat"]          | cfg.owm_lat;
            cfg.owm_lon          = doc["owm_lon"]          | cfg.owm_lon;
            cfg.owm_use_coords   = doc["owm_use_coords"]   | cfg.owm_use_coords;
            cfg.owm_unit         = _clampU8(doc["owm_unit"] | cfg.owm_unit, 0, 1);
            cfg.owm_interval_min = _clampU16(doc["owm_interval_min"] | cfg.owm_interval_min, 10, 1440);
            cfg.brightness       = _clampU8(doc["brightness"] | cfg.brightness, 0, 255);
            cfg.lang             = doc["lang"]             | cfg.lang.c_str();
            cfg.bottom_cycle_sec = _clampU8(doc["bottom_cycle_sec"] | cfg.bottom_cycle_sec, 2, 60);
            cfg.scroll_speed_ms  = _clampU8(doc["scroll_speed_ms"] | cfg.scroll_speed_ms, 30, 200);
            cfg.time_12h         = _clampU8(doc["time_12h"] | cfg.time_12h, 0, 1);
            cfg.date_format      = _clampU8(doc["date_format"] | cfg.date_format, 0, 3);
            cfg.anim_mode        = _clampU8(doc["anim_mode"] | cfg.anim_mode, 0, 2);
            cfg.seconds_mode     = _clampU8(doc["seconds_mode"] | cfg.seconds_mode, 0, 3);
            cfg.forecast_scroll_up = doc["forecast_scroll_up"] | cfg.forecast_scroll_up;
            cfg.ota_enabled        = doc["ota_enabled"]        | cfg.ota_enabled;
            cfg.ota_password     = doc["ota_password"]     | cfg.ota_password.c_str();
            cfg.color_digit      = doc["color_digit"]      | cfg.color_digit.c_str();
            cfg.color_date       = doc["color_date"]       | cfg.color_date.c_str();
            cfg.color_temp       = doc["color_temp"]       | cfg.color_temp.c_str();
            cfg.color_warn       = doc["color_warn"]       | cfg.color_warn.c_str();
            cfg.color_info       = doc["color_info"]       | cfg.color_info.c_str();
            cfg.color_sec        = doc["color_sec"]        | cfg.color_sec.c_str();
            cfg.color_sec5       = doc["color_sec5"]       | cfg.color_sec5.c_str();
            cfg.color_sec10      = doc["color_sec10"]      | cfg.color_sec10.c_str();

            extern MatrixPanel_I2S_DMA* dma_display;
            displaySetBrightness(cfg.brightness);
            colorsInit();
            // Wetter-Cache invalidieren → nächster Loop-Durchlauf ruft neu ab
            // (betrifft Sprache, Standort, Einheit und API-Key-Änderungen)
            lastWeatherFetch = 0;
            configSave();
            req->send(200, "application/json", "{\"ok\":true}");
        }
    );

    // GET /api/status
    webServer.on("/api/status", HTTP_GET, [](AsyncWebServerRequest* req) {
        JsonDocument doc;
        uint32_t nowMs = millis();

        doc["firmware"] = FW_VERSION;
        doc["ip"]       = apModeActive
                          ? WiFi.softAPIP().toString()
                          : WiFi.localIP().toString();
        doc["ssid"]     = apModeActive ? "(AP-Modus)" : WiFi.SSID();
        doc["rssi"]     = apModeActive ? 0 : (int)WiFi.RSSI();
        doc["uptime"]   = _uptime();
        doc["heap"]     = ESP.getFreeHeap();

        time_t now = time(nullptr);
        char tbuf[24];
        strftime(tbuf, sizeof(tbuf), "%d.%m.%Y %H:%M:%S", localtime(&now));
        doc["time"] = tbuf;

        doc["ntp_status"]  = ntpSynced ? "Synchronisiert" : "Nicht synchronisiert";
        doc["ntp_servers"] = cfg.ntp1 + " / " + cfg.ntp2 + " / " + cfg.ntp3;

        // Aktiven NTP-Server ermitteln (erster Server mit Reachability != 0)
        const char* ntpCfg[3] = { cfg.ntp1.c_str(), cfg.ntp2.c_str(), cfg.ntp3.c_str() };
        String activeNtp = "–";
        for (int i = 0; i < 3; i++) {
            if (sntp_getreachability(i) != 0) {
                activeNtp = String(ntpCfg[i]);
                break;
            }
        }
        doc["ntp_active"] = activeNtp;

        // Nächster NTP-Sync (in Sekunden)
        long ntpIntervalMs = (long)cfg.ntp_interval_min * 60000L;
        long nextNtpMs     = ntpIntervalMs - (long)(nowMs - lastNtpSync);
        doc["next_ntp_sec"] = (int)(nextNtpMs / 1000);

        // Wetter-Standort (zur Diagnose)
        if (cfg.owm_apikey.isEmpty()) {
            doc["weather_loc"] = "Kein API-Key";
        } else if (cfg.owm_use_coords) {
            char locBuf[32];
            snprintf(locBuf, sizeof(locBuf), "%.4f, %.4f", cfg.owm_lat, cfg.owm_lon);
            doc["weather_loc"] = locBuf;
        } else {
            doc["weather_loc"] = cfg.owm_city.isEmpty() ? "–" : cfg.owm_city;
        }

        // Wetter-Status
        if (weather.valid) {
            char wb[48];
            snprintf(wb, sizeof(wb), "%.1f° — %s",
                     weather.current_temp, weather.current_cond.c_str());
            doc["weather_status"] = wb;
        } else {
            doc["weather_status"] = weather.error.isEmpty()
                                    ? "Nicht verfügbar" : weather.error;
        }

        // Nächste Wetter-Aktualisierung (in Sekunden)
        long wxIntervalMs = (long)cfg.owm_interval_min * 60000L;
        long nextWxMs     = (lastWeatherFetch == 0)
                            ? 0
                            : wxIntervalMs - (long)(nowMs - lastWeatherFetch);
        doc["next_weather_sec"] = (int)(nextWxMs / 1000);

        String out; serializeJson(doc, out);
        req->send(200, "application/json", out);
    });

    // GET /api/geocode?q={city}&apikey={key}
    // Ruft OWM Geocoding API auf und liefert bis zu 5 Treffer zurück
    webServer.on("/api/geocode", HTTP_GET, [](AsyncWebServerRequest* req) {
        if (!req->hasParam("q") || !req->hasParam("apikey")) {
            req->send(400, "application/json", "{\"error\":\"Parameter q und apikey erforderlich\"}");
            return;
        }
        String city   = req->getParam("q")->value();
        String apikey = req->getParam("apikey")->value();
        city.replace(" ", "+");

        String url = "http://api.openweathermap.org/geo/1.0/direct?q="
                   + city + "&limit=5&appid=" + apikey;
        HttpResult res = _httpGet(url);
        if (res.code == 200) {
            req->send(200, "application/json", res.body);
        } else if (res.code == 404) {
            req->send(404, "application/json", "{\"error\":\"Keine Treffer\"}");
        } else {
            String err = "{\"error\":\"" + (res.error.isEmpty() ? String("OWM Fehler") : res.error) + "\"}";
            req->send(502, "application/json", err);
        }
    });

    // GET /api/reset
    webServer.on("/api/reset", HTTP_GET, [](AsyncWebServerRequest* req) {
        configReset();
        req->send(200, "application/json", "{\"ok\":true}");
        factoryResetPending = true;
    });

    // GET /api/restart — setzt Flag, main loop führt restart sicher durch
    webServer.on("/api/restart", HTTP_GET, [](AsyncWebServerRequest* req) {
        req->send(200, "application/json", "{\"ok\":true}");
        restartPending = true;
    });

    webServer.begin();
}

// ============================================================
// OTA Setup
// ============================================================
void otaSetup() {
    ArduinoOTA.setHostname("hub75clock");
    if (!cfg.ota_password.isEmpty()) {
        ArduinoOTA.setPassword(cfg.ota_password.c_str());
    }
    ArduinoOTA.onStart([]() {
        otaRunning = true;
        displaySetOtaActive(true);
        displayShowOTA(0, 1);
    });
    ArduinoOTA.onProgress([](unsigned int progress, unsigned int total) {
        displayShowOTA((uint32_t)progress, (uint32_t)total);
    });
    ArduinoOTA.onEnd([]()   { otaRunning = false; });
    ArduinoOTA.onError([](ota_error_t) {
        otaRunning = false;
        displaySetOtaActive(false);
    });
    ArduinoOTA.begin();
}
