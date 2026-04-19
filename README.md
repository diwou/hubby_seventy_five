# HUB75 NTP Flip Clock

Eine animierte Flip-Clock auf einem oder zwei 64×32 HUB75 LED-Matrix-Displays, betrieben von einem ESP32-S3.
Zeigt Uhrzeit (NTP-synchronisiert), Datum und aktuelle Wetterdaten an. Vollständig über ein Browser-Webinterface konfigurierbar — kein Neu-Flashen notwendig.

**Aktuelle Version:** 1.6.2

**Repository:** https://codeberg.org/diwou/Hubby_seventy_five

**Lizenz:** CC BY-NC-SA 4.0, siehe [LICENSE](LICENSE)

**Browser-Installer:** `index.html` bzw. `install.html` nutzen automatisch `Firmware/latest/manifest.json`

<p>
  <a href="https://diwou.codeberg.page/hubby_seventy_five/">
    <img src="https://img.shields.io/badge/USB%20Installer-Open%20In%20Browser-d66a1f?style=for-the-badge" alt="USB Installer im Browser oeffnen">
  </a>
</p>

---

## Inhalt

1. [Features](#features)
2. [Hardware](#hardware)
3. [Verkabelung / GPIO](#verkabelung--gpio)
4. [Software-Setup (PlatformIO)](#software-setup-platformio)
5. [Manuelles Update Ohne IDE](#manuelles-update-ohne-ide)
6. [Erstbetrieb](#erstbetrieb)
7. [Webinterface](#webinterface)
8. [Konfigurationsreferenz](#konfigurationsreferenz)
9. [Display-Layout](#display-layout)
10. [Animationsmodi](#animationsmodi)
11. [Sekundenanzeige-Modi](#sekundenanzeige-modi)
12. [REST-API](#rest-api)
13. [Architektur / Code-Struktur](#architektur--code-struktur)
14. [Changelog](#changelog)
15. [Lizenz](#lizenz)

---

## Features

- **Animierte Flip-Clock** — Ziffernwechsel mit Slide-Up- oder Cross-Dissolve-Animation
- **NTP-Zeitsynchronisation** — bis zu 3 konfigurierbare NTP-Server, automatischer Re-Sync
- **Wetteranzeige** — aktuelles Wetter + 3-Tage-Vorhersage via OpenWeatherMap API
- **Captive Portal** — bei fehlendem WLAN öffnet sich automatisch ein Konfigurations-Hotspot
- **Webinterface** — vollständige Konfiguration im Browser unter `http://hub75clock.local`
- **Umschaltbares Panel-Layout** — 1× HUB75 oder 2× HUB75 nebeneinander, mit vorbereiteter Mapping-Struktur für spätere 2×2-Layouts
- **OTA-Updates** — Firmware-Update über WLAN (ArduinoOTA)
- **Konfigurierbare Farben** — 8 Farbwähler für alle Display-Elemente
- **Mehrere Anzeigemodi** — 12h/24h, 4 Datumsformate, 4 Sekundenanzeige-Varianten
- **Zweisprachig** — Wochentage und Wetterbeschreibungen auf Deutsch oder Englisch
- **Double-Buffering** — kein Flackern oder Tearing

---

## Hardware

| Komponente | Beschreibung |
|---|---|
| **Mikrocontroller** | Freenove ESP32-S3 WROOM N16R8 (16 MB Flash, 8 MB PSRAM) |
| **Display** | 64×32 HUB75 RGB LED-Matrix, 1 oder 2 Panels nebeneinander |
| **Pegelwandler** | 74HCT245 (3,3 V → 5 V für HUB75-Signale) |

### KiCad / Platine

Die Platine liegt unter [`kicad/`](kicad/). Die bearbeitbaren KiCad-Quellen liegen in [`kicad/source/`](kicad/source/), die auslieferbaren Fertigungsdaten für Revision `0.2` in [`kicad/fab/rev0.2/`](kicad/fab/rev0.2/). Dort findest Du Gerber, Bohrdaten, BOM, Positionsdateien und den kompletten Fertigungs-ZIP.

---

## Verkabelung / GPIO

Das HUB75-Panel wird über einen **74HCT245**-Pegelwandler angeschlossen (3,3 V ESP32 → 5 V HUB75).

| HUB75-Signal | GPIO |
|---|---|
| R1 | 4 |
| G1 | 5 |
| B1 | 6 |
| R2 | 7 |
| G2 | 8 |
| B2 | 9 |
| A | 10 |
| B | 11 |
| C | 12 |
| D | 13 |
| E | –1 (ungenutzt, 32-Zeilen-Panel) |
| CLK | 14 |
| LAT | 17 |
| OE | 18 |

> **Hinweis:** Pin E wird nur bei 64-Zeilen-Panels benötigt. Bei diesem 32-Zeilen-Panel bleibt er unverbunden (–1).

---

## Software-Setup (PlatformIO)

### Voraussetzungen

- [PlatformIO](https://platformio.org/) (CLI oder VSCode-Extension)
- USB-Kabel oder WLAN (für OTA nach Erstflash)

### Befehle

```bash
# Firmware bauen
pio run

# Firmware flashen (USB, /dev/ttyACM0)
pio run --target upload

# Firmware flashen (OTA, wenn schon eingerichtet)
# → upload_port in platformio.ini auf IP-Adresse setzen
pio run --target upload

# Serieller Monitor (115200 Baud)
pio device monitor

# Build-Verzeichnis leeren
pio run --target clean
```

Nach jedem erfolgreichen `pio run` exportiert PlatformIO die flashbaren Dateien automatisch nach:

```text
Firmware/<Version>/
Firmware/latest/
```

Dort liegen jeweils:
- `bootloader.bin`
- `partitions.bin`
- `boot_app0.bin`
- `firmware.bin`
- `manifest.json`

Damit hast Du pro Version direkt einen vorbereiteten Satz fuer Browser-Flasher oder Release-Downloads.

Fuer einen einfachen USB-Erstflash ohne IDE gibt es ausserdem die Dateien `index.html` und `install.html`.
`index.html` dient als Startseite fuer statisches Hosting wie Codeberg Pages und leitet auf den eigentlichen Installer weiter. `install.html` verwendet automatisch den neuesten Export aus `Firmware/latest/manifest.json`.

> **OTA vs. USB:** In `platformio.ini` ist standardmäßig OTA (`upload_protocol = espota`) konfiguriert.
> Für USB-Flash die Zeile `upload_protocol = espota` auskommentieren und `upload_port = /dev/ttyACM0` einkommentieren.

### Repository klonen

```bash
git clone ssh://git@codeberg.org/diwou/Hubby_seventy_five.git
cd Hubby_seventy_five
```

### Verwendete Bibliotheken

| Bibliothek | Version | Zweck |
|---|---|---|
| `mrfaptastic/ESP32 HUB75 LED MATRIX PANEL DMA Display` | ^3.0.14 | DMA-Display-Treiber |
| `adafruit/Adafruit GFX Library` | ^1.12.5 | Text, Formen, Farben |
| `mathieucarbou/ESPAsyncWebServer` | ^3.3.0 | Asynchroner Webserver |
| `mathieucarbou/AsyncTCP` | ^3.2.0 | Async-TCP-Unterbau |
| `bblanchon/ArduinoJson` | ^7.2.0 | JSON (Config + OWM-API) |

---

## Manuelles Update Ohne IDE

Wer weder die Arduino IDE noch VS Code mit PlatformIO nutzen moechte, kann die Dateien aus `Firmware/latest/` oder aus einer festen Version wie `Firmware/1.6.2/` direkt mit `esptool` flashen.

Die vier relevanten Flash-Offets sind:

```text
0x0000   bootloader.bin
0x8000   partitions.bin
0xE000   boot_app0.bin
0x10000  firmware.bin
```

Vorbereitung:
- Geraet per USB anschliessen
- Seriellen Port ermitteln
- Falls noetig `esptool` installieren: `pip install esptool`

### Linux

USB-Port haeufig: `/dev/ttyACM0` oder `/dev/ttyUSB0`

```bash
esptool.py --chip esp32s3 --port /dev/ttyACM0 --baud 921600 --before default_reset --after hard_reset write_flash -z \
  0x0000 Firmware/latest/bootloader.bin \
  0x8000 Firmware/latest/partitions.bin \
  0xE000 Firmware/latest/boot_app0.bin \
  0x10000 Firmware/latest/firmware.bin
```

### macOS

USB-Port haeufig: `/dev/cu.usbmodem...` oder `/dev/cu.usbserial...`

```bash
esptool.py --chip esp32s3 --port /dev/cu.usbmodemXXXX --baud 921600 --before default_reset --after hard_reset write_flash -z \
  0x0000 Firmware/latest/bootloader.bin \
  0x8000 Firmware/latest/partitions.bin \
  0xE000 Firmware/latest/boot_app0.bin \
  0x10000 Firmware/latest/firmware.bin
```

### Windows

USB-Port haeufig: `COM3`, `COM4`, `COM5`, ...

```powershell
esptool.py --chip esp32s3 --port COM4 --baud 921600 --before default_reset --after hard_reset write_flash -z `
  0x0000 Firmware/latest/bootloader.bin `
  0x8000 Firmware/latest/partitions.bin `
  0xE000 Firmware/latest/boot_app0.bin `
  0x10000 Firmware/latest/firmware.bin
```

Hinweise:
- Wenn `esptool.py` nicht gefunden wird, klappt oft auch `python -m esptool`.
- Bei einer festen Release-Version statt `Firmware/latest/` einfach z. B. `Firmware/1.6.2/` verwenden.
- Wer gar nichts lokal installieren moechte, kann alternativ weiter den Browser-Installer nutzen.
- Der Browser-Installer ist bereits auf dem aktuellen Stand: `Firmware/latest/manifest.json` zeigt derzeit auf Version `1.6.2`.

---

## Erstbetrieb

1. **Firmware flashen** (USB):
   ```bash
   # upload_port in platformio.ini auf /dev/ttyACM0 setzen
   pio run --target upload
   ```

2. **Mit dem Hotspot verbinden:**
   Das Display zeigt `AP-Modus` und den SSID-Namen `HUB75CLOCK-XXXX`.
   Mit diesem WLAN verbinden — das Captive Portal öffnet sich automatisch.
   (Falls nicht: Browser öffnen und `http://192.168.4.1` aufrufen.)

3. **WLAN konfigurieren:**
   Im Tab **WLAN** → SSID und Passwort eingeben → **Speichern & Neustart**.

4. **Nach dem Neustart:**
   Das Display scrollt kurz die IP-Adresse und `hub75clock.local`.
   Das Webinterface ist erreichbar unter:
   - `http://hub75clock.local` (mDNS, empfohlen)
   - `http://<IP-Adresse>` (IP aus dem Router ablesen oder Serial Monitor)

5. **OpenWeatherMap einrichten** (optional, für Wetteranzeige):
   - Kostenloser API-Key: [openweathermap.org](https://openweathermap.org/api)
   - Im Tab **Wetter** → API-Key + Stadtname eingeben → **Speichern**

6. **Zeitzone prüfen:**
   Im Tab **NTP** → Zeitzone als POSIX-String einstellen.
   Standard: `CET-1CEST,M3.5.0,M10.5.0/3` (Europa/Berlin mit Sommerzeit)

---

## Webinterface

Das Webinterface ist in sechs Tabs gegliedert:

### Status
Zeigt Systeminformationen in Echtzeit (Auto-Refresh alle 30 Sekunden):
- Firmware-Version, IP-Adresse, WLAN-SSID und Signalstärke (RSSI)
- Uptime, aktuelle Uhrzeit
- NTP-Synchronisationsstatus und aktiver NTP-Server
- Wetter-Standort, aktuelle Temperatur und Wetterbedingung
- Nächste geplante NTP- und Wetter-Aktualisierung
- Freier Heap-Speicher

Schaltflächen: **Aktualisieren**, **Neustart**, **Factory Reset**

### WLAN
- SSID und Passwort für das Heimnetz
- AP-Passwort (leer = offener Hotspot ohne Passwort)
- WLAN-Verbindungs-Timeout (Standard: 30 s)

### NTP
- Bis zu 3 NTP-Server (Standard: `pool.ntp.org`, `time.google.com`, `time.cloudflare.com`)
- Sync-Intervall (Standard: 15 min)
- Zeitzone: Freitext-POSIX-String oder Auswahl aus Vorlagen (Berlin, London, Athen, UTC, USA)

### Wetter
- OpenWeatherMap API-Key
- Standort: Stadtname mit Geo-Suche **oder** GPS-Koordinaten (Breitengrad / Längengrad)
- Temperatureinheit: Celsius oder Fahrenheit
- Abruf-Intervall (Standard: 30 min)

### Anzeige
- Helligkeit (0–255)
- HUB75-Layout: `1x HUB75 (64x32)` oder `2x HUB75 nebeneinander (128x32)`
- Sprache: Deutsch / Englisch (Wochentage, Wetterbeschreibungen)
- Zeitformat: 24h oder 12h (AM/PM-Indikator)
- Datumsformat: 4 Varianten (siehe [Konfigurationsreferenz](#konfigurationsreferenz))
- Animations-Modus: Slide-Up oder Cross-Dissolve
- Sekundenanzeige: Bar / Wanderpunkt / Dots / HH:MM:SS
- Wetter-Wechselintervall (untere Zeile, Standard: 6 s)
- Scroll-Geschwindigkeit (30 = schnell, 200 = langsam, Standard: 80 ms/Pixel)
- **Farben:** 8 Farbwähler für alle Display-Elemente

Änderungen am HUB75-Layout werden erst nach einem Neustart aktiv, weil die Matrix-DMA-Konfiguration beim Booten initialisiert wird.

### OTA
- OTA-Updates aktivieren / deaktivieren
- Optionales OTA-Passwort
- OTA-Hostname: `hub75clock.local`, Port: 8266

---

## Konfigurationsreferenz

Alle Einstellungen werden in `/cfg.json` auf dem LittleFS-Dateisystem gespeichert.

### WLAN

| Parameter | Standard | Beschreibung |
|---|---|---|
| `sta_ssid` | `""` | SSID des Heimnetzwerks |
| `sta_pass` | `""` | WLAN-Passwort |
| `ap_password` | `""` | Passwort für den Konfigurationshotspot (leer = offen) |
| `wifi_timeout_sec` | `30` | Verbindungs-Timeout in Sekunden |

### NTP

| Parameter | Standard | Beschreibung |
|---|---|---|
| `ntp1` | `pool.ntp.org` | Primärer NTP-Server |
| `ntp2` | `time.google.com` | Sekundärer NTP-Server |
| `ntp3` | `time.cloudflare.com` | Tertiärer NTP-Server |
| `ntp_interval_min` | `15` | Re-Sync-Intervall in Minuten |
| `tz` | `CET-1CEST,M3.5.0,M10.5.0/3` | Zeitzone als POSIX-String |

**Zeitzone-Beispiele:**

| Zeitzone | POSIX-String |
|---|---|
| Europa/Berlin (mit Sommerzeit) | `CET-1CEST,M3.5.0,M10.5.0/3` |
| Europa/London | `GMT0BST,M3.5.0/1,M10.5.0` |
| UTC | `UTC0` |
| USA/New York | `EST5EDT,M3.2.0,M11.1.0` |
| USA/Los Angeles | `PST8PDT,M3.2.0,M11.1.0` |

### Wetter

| Parameter | Standard | Beschreibung |
|---|---|---|
| `owm_apikey` | `""` | OpenWeatherMap API-Key |
| `owm_city` | `""` | Stadtname (z. B. `Berlin`) |
| `owm_lat` / `owm_lon` | `0.0` | GPS-Koordinaten (nur wenn `owm_use_coords=true`) |
| `owm_use_coords` | `false` | `true` = Koordinaten, `false` = Stadtname |
| `owm_unit` | `0` | `0` = Celsius, `1` = Fahrenheit |
| `owm_interval_min` | `30` | Abruf-Intervall in Minuten |

### Anzeige

| Parameter | Standard | Beschreibung |
|---|---|---|
| `brightness` | `90` | Display-Helligkeit (0–255) |
| `lang` | `"de"` | Sprache: `"de"` oder `"en"` |
| `time_12h` | `0` | `0` = 24h, `1` = 12h (AM/PM) |
| `date_format` | `0` | Datumsformat (0–3, siehe unten) |
| `anim_mode` | `0` | `0` = Slide-Up, `1` = Cross-Dissolve |
| `seconds_mode` | `0` | `0` = Bar, `1` = Pulse, `2` = Dots, `3` = HH:MM:SS |
| `display_panel_cols` | `2` | Aktuell unterstützt: `1` = ein Panel, `2` = zwei Panels nebeneinander |
| `display_panel_rows` | `1` | Vorbereitung für späteres 2×2-Layout; derzeit im Webinterface fest auf `1` |
| `display_panel_map` | `0` | Vorbereitung für Mapping-Modi: `0` = linear, `1` = ZigZag/Serpentine; derzeit noch nicht im Webinterface auswählbar |
| `bottom_cycle_sec` | `6` | Wechselintervall der Wetterzeile in Sekunden |
| `scroll_speed_ms` | `80` | Scroll-Geschwindigkeit in ms pro Pixel (30–200) |

**Datumsformate:**

| Wert | Beispiel | Beschreibung |
|---|---|---|
| `0` | `Mo 17.03.26` | Wochentag + TT.MM.JJ (Standard) |
| `1` | `Mo 17.03` | Wochentag + TT.MM (ohne Jahr) |
| `2` | `17.03.2026` | TT.MM.JJJJ (lang, ohne Wochentag) |
| `3` | `03/17/26` | MM/TT/JJ (US-Format) |

### Farben

Alle Farben im Format `#RRGGBB`:

| Parameter | Standard | Element |
|---|---|---|
| `color_digit` | `#FFC832` | Uhrziffern (Gold/Amber) |
| `color_date` | `#64A0FF` | Datumtext (Blau) |
| `color_temp` | `#FF8C28` | Temperaturanzeige (Orange) |
| `color_warn` | `#FFDC00` | Wochentag / Warnmeldungen (Gelb) |
| `color_info` | `#A0A0A0` | Info-Texte (Grau) |
| `color_sec` | `#C88C14` | Sekunden-Balken (Dunkelamber) |
| `color_sec5` | `#00DC46` | 5s-Marke im Sekundenbalken (Grün) |
| `color_sec10` | `#50DCFF` | 10s-Marke im Sekundenbalken (Cyan) |

### OTA

| Parameter | Standard | Beschreibung |
|---|---|---|
| `ota_enabled` | `true` | OTA-Updates aktiviert |
| `ota_password` | `""` | OTA-Passwort (leer = kein Passwort) |

---

## Display-Layout

Die Firmware kann aktuell mit einem oder zwei 64×32-HUB75-Panels betrieben werden:

| Einstellung | Virtuelle Größe | Verhalten |
|---|---:|---|
| `1x HUB75` | 64×32 px | Uhr, Datum und Wetter laufen wie im klassischen Ein-Panel-Layout auf einem Panel |
| `2x HUB75 nebeneinander` | 128×32 px | Uhr bleibt links, Datum nutzt das rechte Panel, die Wetterzeile scrollt über die volle Breite |

Die Konfiguration speichert bereits `display_panel_cols`, `display_panel_rows` und `display_panel_map`. Dadurch ist die Struktur für ein späteres 2×2-Layout vorbereitet. Für 2×2 muss zusätzlich das physische Panel-Mapping implementiert werden, z. B. linear oder ZigZag/Serpentine passend zur Verkabelung.

Bei `1x HUB75` verwendet `HH:MM:SS` bewusst das kompakte Ein-Panel-Layout aus Version `1.5.4`, damit die sechs Ziffern auf 64×32 px nicht zu groß dargestellt werden. Bei `2x HUB75 nebeneinander` bleibt das größere aktuelle Layout aktiv.

Das 64×32-Basislayout ist in vier Bereiche aufgeteilt:

```
┌─────────────────────────────────────────────────────────────────┐ y=0
│                                                                 │
│   Uhrzeit: HH:MM (10×14px Bitmap-Ziffern, Gold/Amber)          │ y=0..13
│                                                                 │
├─────────────────────────────────────────────────────────────────┤ y=14
│   Sekundenanzeige (2px, Mode 0–2) — entfällt bei Mode 3        │ y=14..15
├─────────────────────────────────────────────────────────────────┤ y=16
│   Gap (1px)                                                     │ y=16
├─────────────────────────────────────────────────────────────────┤ y=17
│   Datum: z. B. "Mo 17.03.26"  /  Temperatur (rechtsbündig)     │ y=17..23
├─────────────────────────────────────────────────────────────────┤ y=24
│   Gap (1px)                                                     │ y=24
├─────────────────────────────────────────────────────────────────┤ y=25
│   Wetter (rotierend): Aktuell → Di-Forecast → Mi → Do          │ y=25..31
└─────────────────────────────────────────────────────────────────┘ y=31
```

### Ziffern-Positionen HH:MM (Mode 0–2)

```
H1=7  H2=19  [:]=31  M1=35  M2=47
```
(Kolon: 2×2-Pixel-Punkte bei y=3 und y=9)

### Ziffern-Positionen HH:MM:SS (Mode 3)

```
H1=2  H2=11  [C1]=20..21  M1=23  M2=32  [C2]=41..42  S1=44  S2=53
```
(9px-Ziffern, 2×2-Pixel-Kolons)

### Wetterzeile (rotierend, Wechsel alle `bottom_cycle_sec` Sekunden)

| Modus | Inhalt | Beispiel |
|---|---|---|
| 0 | Aktuelles Wetter scrollend | `+12.4C  Leicht bew.` |
| 1 | Vorhersage +1 Tag statisch | `Di +15/-3` |
| 2 | Vorhersage +2 Tage statisch | `Mi +12/-1` |
| 3 | Vorhersage +3 Tage statisch | `Do +8/-5` |

---

## Animationsmodi

### Slide-Up (`anim_mode = 0`)
- 14 Frames à 15 ms = ~210 ms Gesamtdauer
- Phase 1 (Frame 0–6): Alte Ziffer gleitet nach oben aus dem Slot heraus
- Phase 2 (Frame 7–13): Neue Ziffer gleitet von unten in den Slot hinein

### Cross-Dissolve (`anim_mode = 1`)
- 14 Frames à 18 ms = ~252 ms Gesamtdauer
- Phase 1 (Frame 0–6): Alte Ziffer blendet von voll → schwarz aus (7 Helligkeitsstufen)
- Phase 2 (Frame 7–13): Neue Ziffer blendet von schwarz → voll ein

### Kolon-Animation
- Synchron zum Sekundenwechsel: Cross-Dissolve-Puls (14 Frames à 18 ms)
- Der Kolon bleibt dauerhaft hell und pulsiert kurz zum Sekundenwechsel
- Außerhalb der Animation: statisch in Ziffernfarbe

---

## Sekundenanzeige-Modi

| Modus | Beschreibung |
|---|---|
| `0` — Bar | Balken füllt sich von links; 5s-Marken grün, 10s-Marken cyan |
| `1` — Pulse | Ein Leuchtpunkt wandert mit kurzem Schweif von links nach rechts |
| `2` — Dots | 60 Einzelpunkte; vergangene Sekunden leuchten, zukünftige sind gedimmt |
| `3` — HH:MM:SS | Kein Sekundenbalken — Sekunden erscheinen direkt in der Uhrzeitzeile |

---

## REST-API

Das Webinterface kommuniziert über eine interne JSON-API. Alle Endpunkte sind unter der IP-Adresse des Geräts erreichbar.

| Methode | Endpunkt | Beschreibung |
|---|---|---|
| `GET` | `/api/config` | Alle Konfigurationswerte als JSON abrufen |
| `POST` | `/api/config` | Konfiguration aktualisieren (JSON-Body) |
| `GET` | `/api/status` | Systemstatus (Firmware, IP, WLAN, NTP, Wetter) |
| `GET` | `/api/geocode?q={Stadt}&apikey={Key}` | Ortsuche via OWM Geocoding API (bis zu 5 Treffer) |
| `GET` | `/api/restart` | Gerät neu starten |
| `GET` | `/api/reset` | Factory Reset (alle Einstellungen löschen + Neustart) |

### Beispiel: Konfiguration per curl ändern

```bash
# Helligkeit auf 150 setzen
curl -X POST http://hub75clock.local/api/config \
  -H "Content-Type: application/json" \
  -d '{"brightness": 150}'

# Auf ein einzelnes HUB75-Panel umstellen; Neustart danach erforderlich
curl -X POST http://hub75clock.local/api/config \
  -H "Content-Type: application/json" \
  -d '{"display_panel_cols": 1, "display_panel_rows": 1}'

# Aktuellen Status abrufen
curl http://hub75clock.local/api/status
```

---

## Architektur / Code-Struktur

```
src/
├── main.cpp       — Setup, Loop, WiFi-State-Machine, NTP-Sync, Wetter-Timer
├── config.h       — Config-Struct, LittleFS-Persistenz (configLoad/Save/Reset)
├── display.h      — HUB75-Initialisierung, Bitmap-Font, Flip-Animationen, Display-Update
├── weather.h      — OpenWeatherMap HTTP-Fetch, Wetter-Datenstruktur, Geocoding
└── webserver.h    — AsyncWebServer, REST-API, Captive-Portal, OTA-Setup, HTML/CSS/JS
```

### Wichtige Abhängigkeiten

```
main.cpp
  ├── config.h      (cfg, configLoad/Save/Reset)
  ├── display.h     (displayInit, displayUpdate, displayReset, ...)
  │     └── weather.h (WeatherData, forecastDayName)
  └── webserver.h   (webServerSetup, otaSetup, dnsServer)
        ├── extern MatrixPanel_I2S_DMA* dma_display  ← aus display.h
        └── extern bool ntpSynced                    ← aus main.cpp
```

> Alle `.h`-Dateien definieren globale Variablen direkt (kein ODR-Problem, da nur eine `.cpp`-Datei kompiliert wird).

### WiFi-State-Machine (`main.cpp`)

```
Start
  │
  ▼
wifiConnect() ──── Erfolg ──→ mDNS + NTP-Sync + WebServer + OTA + Wetter
  │
  Timeout
  │
  ▼
startAP() ──→ Hotspot "HUB75CLOCK-XXXX" + Captive-Portal + WebServer
                   (kein NTP, kein Wetter im AP-Modus)

Loop:
  WiFi verloren? ──→ alle 30 s Reconnect-Versuch
```

### Double-Buffering / Kein Flackern

Die HUB75-Bibliothek verwendet zwei DMA-Buffer. Jeder Frame wird in den Back-Buffer gezeichnet, dann mit `flipDMABuffer()` atomisch sichtbar gemacht. Statische Ziffern werden **nicht** gelöscht und neu gezeichnet — nur animierende oder als `dirty` markierte Slots werden aktualisiert.

Nach `ESP.restart()` werden RAM-Überreste durch `displayReset()` in `setup()` explizit gelöscht (verhindert Ghosting beim Soft-Reboot).

### Bitmap-Font

Die Ziffern basieren auf dem **Dottie 8×8-Font**, konvertiert in 10×14-Pixel-Bitmaps (PROGMEM). Jede Ziffer besteht aus 14 `uint16_t`-Werten (eine pro Zeile), wobei Bit 9 die linkeste Spalte repräsentiert.

---

## Changelog

Die vollständige Änderungshistorie steht in [CHANGELOG.md](CHANGELOG.md).

## Lizenz

Dieses Projekt steht unter der Lizenz **Creative Commons Attribution-NonCommercial-ShareAlike 4.0 International**.

Kurzform:
- Namensnennung erforderlich
- Keine kommerzielle Nutzung
- Bearbeitungen müssen unter derselben Lizenz weitergegeben werden

Details siehe [LICENSE](LICENSE) und:
https://creativecommons.org/licenses/by-nc-sa/4.0/
