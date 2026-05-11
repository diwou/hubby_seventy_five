#pragma once
#include <Arduino.h>
#include <WiFi.h>
#include <ESPAsyncWebServer.h>
#include <DNSServer.h>
#include <HTTPClient.h>
#include <ArduinoJson.h>
#include <ArduinoOTA.h>
#include <Update.h>
#include <LittleFS.h>
#include <math.h>
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
static bool     webUpdateOk   = false;
static String   webUpdateError = "";
static constexpr uint16_t OTA_PORT = 3232;

// Externe Variablen (definiert in main.cpp / display.h)
extern bool     ntpSynced;
extern uint32_t lastNtpSync;
extern uint32_t lastWeatherFetch;
extern String   lastStatusMsg;
extern char     rtcLastWatchdog[160];
extern char     rtcLastWatchdogPhase[32];
extern uint32_t rtcLastWatchdogAgeMs;
extern uint32_t rtcLastWatchdogEpoch;
extern bool     i2cDs3231Present;
extern bool     i2cVEML7700Present;
extern bool     i2cBme680Present;
extern String   i2cStatusSummary;
extern String   i2cBusScanSummary;
extern int      i2cBme680Address;
extern int      i2cBme680ChipId;
extern bool     rtcTimeValid;
extern String   rtcTimeSource;
extern String   rtcTimeSourceDetail;
extern uint32_t lastRtcReadMs;
extern uint32_t lastRtcWriteMs;
extern int      i2cDs3231ProbeCode;
extern int      i2cVeml7700ProbeCode;
extern String   i2cRtcDiag;
extern time_t   lastNtpSyncEpoch;
extern bool     vemlOk;
extern String   vemlDiag;
extern float    luxCurrent;
extern float    luxFilt;
extern float    luxHold;
extern bool     bmeOk;
extern String   bmeDiag;
extern float    bmeTempC;
extern float    bmeHumidityPct;
extern float    bmePressureHpa;
extern float    bmeGasKOhm;
extern uint32_t lastBmeReadMs;
bool rtcGetEpoch(time_t& outEpoch);
bool rtcSetEpoch(time_t epoch);

// Forward-Deklarationen (definiert in display.h)
void displayShowOTA(uint32_t progress, uint32_t total);
void displaySetOtaActive(bool active);
void displayInvalidateStatic();
void displayResetWeatherLine();
void colorsInit();
void displaySetBrightness(uint8_t brightness);
const char* resetReasonName(esp_reset_reason_t reason);
int displayWidth();
int displayHeight();
uint8_t displayPanelColumns();
uint8_t displayPanelRows();
uint8_t displayPanelMapping();

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

static bool _webAuthOk(AsyncWebServerRequest* req) {
    return req->authenticate("admin", cfg.web_password.c_str(), "HUB75 Clock", false);
}

static bool _requireWebAuth(AsyncWebServerRequest* req) {
    if (_webAuthOk(req)) return true;
    req->requestAuthentication("HUB75 Clock", false);
    return false;
}

static bool _jsonHasString(JsonDocument& doc, const char* key) {
    return doc[key].is<const char*>();
}

static String _jsonString(JsonDocument& doc, const char* key) {
    return doc[key].as<String>();
}

static void _sendJsonError(AsyncWebServerRequest* req, int code, const char* msg) {
    String out = "{\"ok\":false,\"error\":\"";
    out += msg;
    out += "\"}";
    req->send(code, "application/json", out);
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
  <div class="tab" onclick="show('rtc',this)">RTC</div>
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

<!-- RTC -->
<div id="p-rtc" class="panel">
  <p style="font-size:.85em;color:#aaa">DS3231 ist Laufzeit-Zeitquelle; NTP korrigiert die RTC.</p>
  <div class="row">
    <label>Datum <input id="rtc_date" type="date"></label>
    <label>Zeit <input id="rtc_time" type="time" step="1"></label>
  </div>
  <button class="btn btn-save" onclick="rtcSetNow()">RTC setzen</button>
  <hr style="border-color:#333;margin:12px 0">
  <div class="status-box" id="rtcStatusBox">Lade...</div>
</div>

<!-- WLAN -->
<div id="p-wifi" class="panel">
  <label>SSID <input id="sta_ssid" maxlength="32"></label>
  <label>Passwort <input id="sta_pass" type="password" maxlength="64"></label>
  <label>AP-Passwort (leer = offener AP, sonst mindestens 8 Zeichen) <input id="ap_password" type="password" minlength="8" maxlength="32" placeholder="unverändert"></label>
  <label>Web-Passwort (Benutzer: admin, mindestens 8 Zeichen) <input id="web_password" type="password" minlength="8" maxlength="32" placeholder="unverändert"></label>
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
  <hr style="border-color:#333;margin:12px 0">
  <div class="status-box" id="bmeStatusBox">Lade...</div>
</div>

<!-- ANZEIGE -->
<div id="p-display" class="panel">
  <label>Helligkeit (0-255) <input id="brightness" type="number" min="0" max="255"></label>
  <label>HUB75-Layout
    <select id="display_panel_cols">
      <option value="1">1x HUB75 (64x32)</option>
      <option value="2">2x HUB75 nebeneinander (128x32)</option>
    </select>
  </label>
  <p style="font-size:.78em;color:#888;margin-top:4px">Aenderungen am Panel-Layout werden nach einem Neustart aktiv. Intern ist die Konfiguration bereits fuer spaeteres 2x2-/ZigZag-Mapping vorbereitet.</p>
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
  <label><input type="checkbox" id="auto_brightness"> Auto-Helligkeit per VEML7700</label>
  <hr style="border-color:#333;margin:12px 0">
  <div class="status-box" id="vemlStatusBox">Lade...</div>
  <hr style="border-color:#333;margin:12px 0">
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
  <hr style="border-color:#333;margin:12px 0">
  <label>Firmware-Datei (`.bin`) vom Release oder lokalen Build auswählen
    <input id="fw_file" type="file" accept=".bin,application/octet-stream">
  </label>
  <button class="btn btn-info" onclick="uploadFirmware()">Firmware jetzt einspielen</button>
  <p id="otaUploadStatus" style="margin-top:8px;font-size:.85em;color:#aaa"></p>
  <p style="margin-top:10px;font-size:.8em;color:#888">OTA-Hostname: hub75clock.local &nbsp;|&nbsp; Port: 3232</p>
</div>

<div id="msg"></div>

<script>
const fields=['sta_ssid','sta_pass','ap_password','web_password','wifi_timeout_sec','ntp1','ntp2','ntp3',
  'ntp_interval_min','tz','owm_apikey','owm_city','owm_lat','owm_lon','owm_use_coords',
  'owm_unit','owm_interval_min','brightness','lang','time_12h','date_format',
  'anim_mode','seconds_mode','display_panel_cols','bottom_cycle_sec','scroll_speed_ms','auto_brightness','forecast_scroll_up',
  'ota_enabled','ota_password',
  'color_digit','color_date','color_temp','color_warn','color_info',
  'color_sec','color_sec5','color_sec10'];
// Select-Felder mit numerischen Werten (müssen als Int gespeichert werden)
const numSelects=new Set(['owm_unit','time_12h','date_format','anim_mode','seconds_mode','display_panel_cols']);
const secretFields=new Set(['sta_pass','ap_password','web_password','owm_apikey','ota_password']);

let statusInterval=null;
let statusLoading=false;

function show(id,el){
  document.querySelectorAll('.panel').forEach(p=>p.classList.remove('active'));
  document.querySelectorAll('.tab').forEach(t=>t.classList.remove('active'));
  document.getElementById('p-'+id).classList.add('active');
  el.classList.add('active');
  loadStatus();
  updateStatusTimerHint();
}

function startStatusTimer(){
  if(statusInterval)return;
  statusInterval=setInterval(()=>{loadStatus();},1000);
  updateStatusTimerHint();
}
function updateStatusTimerHint(){
  const t=document.getElementById('statusTimer');
  const statusVisible=document.getElementById('p-status').classList.contains('active');
  t.textContent=statusVisible?'Aktualisierung jede Sekunde':'';
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

async function rtcSetNow(){
  const dateEl=document.getElementById('rtc_date');
  const timeEl=document.getElementById('rtc_time');
  const dateVal=(dateEl&&dateEl.value)||'';
  const timeVal=(timeEl&&timeEl.value)||'';
  if(!dateVal||!timeVal){showMsg('Bitte Datum und Zeit setzen',false);return;}
  const body=JSON.stringify({date:dateVal,time:timeVal});
  const r=await fetch('/api/rtcset',{method:'POST',headers:{'Content-Type':'application/json'},body});
  const j=await r.json();
  showMsg(j.ok?'RTC gesetzt':'Fehler: '+(j.error||'?'),!!j.ok);
  if(j.ok)loadStatus();
}

function fmtSec(s){
  if(s<0)return'jetzt';
  if(s<60)return s+'s';
  return Math.floor(s/60)+'min '+( s%60)+'s';
}

function setStatusRows(boxId,rows){
  const box=document.getElementById(boxId);
  if(!box)return;
  box.textContent='';
  rows.forEach(([label,value])=>{
    const div=document.createElement('div');
    const lbl=document.createElement('span');
    const val=document.createElement('span');
    lbl.className='lbl';
    val.className='val';
    lbl.textContent=label;
    val.textContent=value;
    div.appendChild(lbl);
    div.appendChild(val);
    box.appendChild(div);
  });
}

async function loadConfig(){
  const r=await fetch('/api/config');
  const d=await r.json();
  fields.forEach(f=>{
    const el=document.getElementById(f);if(!el)return;
    if(el.type==='checkbox')el.checked=!!d[f];
    else if(el.tagName==='SELECT')el.value=String(d[f]??'');
    else el.value=d[f]??'';
    if(secretFields.has(f)){
      const configured=!!d[f+'_configured'];
      el.placeholder=configured?'(gespeichert)':'';
    }
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
    else if(f==='ota_password')d[f]=el.value;
    else if(secretFields.has(f)&&el.value==='')return;
    else d[f]=el.value;
  });
  d.display_panel_rows=1;
  const r=await fetch('/api/config',{method:'POST',headers:{'Content-Type':'application/json'},body:JSON.stringify(d)});
  const j=await r.json();
  showMsg(j.ok?(j.restart_required?'Gespeichert! Neustart erforderlich, damit das Display-Layout aktiv wird.':'Gespeichert!'):'Fehler: '+(j.error||'?'),j.ok);
}

async function loadStatus(){
  if(statusLoading)return;
  statusLoading=true;
  try{
    const r=await fetch('/api/status');
    const d=await r.json();
    const verEl=document.getElementById('fwVer');
    if(verEl)verEl.textContent='v'+d.firmware.replace(/^.*\s/,'');
    setStatusRows('statusBox',[
      ['Firmware',d.firmware||'-'],
      ['Display',d.display_layout||'-'],
      ['IP / mDNS',(d.ip||'-')+' / hub75clock.local'],
      ['WLAN',(d.ssid||'-')+' ('+(d.rssi??'-')+' dBm)'],
      ['Uptime',d.uptime||'-'],
      ['Uhrzeit',d.time||'-'],
      ['Meldung',d.system_msg||'-'],
      ['Letzter Watchdog',d.last_watchdog||'-'],
      ['Watchdog-Zeitpunkt',d.watchdog_time||'-'],
      ['Watchdog-Phase',d.watchdog_phase||'-'],
      ['Watchdog-Zeit',fmtSec(Math.floor((d.watchdog_age_ms||0)/1000))],
      ['Boot-Grund',d.boot_reason||'-'],
      ['OTA aktiviert',d.ota_enabled?'Ja':'Nein'],
      ['OTA-Passwort',d.ota_password_set?'gesetzt':'leer'],
      ['NTP-Status',d.ntp_status||'-'],
      ['Aktiver NTP-Server',d.ntp_active||'-'],
      ['Nächster NTP-Sync',fmtSec(d.next_ntp_sec||0)],
      ['I2C-Status',d.i2c_status||'-'],
      ['I2C-Scan',d.i2c_scan||'-'],
      ['Wetter-Standort',d.weather_loc||'-'],
      ['Wetter',d.weather_status||'-'],
      ['Nächstes Wetter',fmtSec(d.next_weather_sec||0)],
      ['Freier Heap',(d.heap??'-')+' Byte']
    ]);

    setStatusRows('rtcStatusBox',[
      ['RTC vorhanden',d.rtc_present?'ja':'nein'],
      ['RTC gültig',d.rtc_valid?'ja':'nein'],
      ['I2C',d.i2c_rtc_diag||'-'],
      ['DS3231',d.i2c_ds3231||'-'],
      ['Zeitquelle',d.time_source||'-'],
      ['Info',d.time_source_detail||'-'],
      ['RTC-Datum',d.rtc_date||'-'],
      ['RTC gelesen vor',fmtSec(Math.floor((d.rtc_read_age_ms||0)/1000))],
      ['RTC geschrieben vor',fmtSec(Math.floor((d.rtc_write_age_ms||0)/1000))],
      ['RTC Zeit',d.rtc_time||'-'],
      ['Letzter NTP-Sync',d.ntp_last_sync||'-'],
      ['NTP-Server',d.ntp_servers||'-']
    ]);

    setStatusRows('vemlStatusBox',[
      ['VEML7700',d.i2c_veml7700||'-'],
      ['VEML aktiv',d.veml_ok?'ja':'nein'],
      ['VEML-Diag',d.veml_diag||'-'],
      ['Auto-Helligkeit',d.auto_brightness?'an':'aus'],
      ['Max-Helligkeit',d.brightness_max??'-'],
      ['Lux aktuell',d.veml_lux_current||'-'],
      ['Lux gefiltert',d.veml_lux_filt||'-'],
      ['Lux gehalten',d.veml_lux_hold||'-'],
      ['BME680',d.i2c_bme680||'-'],
      ['BME680-Adresse',d.i2c_bme680_addr||'-'],
      ['BME680-Chip-ID',d.i2c_bme680_chipid||'-']
    ]);

    setStatusRows('bmeStatusBox',[
      ['BME680 erkannt',d.i2c_bme680||'-'],
      ['BME680 aktiv',d.bme_ok?'ja':'nein'],
      ['BME680-Diag',d.bme_diag||'-'],
      ['BME680-Adresse',d.i2c_bme680_addr||'-'],
      ['BME680-Chip-ID',d.i2c_bme680_chipid||'-'],
      ['Temperatur',d.bme_temp_c||'--'],
      ['Luftfeuchte',d.bme_humidity_pct||'--'],
      ['Luftdruck',d.bme_pressure_hpa||'--'],
      ['Gaswiderstand',d.bme_gas_k_ohm||'--'],
      ['Letzte BME-Messung',fmtSec(Math.floor((d.bme_read_age_ms||0)/1000))]
    ]);
  }catch(e){
    document.getElementById('statusBox').textContent='Fehler beim Laden';
  }finally{
    statusLoading=false;
  }
}

async function searchCity(){
  const city=document.getElementById('owm_city').value.trim();
  if(!city){showMsg('Bitte Stadt eingeben',false);return;}
  const btn=event.target;
  btn.disabled=true;btn.textContent='...';
  try{
    const r=await fetch('/api/geocode?q='+encodeURIComponent(city));
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

async function uploadFirmware(){
  const fileEl=document.getElementById('fw_file');
  const statusEl=document.getElementById('otaUploadStatus');
  const file=fileEl.files&&fileEl.files[0];
  if(!file){showMsg('Bitte zuerst eine Firmware-Datei (.bin) auswaehlen',false);return;}
  if(!confirm('Firmware wirklich hochladen und das Geraet danach neu starten?'))return;

  const form=new FormData();
  form.append('firmware',file,file.name);
  statusEl.textContent='Upload laeuft...';

  try{
    const r=await fetch('/api/update',{method:'POST',body:form});
    const j=await r.json();
    if(j.ok){
      statusEl.textContent='Upload erfolgreich. Neustart laeuft...';
      showMsg('Firmware eingespielt. Geraet startet neu...',true);
      setTimeout(()=>location.reload(),8000);
    }else{
      statusEl.textContent='Upload fehlgeschlagen';
      showMsg('Firmware-Update fehlgeschlagen: '+(j.error||'?'),false);
    }
  }catch(e){
    statusEl.textContent='Upload fehlgeschlagen';
    showMsg('Firmware-Upload fehlgeschlagen',false);
  }
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
        if (!_requireWebAuth(req)) return;
        JsonDocument doc;
        doc["sta_ssid"]         = cfg.sta_ssid;
        doc["sta_pass"]         = "";
        doc["sta_pass_configured"] = !cfg.sta_pass.isEmpty();
        doc["ap_password"]      = "";
        doc["ap_password_configured"] = !cfg.ap_password.isEmpty();
        doc["web_password"]     = "";
        doc["web_password_configured"] = !cfg.web_password.isEmpty();
        doc["wifi_timeout_sec"] = cfg.wifi_timeout_sec;
        doc["ntp1"]             = cfg.ntp1;
        doc["ntp2"]             = cfg.ntp2;
        doc["ntp3"]             = cfg.ntp3;
        doc["ntp_interval_min"] = cfg.ntp_interval_min;
        doc["tz"]               = cfg.tz;
        doc["owm_apikey"]       = "";
        doc["owm_apikey_configured"] = !cfg.owm_apikey.isEmpty();
        doc["owm_city"]         = cfg.owm_city;
        doc["owm_lat"]          = cfg.owm_lat;
        doc["owm_lon"]          = cfg.owm_lon;
        doc["owm_use_coords"]   = cfg.owm_use_coords;
        doc["owm_unit"]         = cfg.owm_unit;
        doc["owm_interval_min"] = cfg.owm_interval_min;
        doc["brightness"]       = cfg.brightness;
        doc["auto_brightness"]  = cfg.auto_brightness;
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
        doc["ota_password"]     = "";
        doc["ota_password_configured"] = !cfg.ota_password.isEmpty();
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
            if (!_requireWebAuth(req)) return;
            JsonDocument doc;
            if (deserializeJson(doc, data, len)) {
                req->send(400, "application/json", "{\"ok\":false,\"error\":\"JSON ungueltig\"}");
                return;
            }
            if (_jsonHasString(doc, "ap_password")) {
                String newApPassword = _jsonString(doc, "ap_password");
                if (!newApPassword.isEmpty() && newApPassword.length() < 8) {
                    _sendJsonError(req, 400, "AP-Passwort muss mindestens 8 Zeichen haben");
                    return;
                }
            }
            if (_jsonHasString(doc, "web_password")) {
                String newWebPassword = _jsonString(doc, "web_password");
                if (!newWebPassword.isEmpty() && newWebPassword.length() < 8) {
                    _sendJsonError(req, 400, "Web-Passwort muss mindestens 8 Zeichen haben");
                    return;
                }
            }
            bool oldOtaEnabled = cfg.ota_enabled;
            String oldOtaPassword = cfg.ota_password;

            cfg.sta_ssid         = doc["sta_ssid"]         | cfg.sta_ssid.c_str();
            if (_jsonHasString(doc, "sta_pass")) {
                String newPass = _jsonString(doc, "sta_pass");
                if (!newPass.isEmpty()) cfg.sta_pass = newPass;
            }
            if (_jsonHasString(doc, "ap_password")) {
                String newApPassword = _jsonString(doc, "ap_password");
                if (!newApPassword.isEmpty()) cfg.ap_password = newApPassword;
            }
            if (_jsonHasString(doc, "web_password")) {
                String newWebPassword = _jsonString(doc, "web_password");
                if (!newWebPassword.isEmpty()) cfg.web_password = newWebPassword;
            }
            cfg.wifi_timeout_sec = _clampU16(doc["wifi_timeout_sec"] | cfg.wifi_timeout_sec, 10, 120);
            cfg.ntp1             = doc["ntp1"]             | cfg.ntp1.c_str();
            cfg.ntp2             = doc["ntp2"]             | cfg.ntp2.c_str();
            cfg.ntp3             = doc["ntp3"]             | cfg.ntp3.c_str();
            cfg.ntp_interval_min = _clampU16(doc["ntp_interval_min"] | cfg.ntp_interval_min, 5, 1440);
            cfg.tz               = doc["tz"]               | cfg.tz.c_str();
            if (_jsonHasString(doc, "owm_apikey")) {
                String newApiKey = _jsonString(doc, "owm_apikey");
                if (!newApiKey.isEmpty()) cfg.owm_apikey = newApiKey;
            }
            cfg.owm_city         = doc["owm_city"]         | cfg.owm_city.c_str();
            cfg.owm_lat          = doc["owm_lat"]          | cfg.owm_lat;
            cfg.owm_lon          = doc["owm_lon"]          | cfg.owm_lon;
            cfg.owm_use_coords   = doc["owm_use_coords"]   | cfg.owm_use_coords;
            cfg.owm_unit         = _clampU8(doc["owm_unit"] | cfg.owm_unit, 0, 1);
            cfg.owm_interval_min = _clampU16(doc["owm_interval_min"] | cfg.owm_interval_min, 10, 1440);
            uint8_t oldPanelCols = cfg.display_panel_cols;
            uint8_t oldPanelRows = cfg.display_panel_rows;
            uint8_t oldPanelMap  = cfg.display_panel_map;

            cfg.brightness       = _clampU8(doc["brightness"] | cfg.brightness, 0, 255);
            cfg.auto_brightness  = doc["auto_brightness"] | cfg.auto_brightness;
            cfg.lang             = doc["lang"]             | cfg.lang.c_str();
            cfg.bottom_cycle_sec = _clampU8(doc["bottom_cycle_sec"] | cfg.bottom_cycle_sec, 2, 60);
            cfg.scroll_speed_ms  = _clampU8(doc["scroll_speed_ms"] | cfg.scroll_speed_ms, 30, 200);
            cfg.time_12h         = _clampU8(doc["time_12h"] | cfg.time_12h, 0, 1);
            cfg.date_format      = _clampU8(doc["date_format"] | cfg.date_format, 0, 3);
            cfg.anim_mode        = _clampU8(doc["anim_mode"] | cfg.anim_mode, 0, 2);
            cfg.seconds_mode     = _clampU8(doc["seconds_mode"] | cfg.seconds_mode, 0, 3);
            cfg.forecast_scroll_up = doc["forecast_scroll_up"] | cfg.forecast_scroll_up;
            cfg.display_panel_cols = _clampU8(doc["display_panel_cols"] | cfg.display_panel_cols, 1, 2);
            cfg.display_panel_rows = _clampU8(doc["display_panel_rows"] | cfg.display_panel_rows, 1, 1);
            cfg.display_panel_map  = _clampU8(doc["display_panel_map"]  | cfg.display_panel_map,  0, 1);
            cfg.ota_enabled        = doc["ota_enabled"]        | cfg.ota_enabled;
            if (_jsonHasString(doc, "ota_password")) {
                cfg.ota_password = _jsonString(doc, "ota_password");
            }
            cfg.color_digit      = doc["color_digit"]      | cfg.color_digit.c_str();
            cfg.color_date       = doc["color_date"]       | cfg.color_date.c_str();
            cfg.color_temp       = doc["color_temp"]       | cfg.color_temp.c_str();
            cfg.color_warn       = doc["color_warn"]       | cfg.color_warn.c_str();
            cfg.color_info       = doc["color_info"]       | cfg.color_info.c_str();
            cfg.color_sec        = doc["color_sec"]        | cfg.color_sec.c_str();
            cfg.color_sec5       = doc["color_sec5"]       | cfg.color_sec5.c_str();
            cfg.color_sec10      = doc["color_sec10"]      | cfg.color_sec10.c_str();
            configSanitizeColors();

            displaySetBrightness(cfg.brightness);
            colorsInit();
            displayInvalidateStatic();
            displayResetWeatherLine();
            // Wetter-Cache invalidieren → nächster Loop-Durchlauf ruft neu ab
            // (betrifft Sprache, Standort, Einheit und API-Key-Änderungen)
            lastWeatherFetch = 0;
            if (!configSave()) {
                _sendJsonError(req, 500, "Konfiguration konnte nicht gespeichert werden");
                return;
            }

            bool restartRequired = (oldPanelCols != cfg.display_panel_cols)
                                || (oldPanelRows != cfg.display_panel_rows)
                                || (oldPanelMap  != cfg.display_panel_map)
                                || (oldOtaEnabled != cfg.ota_enabled)
                                || (oldOtaPassword != cfg.ota_password);
            String out;
            JsonDocument resp;
            resp["ok"] = true;
            resp["restart_required"] = restartRequired;
            serializeJson(resp, out);
            req->send(200, "application/json", out);
        }
    );

    // GET /api/status
    webServer.on("/api/status", HTTP_GET, [](AsyncWebServerRequest* req) {
        if (!_requireWebAuth(req)) return;
        JsonDocument doc;
        uint32_t nowMs = millis();

        doc["firmware"] = String("HUB75 Clock ") + FW_VERSION;
        doc["display_layout"] = String(displayPanelColumns()) + "x" + String(displayPanelRows())
                              + " HUB75 (" + String(displayWidth()) + "x" + String(displayHeight())
                              + ", " + (displayPanelMapping() == 0 ? "linear" : "zigzag") + ")";
        doc["ip"]       = apModeActive
                          ? WiFi.softAPIP().toString()
                          : WiFi.localIP().toString();
        doc["ssid"]     = apModeActive ? "(AP-Modus)" : WiFi.SSID();
        doc["rssi"]     = apModeActive ? 0 : (int)WiFi.RSSI();
        doc["uptime"]   = _uptime();
        doc["heap"]     = ESP.getFreeHeap();
        doc["system_msg"] = lastStatusMsg;
        doc["last_watchdog"] = rtcLastWatchdog[0] ? rtcLastWatchdog : "kein Watchdog-Hänger gespeichert";
        if (rtcLastWatchdogEpoch > 0) {
            time_t wd = (time_t)rtcLastWatchdogEpoch;
            char watchdogTimeBuf[24];
            strftime(watchdogTimeBuf, sizeof(watchdogTimeBuf), "%d.%m.%Y %H:%M:%S", localtime(&wd));
            doc["watchdog_time"] = watchdogTimeBuf;
        } else {
            doc["watchdog_time"] = "-";
        }
        doc["boot_reason"] = resetReasonName(esp_reset_reason());
        doc["watchdog_phase"] = rtcLastWatchdogPhase[0] ? rtcLastWatchdogPhase : "-";
        doc["watchdog_age_ms"] = rtcLastWatchdogAgeMs;
        doc["ota_enabled"] = cfg.ota_enabled;
        doc["ota_password_set"] = !cfg.ota_password.isEmpty();
        doc["brightness_max"] = cfg.brightness;
        doc["rtc_present"] = i2cDs3231Present;
        doc["time_source"] = rtcTimeSource;
        doc["time_source_detail"] = rtcTimeSourceDetail;
        doc["rtc_valid"] = rtcTimeValid;
        doc["i2c_rtc_diag"] = i2cRtcDiag;
        doc["rtc_read_age_ms"] = lastRtcReadMs ? (nowMs - lastRtcReadMs) : 0;
        doc["rtc_write_age_ms"] = lastRtcWriteMs ? (nowMs - lastRtcWriteMs) : 0;

        time_t now = time(nullptr);
        char tbuf[24];
        strftime(tbuf, sizeof(tbuf), "%d.%m.%Y %H:%M:%S", localtime(&now));
        doc["time"] = tbuf;
        time_t rtcEpoch = 0;
        if (rtcGetEpoch(rtcEpoch)) {
            char rtcDateBuf[16];
            char rtcTimeBuf[24];
            strftime(rtcDateBuf, sizeof(rtcDateBuf), "%d.%m.%Y", localtime(&rtcEpoch));
            strftime(rtcTimeBuf, sizeof(rtcTimeBuf), "%d.%m.%Y %H:%M:%S", localtime(&rtcEpoch));
            doc["rtc_date"] = rtcDateBuf;
            doc["rtc_time"] = rtcTimeBuf;
        } else {
            doc["rtc_date"] = "-";
            doc["rtc_time"] = "-";
        }
        if (lastNtpSyncEpoch > 0) {
            char ntpBuf[24];
            strftime(ntpBuf, sizeof(ntpBuf), "%d.%m.%Y %H:%M:%S", localtime(&lastNtpSyncEpoch));
            doc["ntp_last_sync"] = ntpBuf;
        } else {
            doc["ntp_last_sync"] = "-";
        }

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
        doc["i2c_status"] = i2cStatusSummary;
        doc["i2c_scan"] = i2cBusScanSummary;
        doc["i2c_ds3231"] = i2cDs3231Present ? "OK" : "fehlt";
        doc["i2c_veml7700"] = i2cVEML7700Present ? "OK" : "fehlt";
        doc["veml_ok"] = vemlOk;
        doc["veml_diag"] = vemlDiag;
        doc["auto_brightness"] = cfg.auto_brightness;
        if (isfinite(luxCurrent)) doc["veml_lux_current"] = String(luxCurrent, 1);
        else doc["veml_lux_current"] = "--";
        if (isfinite(luxFilt)) doc["veml_lux_filt"] = String(luxFilt, 1);
        else doc["veml_lux_filt"] = "--";
        if (isfinite(luxHold)) doc["veml_lux_hold"] = String(luxHold, 1);
        else doc["veml_lux_hold"] = "--";
        doc["i2c_bme680"] = i2cBme680Present ? "OK" : "fehlt";
        if (i2cBme680Address >= 0) {
            char addrBuf[8];
            snprintf(addrBuf, sizeof(addrBuf), "0x%02X", i2cBme680Address);
            doc["i2c_bme680_addr"] = addrBuf;
        } else {
            doc["i2c_bme680_addr"] = "--";
        }
        if (i2cBme680ChipId >= 0) {
            char idBuf[8];
            snprintf(idBuf, sizeof(idBuf), "0x%02X", i2cBme680ChipId);
            doc["i2c_bme680_chipid"] = idBuf;
        } else {
            doc["i2c_bme680_chipid"] = "--";
        }
        doc["bme_ok"] = bmeOk;
        doc["bme_diag"] = bmeDiag;
        if (isfinite(bmeTempC)) doc["bme_temp_c"] = String(bmeTempC, 1) + " C";
        else doc["bme_temp_c"] = "--";
        if (isfinite(bmeHumidityPct)) doc["bme_humidity_pct"] = String(bmeHumidityPct, 1) + " %";
        else doc["bme_humidity_pct"] = "--";
        if (isfinite(bmePressureHpa)) doc["bme_pressure_hpa"] = String(bmePressureHpa, 1) + " hPa";
        else doc["bme_pressure_hpa"] = "--";
        if (isfinite(bmeGasKOhm)) doc["bme_gas_k_ohm"] = String(bmeGasKOhm, 1) + " kOhm";
        else doc["bme_gas_k_ohm"] = "--";
        doc["bme_read_age_ms"] = lastBmeReadMs ? (nowMs - lastBmeReadMs) : 0;

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
        if (weatherFetchActive) {
            doc["weather_status"] = "Abruf laeuft";
        } else if (weather.valid) {
            char wb[48];
            snprintf(wb, sizeof(wb), "%.1f° — %s",
                     weather.current_temp, weatherDisplayTextToUtf8(weather.current_cond).c_str());
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

    // GET /api/geocode?q={city}
    // Ruft OWM Geocoding API auf und liefert bis zu 5 Treffer zurück
    webServer.on("/api/geocode", HTTP_GET, [](AsyncWebServerRequest* req) {
        if (!_requireWebAuth(req)) return;
        if (!req->hasParam("q")) {
            req->send(400, "application/json", "{\"error\":\"Parameter q erforderlich\"}");
            return;
        }
        String city   = req->getParam("q")->value();
        if (cfg.owm_apikey.isEmpty()) {
            req->send(400, "application/json", "{\"error\":\"Kein API-Key konfiguriert\"}");
            return;
        }
        city.replace(" ", "+");

        String url = "https://api.openweathermap.org/geo/1.0/direct?q="
                   + city + "&limit=5&appid=" + cfg.owm_apikey;
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
        if (!_requireWebAuth(req)) return;
        configReset();
        req->send(200, "application/json", "{\"ok\":true}");
        factoryResetPending = true;
    });

    // GET /api/restart — setzt Flag, main loop führt restart sicher durch
    webServer.on("/api/restart", HTTP_GET, [](AsyncWebServerRequest* req) {
        if (!_requireWebAuth(req)) return;
        req->send(200, "application/json", "{\"ok\":true}");
        restartPending = true;
    });

    // POST /api/rtcset {"date":"YYYY-MM-DD","time":"HH:MM[:SS]"}
    webServer.on("/api/rtcset", HTTP_POST,
        [](AsyncWebServerRequest* req) {},
        nullptr,
        [](AsyncWebServerRequest* req, uint8_t* data, size_t len, size_t, size_t) {
            if (!_requireWebAuth(req)) return;
            JsonDocument doc;
            if (deserializeJson(doc, data, len)) {
                _sendJsonError(req, 400, "JSON ungueltig");
                return;
            }
            if (!_jsonHasString(doc, "date") || !_jsonHasString(doc, "time")) {
                _sendJsonError(req, 400, "date/time fehlen");
                return;
            }
            String date = _jsonString(doc, "date");
            String time = _jsonString(doc, "time");
            if (date.length() != 10 || time.length() < 5) {
                _sendJsonError(req, 400, "Ungueltiges Format");
                return;
            }

            struct tm t = {};
            t.tm_year = date.substring(0, 4).toInt() - 1900;
            t.tm_mon  = date.substring(5, 7).toInt() - 1;
            t.tm_mday = date.substring(8, 10).toInt();
            t.tm_hour = time.substring(0, 2).toInt();
            t.tm_min  = time.substring(3, 5).toInt();
            t.tm_sec  = (time.length() >= 8) ? time.substring(6, 8).toInt() : 0;
            t.tm_isdst = -1;
            time_t epoch = mktime(&t);
            if (epoch < 1700000000L) {
                _sendJsonError(req, 400, "Zeit ungueltig");
                return;
            }
            if (!rtcSetEpoch(epoch)) {
                _sendJsonError(req, 500, "RTC konnte nicht gesetzt werden");
                return;
            }
            ntpSynced = true;
            lastNtpSync = millis();
            req->send(200, "application/json", "{\"ok\":true}");
        }
    );

    // POST /api/update - Firmware-Datei direkt ueber das Webinterface einspielen
    webServer.on("/api/update", HTTP_POST,
        [](AsyncWebServerRequest* req) {
            if (!_requireWebAuth(req)) return;
            if (webUpdateOk) {
                req->send(200, "application/json", "{\"ok\":true}");
                restartPending = true;
            } else {
                String err = webUpdateError.isEmpty() ? "Update fehlgeschlagen" : webUpdateError;
                String out = "{\"ok\":false,\"error\":\"" + err + "\"}";
                req->send(500, "application/json", out);
                otaRunning = false;
                displaySetOtaActive(false);
            }
        },
        [](AsyncWebServerRequest* req, const String& filename, size_t index,
           uint8_t* data, size_t len, bool final) {
            if (!_webAuthOk(req)) {
                webUpdateOk = false;
                webUpdateError = "Nicht authentifiziert";
                return;
            }
            if (index == 0) {
                webUpdateOk = false;
                webUpdateError = "";
                otaRunning = true;
                displaySetOtaActive(true);
                displayShowOTA(0, req->contentLength() > 0 ? req->contentLength() : 1);

                if (!Update.begin(UPDATE_SIZE_UNKNOWN)) {
                    webUpdateError = Update.errorString();
                    otaRunning = false;
                    displaySetOtaActive(false);
                    return;
                }
                Serial.printf("[WebUpdate] Starte Update: %s\n", filename.c_str());
            }

            if (webUpdateError.isEmpty()) {
                appWatchdogFeed();
                if (Update.write(data, len) != len) {
                    webUpdateError = Update.errorString();
                } else {
                    size_t done = index + len;
                    size_t total = req->contentLength() > 0 ? req->contentLength() : done;
                    displayShowOTA((uint32_t)done, (uint32_t)total);
                }
                appWatchdogFeed();
            }

            if (final) {
                appWatchdogFeed();
                if (!webUpdateError.isEmpty()) {
                    Update.abort();
                    otaRunning = false;
                    displaySetOtaActive(false);
                    Serial.printf("[WebUpdate] Fehler: %s\n", webUpdateError.c_str());
                    return;
                }
                if (!Update.end(true)) {
                    webUpdateError = Update.errorString();
                    otaRunning = false;
                    displaySetOtaActive(false);
                    Serial.printf("[WebUpdate] End-Fehler: %s\n", webUpdateError.c_str());
                    return;
                }
                webUpdateOk = true;
                otaRunning = false;
                displaySetOtaActive(false);
                Serial.println("[WebUpdate] Erfolgreich, Neustart folgt.");
            }
        }
    );

    webServer.begin();
}

// ============================================================
// OTA Setup
// ============================================================
void otaSetup() {
    ArduinoOTA.setHostname("hub75clock");
    ArduinoOTA.setPort(OTA_PORT);
    if (!cfg.ota_password.isEmpty()) {
        ArduinoOTA.setPassword(cfg.ota_password.c_str());
    }
    ArduinoOTA.onStart([]() {
        otaRunning = true;
        displaySetOtaActive(true);
        displayShowOTA(0, 1);
        appWatchdogFeed();
    });
    ArduinoOTA.onProgress([](unsigned int progress, unsigned int total) {
        displayShowOTA((uint32_t)progress, (uint32_t)total);
        appWatchdogFeed();
    });
    ArduinoOTA.onEnd([]()   {
        appWatchdogFeed();
        otaRunning = false;
        displaySetOtaActive(false);
    });
    ArduinoOTA.onError([](ota_error_t) {
        appWatchdogFeed();
        otaRunning = false;
        displaySetOtaActive(false);
    });
    ArduinoOTA.begin();
    Serial.printf("[OTA] Aktiv auf %s:%u\n",
                  WiFi.localIP().toString().c_str(),
                  (unsigned)OTA_PORT);
}
