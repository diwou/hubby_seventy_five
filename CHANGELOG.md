# Changelog

Alle wesentlichen Änderungen an diesem Projekt werden in dieser Datei dokumentiert.

Das Format basiert auf [Keep a Changelog](https://keepachangelog.com/de/1.0.0/).

Ab `1.6.0` stehen Codeberg-Referenzen direkt an den einzelnen Aenderungspunkten stehen, damit auch bei mehreren Commits am selben Tag jede Aenderung eindeutig nachvollziehbar bleibt. Sammel-IDs nur fuer Release-/Milestone-Metadaten, nicht als Ersatz fuer Einzelreferenzen.

---

## [UNRELEASED]

### Fixed
- Nach der Verschiebung der Datumszeile im 1x-HUB75-Modus wurde der Wetter-Clear-Bereich ebenfalls nach unten angepasst, damit die untere Datumszeile nicht mehr vom Wetter-Redraw geloescht wird. [9b54a0e]
- Die 1x-HUB75-Vorhersage wurde horizontal verdichtet (2-stellige Wochentage, engere Abstaende um `C` und `/`), damit die komplette Zeile wieder sicher auf ein Einzelpanel passt. [9b54a0e]
- Im 1x-HUB75-Modus wurde die Datumszeile (inkl. Trennpunkte) um 2 Pixel nach unten verschoben, damit bei Einzelpanel die Uhrzeit nicht mehr in den Datumsbereich hineinschreibt. [65932e8]
- DS3231 als aktive Laufzeit-Zeitquelle integriert: Die Systemzeit wird aus der RTC gefuehrt, NTP korrigiert die RTC, und das Webinterface wurde um RTC-Status sowie einen RTC-Setzen-Endpunkt erweitert. [65932e8]
- Konfigurations-Bootstrap gehaertet: Wenn `cfg.json` fehlt, unlesbar oder inkonsistent ist, schreibt die Firmware jetzt sofort einen sauberen Default-Stand auf LittleFS. Zusaetzlich werden sensible Textfelder beim Laden normalisiert, damit keine kaputten Erstwerte im Laufzeit- oder Speicherzustand bleiben. [65932e8]
- OTA-Passwort-Handling korrigiert: Ein geleertes OTA-Passwort wird jetzt beim Speichern tatsaechlich entfernt (statt still unveraendert zu bleiben). Zusaetzlich zeigt das Status-Tab jetzt, ob OTA aktiviert ist und ob ein OTA-Passwort gesetzt ist. [65932e8]
- OTA-Dokumentation im Webinterface wurde korrigiert: Statt des falschen Ports `8266` verwendet die Firmware jetzt explizit `ArduinoOTA` auf Port `3232`, damit Uploads per IP wieder mit den angezeigten Verbindungsdaten funktionieren. [65932e8]
- Die I2C-Diagnose wurde fuer den BME680 erweitert: Neben dem reinen Address-ACK zeigt die Firmware jetzt zusaetzlich den gefundenen BME680-I2C-Kanal (`0x76`/`0x77`), die gelesene Chip-ID aus Register `0xD0` und einen kompakten Bus-Scan im Status. Damit laesst sich eindeutig unterscheiden, ob der Sensor gar nicht antwortet oder mit unerwarteter ID antwortet. [65932e8]
- Web-Konfiguration zeigt jetzt fuer geheime Felder (u. a. OpenWeatherMap API-Key) den gespeicherten Zustand klar als Platzhalter an, damit ein korrekt gespeicherter Key nicht wie "verloren" wirkt; zusaetzlich wird der I2C-Geraetestatus (DS3231/VEML7700/BME680) im Status-Tab sichtbar. [65932e8]
- Das Web-Statusbild wurde auf Dottie-Stand erweitert: zusaetzliche RTC-Statuszeilen sowie VEML7700-Diagnose mit Lux-Werten und Auto-Helligkeit sind jetzt direkt im Status sichtbar; die Option `Auto-Helligkeit per VEML7700` ist im Anzeige-Tab konfigurierbar. [9b54a0e]
- Auto-Helligkeit per VEML7700 arbeitet jetzt ohne periodisches Flackern: Sensor-Reads sind langsamer getaktet, Lesefehler halten den letzten Wert statt auf den manuellen Wert zurueckzuspringen, und Helligkeitsaenderungen werden mit Delta-/Zeit-Hysterese ausgegeben. Der Helligkeitsregler wirkt im Auto-Modus jetzt als Obergrenze (`Max-Helligkeit`) analog Dottie. [3c233c6]
- Statusdaten wurden im Webinterface sortiert: RTC-/DS3231-Details stehen im RTC-Tab, VEML7700-/Lux-Details im Anzeige-Tab; die Haupt-Statusseite zeigt wieder nur den kompakten Systemueberblick. [3c233c6]
- BME680 wurde als aktiver Umweltsensor eingebunden: Temperatur, Luftfeuchte, Luftdruck und Gaswiderstand werden zyklisch ausgelesen und im Wetter-Tab des Webinterfaces angezeigt. [844096c]
- Die kompakte 1x-HUB75-Vorhersage nutzt jetzt exakt 2 Pixel Abstand zwischen Temperatur und Einheit sowie vor/nach dem `/`, damit die Zeile sauber auf ein Panel passt. [844096c]
- Hardware-Reset und I2C-Basis wurden auf die neue Verdrahtung angepasst: Factory-Reset liegt jetzt auf GPIO3 (Long-Press), der I2C-Bus auf SDA GPIO38 und SCL GPIO39; beim Boot prueft die Firmware jetzt gezielt DS3231 (`0x68`), VEML7700 (`0x10`) und BME680 (`0x76/0x77`) und meldet fehlende Geraete im Status. [65932e8]
- Die HUB75-Adressleitung `E` ist jetzt als eigener GPIO (`PIN_E = 14`) verdrahtet und wird in der Matrix-Pinmap aktiv angesteuert statt ungenutzt auf `-1`, damit Panels mit genutzter E-Leitung korrekt adressiert werden. [65932e8]
- Ungueltige oder leere Farbwerte aus der gespeicherten Konfiguration werden jetzt beim Laden und Speichern konsequent auf gueltige `#RRGGBB`-Defaults zurueckgesetzt, damit ein frisch aufgespielter oder migrierter Stand nicht mit schwarzen Farben startet. [65932e8]
- Nach einem echten Power-On- oder Brownout-Reset wartet die Firmware jetzt kurz auf stabile Versorgung und fuehrt danach genau einmal einen kontrollierten Soft-Neustart aus. Das bildet den bisher noetigen manuellen Reset-Tastendruck nach, ohne bei normalen Software-Neustarts eine Reset-Schleife zu erzeugen. [power-on-soft-reset-20260430]
- OTA-Fortschrittsanzeigen werden jetzt gedrosselt, damit ArduinoOTA- und Web-Firmware-Uploads nicht durch zu haeufige Display-Redraws ausgebremst oder abgebrochen werden. [ota-progress-drossel-20260428]
- Die Subpixel-Ueberblendung des aktuellen Wetterlaufbands wurde wieder entfernt, weil sie die Buchstaben beim Scrollen sichtbar unscharf wirken liess. [scroll-subpixel-ruecknahme-20260428]
- Das aktuelle Wetterlaufband nutzt jetzt eine leichte Subpixel-Ueberblendung zwischen benachbarten Pixelspalten, damit die ganzen Pixel-Schritte weniger ruckelig wirken. [scroll-subpixel-20260428]
- Der aktuelle Wetter-Scroll berechnet seine Position jetzt aus einer festen Startzeit statt aus fortgeschriebenen Frame-Deltas, damit der Lauf gleichmaessiger bleibt und beim Durchlaufende sauber einfriert. [scroll-startzeit-20260428]
- Der erste Wetterabruf nach dem Boot laeuft jetzt in einem eigenen Task, damit Uhr und Display nicht waehrend DNS-/HTTPS-/OWM-Wartezeiten stehen bleiben; leere Web-Passwoerter aus alten Konfigurationen werden beim Laden wieder auf den Standardzugang gesetzt. [fb5a44a]
- Das Webinterface schuetzt Konfigurations-, Neustart-, Reset- und Firmware-Update-Endpunkte jetzt per HTTP Basic Auth, liefert gespeicherte Geheimnisse nicht mehr an `/api/config` aus und uebernimmt Passwortfelder nur noch, wenn wirklich ein neuer Wert gesendet wurde. [603e3f2]
- Der Web-Firmware-Upload fuettert nun ebenfalls den App-Watchdog, damit lange Uploads nicht mehr durch den eigenen Monitor-Neustart abgebrochen werden. [603e3f2]
- Der Wetterabruf blockiert den Bootpfad nicht mehr; nach erfolgreicher WLAN-/NTP-Initialisierung uebernimmt wieder der normale Loop den ersten Abruf. [603e3f2]
- Aenderungen an sichtbaren Display-Einstellungen invalidieren jetzt Uhr-, Datums- und Wetterbereiche sofort, damit Farben, Sprache und Datumsformat ohne alte statische Pixel sichtbar werden. [603e3f2]
- Ungueltig kurze AP-Passwoerter werden serverseitig abgelehnt und der AP-Start prueft den Rueckgabewert von `WiFi.softAP()`, statt Fehler still zu uebergehen. [603e3f2]

### Changed
- Der Standardwert fuer die Wetter-Scrollgeschwindigkeit wurde von `80` auf `40 ms/Pixel` gesetzt; gespeicherte alte Standardwerte `80` werden beim Laden auf den weicheren Lauf migriert. [scroll-standard-20260428]
- Die `6` im `HH:MM:SS`-Font wurde noch einmal nachkorrigiert, damit der letzte Rahmen oben und unten wieder sauber schließt. [b25ff24]
- OWM laeuft jetzt ausschliesslich per HTTPS, die Geocoding-Suche sendet den API-Key nicht mehr aus dem Browser, und `displayReset()` setzt den Sekunden-Mode-Zustand sauber mit zurueck. [2a2b83f]
- Die `HH:MM:SS`-Ziffern verwenden jetzt exakt die neue 7×14-Glyphen-Tabelle mit 2px-starken Strichen und bleiben damit wieder sauber lesbar. [961f516]
- Die Abstaende nach den Doppelpunkten im `HH:MM:SS`-Layout wurden so verschoben, dass danach jeweils 2 freie Pixel bleiben. [930fa2f]
- Die sichtbare Versionsquelle liegt nun in `include/config.h` als `APP_VERSION_BASE`; `include/version_generated.h` wird beim Build aus Base-Version und Git-Hash erzeugt und von der Firmware genutzt. [603e3f2]
- KiCad-Projekt und Fertigungsdaten wurden auf Revision `0.3` angehoben: `kicad/source/` spiegelt jetzt den aktuellen Schaltplan- und PCB-Stand, und `kicad/fab/rev0.3/` enthält das passende Gerber-/BOM-/ZIP-Paket fuer den Platinendienstleister. [6bd7dc8]
- Dauerhafte Projektleitplanken wurden jetzt direkt in `AGENTS.md` und `docs/RELEASING.md` verankert, damit KiCad-Revision, Backups und Release-Artefakte nicht mehr jedes Mal neu erklärt werden muessen. [12f18d2]

## [1.6.2] - 2026-04-19

### Fixed
- Beim Ruecksprung vom Forecast auf das aktuelle Wetter werden Scroll-Start und Scroll-Zeit jetzt gemeinsam neu gesetzt, damit nicht nur der Rest der Zeile sichtbar bleibt [`bb6ac12`]
- Der aktuelle Wetter-Scroll nutzt jetzt wieder eine feste Zeitbasis mit Festkomma-Position, ohne Forecast oder Übergänge anzufassen [`2b9e867`]
- Beim Neustart des aktuellen Wetter-Scrolls wird der Scroll-Start jetzt mit zurückgesetzt, damit nicht nur der Zeilenschluss sichtbar wird [`2b9e867`]
- Die Wetterzeile wurde wieder auf die alte Anzeige zurückgesetzt, nachdem die jüngsten Experimente Artefakte und Flickern erzeugt hatten [`b035168`]
- Der neue Heartbeat-Watchdog prueft jetzt sauber, ob sein Task wirklich gestartet wurde. Wenn die Task-Erzeugung fehlschlaegt, wird das auch als Fehler gemeldet statt stillschweigend als aktiv zu gelten.
- Der Watchdog meldet beim Ausloesen jetzt auch die zuletzt aktive Programmpassage, damit sich echte Hänger schneller von harmlosen Zwischenzuständen unterscheiden lassen.
- Die Watchdog-Hängermeldung ist jetzt auch im Webinterface sichtbar, inklusive Phase, Dauer und aktuellem Boot-Grund.
- Das Webinterface zeigt beim letzten Watchdog-Hänger jetzt auch den gespeicherten Zeitpunkt an, sofern zum Auslösezeitpunkt bereits eine gültige Uhrzeit vorhanden war.
- Der Wetterabruf wurde aus dem `setup()`-Pfad entfernt, damit ein blockierender API- oder Netzfehler den Start nicht mehr bei `Wetter...` festhält; der normale Loop übernimmt den Abruf.
- Beim Boot werden jetzt alle Laufzeit-Flags explizit zurückgesetzt, damit ein Software-Reset keine alten OTA-/AP-/Reconnect-Zustände aus der vorherigen Sitzung mitnimmt.
- Der WiFi-Stack wird beim Boot nicht mehr hart auf `WIFI_OFF` zurückgesetzt; nur die Laufzeit-Flags werden noch zurückgesetzt, damit OTA und WLAN-Setup nicht wieder unnötig gestört werden.
- OTA gibt die Display-Sperre nach einem erfolgreichen Abschluss jetzt wieder frei, damit ein verzögerter Neustart nicht auf einem dauerhaft blockierten Display endet.
- Das Status-Tab im Webinterface aktualisiert sich jetzt per Ajax im 1-Sekunden-Takt und zeigt zusätzlich die letzte interne Gerätemeldung an.
- Die Firmware-Version zeigt im Development-Build jetzt zusätzlich die kurze Codeberg-Commit-ID an, während Release-Builds nur die Versionsnummer anzeigen.
- Die Startanzeige auf dem Display verwendet jetzt denselben Release-/Development-String wie der Rest der Firmware-Version.
- Die Versionsausgabe nutzt jetzt wieder nur noch eine einzige Variable (`FW_VERSION`) für Display, Webinterface, Serial-Log und Firmware-Export.
- Die Kurz-BOM in `kicad/README.md` wird jetzt direkt aus `kicad/fab/rev0.3/bom/bom.csv` generiert, zeigt jetzt auch die Bauformen mit an und kann mit `scripts/render_kicad_bom_summary.py --write-readme` reproduzierbar aktualisiert werden.
- Die BOM-Übersicht nennt jetzt explizit, dass `U5` ein ESP32-S3-WROOM-1 mit PSRAM sein muss.
- Die Wetterzeile wird jetzt erst komplett in einem Offscreen-Canvas gerendert und danach als fertiger Block auf das Display kopiert; das reduziert das sichtbare Flickern bei Scrolltext und Vorhersage merklich.
- Statusbilder, die die Matrix kurz loeschen, markieren nun die statischen Uhr- und Datumsbereiche wieder als dirty. Dadurch kommen Uhrzeit, Datum und Wochentag nach einer kurzen NTP-/WLAN-Stoerung sofort komplett zurueck und bleiben nicht bis zur naechsten echten Aenderung leer [`572f1c1`]
- Vorhersage-Kacheln wechseln jetzt als Uebergang nach oben weg: Die aktuelle Kachel faehrt und blendet nach oben aus, waehrend die naechste von unten einfaehrt und dabei eingeblendet wird [`200baba`]
- Der neue Vorhersage-Uebergang baut jetzt sauber ohne alte `forecastSlide*`-Reste und wurde mit `pio run` erfolgreich kompiliert.
- Der Vorhersage-Uebergang bleibt jetzt im Wetterfenster: Die Datumszeile wird waehrend des Wechsels nach dem Wetter-Redraw noch einmal sauber ueberzeichnet, damit beim Hochfahren keine Artefakte mehr in die Uhr-/Datumszone laufen [`bfcbea4`]
- Der Forecast-Übergang wurde nach dem Praxistest entschärft: Die alte Kachel wird jetzt nur noch im Wetterfenster ausgeblendet, damit nichts mehr in Uhrzeit oder Datum hineinzeichnet; die neue Kachel startet wieder sichtbar von unten [`86bcc12`]
- Wetterkreislauf repariert: Nach dem aktuellen Wetterlaufband startet die erste Vorhersage wieder per Uebergang, und nach der dritten Vorhersage springt das System sauber zum aktuellen Wetter zurueck. Damit wechselt die Anzeige wieder zyklisch zwischen Laufband und Vorhersage [`9739479`]
- Wetterkreislauf erneut gefixt: Der Forecast-Zyklus kehrt nach dem dritten Tag wieder zum aktuellen Wetterlaufband zurueck, und der vertikale Uebergang bleibt dabei auf den Wetterbereich begrenzt, damit nichts mehr in die Uhrzeit hineinragt [`39f6d34`]
- Forecast-Animation weiter entkoppelt: Erst blendet die alte Zeile komplett aus, erst danach faehrt die neue von unten ein. Damit ueberlagern sich Aus- und Einblenden nicht mehr [`2be3e2a`]
- Nach dem Praxistest wurde der vertikale Abstieg des alten Forecasts wieder auf den unteren Wetterstreifen begrenzt, damit er auf dem Einzelpanel nicht mehr in die Uhrzeit hineinragt [`5f48c5d`]
- Wenn der Forecast uebergangsweise in die oberen Zeilen hineinragt, wird die Uhrzeit direkt danach noch einmal frisch gezeichnet, damit der obere Bereich auf 1x und 2x HUB75 sauber bleibt [`b71d805`]
- Waehrend der Forecast-Transition wird der linke Uhrbereich nun aktiv geloescht und vollstaendig neu gezeichnet, inklusive statischer Ziffern und Sekundenleiste. Dadurch kann der Forecast wieder komplett nach oben ausblenden, ohne sichtbare Reste in der Uhrzeit zu hinterlassen [`87ecbb4`]
- Forecast-Transition korrigiert die eigentliche Ursache: Die bewegte Vorhersage wird nun vor dem Schreiben auf das HUB75-Display auf das Wetterfenster ab `y=21` geclippt, statt die Uhrzeit nachtraeglich zu reparieren. Dadurch kann die alte Zeile komplett nach oben ausblenden, ohne auf dem linken Panel in die Uhrzeit zu schreiben; `pio run` erfolgreich [`186398b`]
- Monitor-Watchdog ergänzt: Ein eigener FreeRTOS-Task ueberwacht jetzt den Heartbeat des Hauptpfads und startet das Geraet neu, wenn es wirklich haengt. OTA bleibt dabei aktiv, weil der Monitor nicht auf `esp_task_wdt` des Loop-Tasks angewiesen ist.
- OTA-Watchdog-Falle beseitigt: Waehrend eines Firmware-Uploads fuettern jetzt die `ArduinoOTA`-Callbacks den Heartbeat mit, damit der Upload nicht selbst als Hänger endet.
- KiCad-Projekt und Fertigungsdaten ins Repo einsortiert: `kicad/source/` enthält die Bearbeitungsquellen fuer Revision `0.2`, `kicad/fab/rev0.2/` die Gerber-, BOM- und Bestell-Artefakte; das komplette Fertigungspaket liegt dort direkt als ZIP, lokale Backups und Cache-Dateien bleiben per `.gitignore` ausgeschlossen.
- In `kicad/README.md` steht jetzt eine kleine lesbare Kurz-BOM aus der exportierten CSV, damit man ohne KiCad sofort die wichtigsten Bestückungsgruppen sieht; ein kleiner Generator liegt in `scripts/render_kicad_bom_summary.py`.

## [1.6.1] - 2026-04-11

### Added
- Webinterface um eine Firmware-Update-Moeglichkeit direkt auf der Uhr erweitert: Im OTA-Tab kann jetzt eine `.bin`-Datei per Browser hochgeladen und direkt eingespielt werden; danach startet das Geraet automatisch neu [`0b8532a`]
- Display-Layout als persistente Konfiguration vorbereitet: intern werden Panel-Spalten, Panel-Zeilen und Mapping-Modus gespeichert, damit spaeter 2x2-/ZigZag-Layouts angebunden werden koennen; im Webinterface sind aktuell bewusst nur `1x HUB75` und `2x HUB75 nebeneinander` auswählbar [`86957cd`]

### Changed
- `HH:MM:SS`-Doppelpunkte um 1 Pixel nach links verschoben, damit der seitliche Abstand zu den Ziffern ausgewogener wirkt [`0d3de8f`]
- Matrix-Initialisierung und dynamische Anzeigeelemente nutzen jetzt die konfigurierte virtuelle Displaybreite statt einer fest verdrahteten Zwei-Panel-Breite; Aenderungen am Panel-Layout werden beim Speichern als neustartpflichtig gemeldet [`86957cd`]
- Bei `1x HUB75` verwendet die `HH:MM:SS`-Anzeige wieder das kompakte Ein-Panel-Layout aus `1.5.4`, damit die Uhrzeit nicht zu gross fuer 64x32 dargestellt wird [`86957cd`]
- Wettertexte werden fuer die Webinterface-/API-Ausgabe wieder nach UTF-8 zurueckgewandelt, waehrend die displayinterne Latin-1-Umlautdarstellung unveraendert bleibt [`86957cd`]
- Die cspell-Konfiguration nutzt jetzt ein projektlokal versioniertes deutsches Wörterbuch, damit README, Changelog und deutschsprachige UI-Texte reproduzierbar geprüft werden können [`83dada4`]

## [1.6.0] - 2026-04-10

### Codeberg
- Release-ID: `8429130` (`v1.6.0`)

### Added
- Zweites HUB75-Panel als Daisy-Chain unterstützt (`DISPLAY_PANELS = 2`) [`bdcf033`]
- `DISPLAY_W`-Konstante für einfache Anpassung der Panelanzahl [`bdcf033`]
- Datum auf rechtes Panel verschoben: Zeile 1 voller Wochentag (zentriert), Zeile 2 `TT.MM.JJJJ` (zentriert) [`bdcf033`]
- `_weekdayFull()` für vollständige Wochentagnamen (DE/EN) [`bdcf033`]

### Changed
- Scroll-Text in der Wetterzeile läuft jetzt über beide Panels (128 px Gesamtbreite) [`bdcf033`]
- Scrolltext des aktuellen Wetters beginnt jetzt mit `Aktuell:` und verwendet Temperaturangaben mit Abstand vor der Einheit, z. B. `+12.4 C` [`bdcf033`]
- Vorhersage-Kacheln zeigen jetzt 3-Buchstaben-Wochentage, Temperatur-Einheiten und Leerzeichen um den Trenner, z. B. `Fre +15 C / +3 C` [`573a364`]
- Uhrzeit links vertikal deutlich vergrößert; nach dem verworfenen 7-Segment-Zwischenschritt nutzen die Zeitmodi jetzt hochskalierte Bitmap-Ziffern mit weicherem Look fuer `HH:MM` und `HH:MM:SS` [`a549f49`]
- Beim Wechsel zwischen `HH:MM` und `HH:MM:SS` wird jetzt immer die maximale Uhrenflaeche geloescht, damit unterhalb der Zeit keine Fragmente des vorherigen Modus stehenbleiben [`4556f6d`]
- In `HH:MM:SS` wird der alte Sekundenbalken-Bereich nicht mehr ueber die Ziffern geloescht; dadurch bleiben Zeile 11 und 12 nicht mehr leer [`5ce5838`]
- Die `HH:MM:SS`-Anzeige wird 3 Pixelreihen tiefer gezeichnet, inklusive Doppelpunkt und Animationspfad; `HH:MM` bleibt unveraendert [`5ce5838`]
- Vertikaler Clip-Bug in der Uhrdarstellung behoben: Der Zeichen-Clip beruecksichtigt jetzt auch den Y-Offset des aktiven Modus, damit unten keine Ziffernreihen mehr abgeschnitten werden [`5ce5838`]
- Bitmap-Ziffern fuer `HH:MM:SS` unten optisch geglaettet: die untere Kante endet jetzt wie oben mit 2 statt 3 Pixelreihen [`5ce5838`]
- OTA-Anzeige exklusiv gemacht: waehrend eines laufenden OTA-Updates blockiert ein eigener Display-Lock alle normalen Uhr-/Wetter-Redraws, damit nichts mehr in den Fortschrittsbildschirm hineinzeichnet und das Update stoert [`5ce5838`]
- README um einen manuellen Update-Abschnitt ohne Arduino IDE / ohne VS Code fuer Linux, macOS und Windows ergaenzt; ausserdem dokumentiert, dass der Browser-Installer ueber `Firmware/latest/manifest.json` bereits auf `1.6.0` zeigt [`c6aa33b`]

## [1.5.4] — 2026-04-06

### Geändert
- HUB75-Startsequenz entschärft: Die aggressive Kaltstart-Verzögerung und zusätzliche frühe Buffer-Resets wurden wieder entfernt, nachdem sie das Einschalt-Flackern eher verlängert haben. Beibehalten wurde nur eine minimale Pin-Vorbereitung vor der Matrix-Initialisierung, damit `OE` das Panel bis zur DMA-Übernahme dunkel hält.
- Boot-Diagnose ergänzt: Der serielle Log gibt jetzt den ESP32-Reset-Grund (`POWERON`, `BROWNOUT`, `WDT` usw.) direkt beim Start aus, um das Power-On-Problem besser eingrenzen zu können.
- HUB75-Panel testweise auf den Matrix-Treiber `FM6126A` umgestellt. Einige 64x32-Panels initialisieren beim Kaltstart nur zuverlässig, wenn deren Schieberegister-/Treiberchip explizit vor dem DMA-Start initialisiert wird.
- Matrix startet jetzt zunächst mit stark reduzierter Boot-Helligkeit und wird erst nach abgeschlossenem Setup auf den konfigurierten Wert angehoben. Das dient als gezielter Test, ob der Kaltstart durch Lastspitzen beim Panel-Scan beeinflusst wird.
- Boot-Helligkeit weiter abgesenkt und der Uebergang zur Zielhelligkeit auf eine kurze Rampe umgestellt. Damit steigt die Panel-Last nach dem Einschalten weicher an, statt sofort sprunghaft auf den konfigurierten Wert zu springen.
- Web-Konfiguration serverseitig abgesichert: numerische Felder werden jetzt auf sinnvolle Bereiche begrenzt, damit fehlerhafte oder leere Werte aus dem Webinterface keine ungueltigen Runtime-Settings erzeugen.
- Factory Reset wird jetzt wie der normale Neustart sicher aus dem Main-Loop ausgefuehrt statt direkt aus dem AsyncWebServer-Callback.
- OTA nach WLAN-Reconnect wieder vollstaendig ueber `otaSetup()` initialisiert, damit Hostname, Passwort und Callback-Hooks konsistent erhalten bleiben.
- Wetter-/HTTP-Fehlerbehandlung gehaertet: HTTP-Antwortcodes werden jetzt sauberer ausgewertet, Fehlertexte konkreter zurueckgegeben und unvollstaendige API-Antworten besser erkannt.
- Forecast-Auswertung robuster gemacht: Tagesdifferenzen werden jetzt ueber echte Kalenderzeiten statt ueber `tm_mday` mit 31-Tage-Fallback berechnet. Das verhindert Fehlzuordnungen an Monats- und Jahreswechseln.
- Geocoding im Webinterface verwendet jetzt dieselbe robustere HTTP-Auswertung wie der Wetterabruf und liefert klarere API-Fehler an den Browser zurueck.
- README fuer die Codeberg-Veroeffentlichung aktualisiert: aktuelle Version, Repository-URL, Lizenzhinweis und Clone-Beispiel ergaenzt; veraltete Kurz-Changelog-Tabelle durch Verweis auf `CHANGELOG.md` ersetzt.
- AGENTS.md um feste Repo-Metadaten fuer Codeberg und die verwendete Lizenz `CC BY-NC-SA 4.0` erweitert.
- `.gitignore` fuer die Veroeffentlichung geschaerft: lokale IDE-Dateien, Build-Artefakte, KI-/Agent-Konfigurationen, Caches und persoenliche Notizen werden jetzt vom Repository ausgeschlossen.
- PlatformIO exportiert nach jedem erfolgreichen Build automatisch einen versionierten Firmware-Ordner unter `Firmware/<Version>/` sowie `Firmware/latest/`, inklusive `bootloader.bin`, `partitions.bin`, `boot_app0.bin`, `firmware.bin` und passender `manifest.json` fuer Web-Flasher.
- `install.html` fuer einen plattformunabhaengigen USB-Erstflash per Browser hinzugefuegt. Die Seite verwendet `ESP Web Tools` und zeigt automatisch auf `Firmware/latest/manifest.json`.
- `index.html` als Startseite fuer statisches Hosting hinzugefuegt und auf `install.html` weitergeleitet, damit Codeberg Pages die Projektseite direkt unter der Repository-URL ausliefern kann.
- README um einen prominent platzierten Browser-Installer-Button erweitert, der direkt auf die Codeberg-Pages-Installationsseite verweist.
- `Firmware/` wird auf `main` nicht mehr versioniert. Der lokale Build-Export bleibt erhalten, ist aber jetzt per `.gitignore` aus dem Quellcode-Branch ausgeschlossen; fuer den Browser-Installer bleibt der Ordner weiterhin auf der `pages`-Branch relevant.
- Lokale Hilfsskripte fuer persoenliche Git-/Pages-Abläufe wieder aus dem Repository entfernt, damit im oeffentlichen Projekt nur allgemein nutzbare Dateien verbleiben.

## [1.5.3] — 2026-03-28

### Behoben
- NTP-Synchronisation zwischen mehreren Uhren: Animationen starten jetzt synchron zur NTP-Sekunde statt zum zufälligen Loop-Zeitpunkt. `_setFlipTime` liest per `gettimeofday()` die exakten Mikrosekunden der aktuellen Sekunde und berechnet daraus den korrekten Startframe (`startFrame`) und Frame-Timer-Offset (`msIntoFrame`). Zwei Uhren am gleichen NTP-Server zeigen dadurch exakt denselben Animationsfortschritt (Restzitter ≤ 1 Frame = 15 ms).
- Frame-Timer-Drift behoben: `d.last_t += ms_per_frame` statt `d.last_t = now` in `_drawFlipClock`. Verhindert akkumulierten Drift bei langsamen Loop-Iterationen und erhält die Synchronisierung über die gesamte Animationsdauer.
- Loop-Verzögerung von 33 ms auf 10 ms reduziert. Geringerer Erkennungs-Jitter beim Sekundenwechsel (~10 ms statt ~33 ms).

---

## [1.5.2] — 2026-03-28

### Behoben
- Slide-Up-Animation: Pixels sind jetzt strikt auf den 8-Zeilen-Ziffernbereich (y=0..8) begrenzt. Clip in `_drawDigitAt` von `dy >= DIGIT_H-1` auf `dy >= 9` geändert.
- Slide-Up-Animation: Keine schwarzen Lücken mehr beim Übergang von alter zu neuer Ziffer. Schritt auf 1px/Frame geändert (statt 2px), Phase 1 (0-8) und Phase 2 (9-16) lückenlos aneinander: Frame 8 zeigt letzte Pixel der alten Ziffer (Row 8 bei dy=0), Frame 9 erste Pixel der neuen (Row 1 bei dy=8).
- Slide-Up-Animation: Neue Ziffer tritt nicht mehr unterhalb des Slots ein ("alte Position"). Phase 2 startet jetzt bei yOff=7 (erste Inhalt-Row bei dy=8, innerhalb des Clips).

### Hinzugefügt
- Animations-Modus "Keine" (anim_mode=2): Ziffernwechsel sofort ohne Animation. Im Webinterface unter Anzeige → Animation auswählbar.

---

## [1.5.1] — 2026-03-24

### Hinzugefügt
- Vorhersage-Kacheln (+1/+2/+3 Tage) scrollen optional von unten in den unteren Panel-Bereich ein (10px, ~200ms, 1px/20ms). Konfigurierbar im Webinterface unter Anzeige: "Vorhersage-Kacheln von unten einscrollen" (Standard: an). Neue Config-Option `forecast_scroll_up`.

### Geändert
- Trennzeichen im Scrolltext zwischen aktueller Bedingung und heutiger Vorhersage von zwei Leerzeichen auf ` / ` geändert: z.B. `+10.1C  Bedeckt / Heute: +14/+2C`.

---

## [1.5.0] — 2026-03-24

### Hinzugefügt
- Heutige Tagesvorhersage (Max/Min) im Scrolltext des aktuellen Wetters: Format `+10.1C Bedeckt  Heute: +14/+2C`. Die Daten werden aus dem 3h-Forecast für den aktuellen Tag (diff==0) berechnet — Max über alle verfügbaren Einträge des Tages, Min ebenso. Fallback auf `current_temp` wenn keine Forecast-Einträge für heute vorhanden sind. `WeatherData` um `DayForecast today` erweitert.

---

## [1.4.9] — 2026-03-23

### Behoben
- Großbuchstaben-Umlaute (Ä/Ö/Ü): Punkte bei y-2 statt y-1, damit 1 leere Pixelreihe Abstand zum Buchstabenkörper (der bei Row 0 der Zeichenzelle beginnt). fillRect ab Row 21 statt 22. Buchstabe bleibt auf normalem y — keine Überlänge mehr.

---

## [1.4.8] — 2026-03-23

### Behoben
- Umlaut-Buchstaben wieder auf normaler y-Position (keine Höhenverschiebung mehr). Punkte basierend auf tatsächlicher Font-Geometrie: Kleinbuchstaben (a/o/u) haben Font-Rows 0-1 leer → Punkte bei y (Row 0), 1px Gap bei y+1. Großbuchstaben haben Font-Row 0 leer → Punkte bei y-1 (über der Zeichenzelle), 1px Gap bei y (Row 0).

---

## [1.4.7] — 2026-03-23

### Behoben
- Umlaut-Abstand vereinheitlicht: Punkte bei y-1, Kleinbuchstaben-Char bei y-1 (Körper bei y+1), Großbuchstaben-Char bei y+1 (Körper bei y+1). Alle 6 Umlaute zeigen exakt 1 leere Pixelzeile zwischen Punkt und Buchstabenkörper.

---

## [1.4.6] — 2026-03-23

### Behoben
- Umlaut-Punkte werden jetzt bei y-1 gezeichnet (eine Zeile über der Zeichenzelle), Buchstabe bleibt bei normalem y. Damit ist der Abstand zwischen Punkt und Buchstabe garantiert sichtbar, unabhängig von der internen Fontgeometrie. Ä/Ö/Ü erscheinen wieder auf gleicher Höhe wie andere Großbuchstaben.

---

## [1.4.5] — 2026-03-23

### Behoben
- Umlaut-Punkte: 1px Abstand zwischen Punkten und Buchstabenkörper (Zwischenversion, durch 1.4.6 ersetzt).

---

## [1.4.4] — 2026-03-23

### Behoben
- Sekundenbalken von y=11-12 auf y=10-11 verschoben — 1px Gap (Zeile 12) zwischen Sekundenbalken und Datumszeile (y=13).

---

## [1.4.3] — 2026-03-23

### Behoben
- `datLastMday`/`datDirty` auf Modulebene verschoben; `displayReset()` setzt sie jetzt zurück. Bugfix: nach WiFi-Reconnect wurde das Datum nicht mehr angezeigt (dateDirty war 0, kein Redraw bis Tageswechsel).

---

## [1.4.2] — 2026-03-23

### Behoben
- Sekundenbalken 2px nach oben (y=11-12), Datum 2px nach oben (y=13), damit 3 Zeilen Abstand vor der Wetterzeile entstehen (Zeilen 20-22).
- Wetter-fillRect startet nun bei y=22, damit Grossbuchstaben-Umlautpunkte (Ae/Oe/Ue) bei y=22 gezeichnet und korrekt geloescht werden.
- `_drawCharEx`: Grossbuchstaben-Umlaute verschieben den Buchstabenkörper auf y+1 und zeichnen Punkte bei y — damit sind Ä/Ö/Ü jetzt vollständig mit Punkten sichtbar.

---

## [1.4.1] — 2026-03-23

### Behoben
- Layout komprimiert: Ziffern-Inhalt 2px nach oben verschoben (Zeilen 1-8 statt 3-10), Sekundenbalken auf y=13, Datum auf y=15, Wetter auf y=23. Damit sind Unterlängen (g, p, q) vollständig sichtbar und Umlaut-Punkte landen nicht mehr am Displayrand.
- Wetter-fillRect auf 9 Zeilen erweitert (Zeilen 23-31), um Unterlängen und Umlaut-Punkte (y+1) zu erfassen.
- Kolons und AM/PM-Indikator entsprechend um 2px nach oben verschoben.

---

## [1.4.0] — 2026-03-23

### Neu
- Echte Umlaut-Darstellung auf dem Display: ä, ö, ü werden als Buchstabe + zwei Diakritik-Punkte gerendert (Pixel bei y+1 im leeren oberen Bereich des 5×7-Font-Cells). Großbuchstaben Ä/Ö/Ü werden ohne Punkte dargestellt. ß bleibt „ss".
- `_sanitizeUtf8()` gibt nun Latin-1-Einzelbytes aus (statt `ae`/`oe`-Ersatz); neue `_drawCharEx()`/`_printStrEx()`-Hilfsfunktionen in `display.h` übernehmen die Pixel-Ausgabe.

---

## [1.3.9] — 2026-03-17

### Behoben
- UTF-8-Umlaute in Wetterbeschreibungen werden korrekt dargestellt (`ä→ae`, `ö→oe`, `ü→ue`, `Ä→Ae`, `Ö→Oe`, `Ü→Ue`, `ß→ss`)

---

## [1.3.8]

### Behoben
- Moduswechsel: beide DMA-Buffer werden geleert — kein Ghosting alter Pixel mehr nach dem Wechsel des Sekundenanzeige-Modus

---

## [1.3.7]

### Behoben
- HH:MM:SS: Kolon-Positionen C1 (x=20→21) und C2 (x=41→42) korrigiert — je 2px symmetrischer Abstand zu den Ziffernblöcken

---

## [1.3.6]

### Behoben
- HH:MM: Kolon-Position auf x=31 gesetzt — je 3px symmetrischer Abstand zu beiden Zifferblöcken
- HH:MM: Zifferpaare neu positioniert (H1=6→H2=18, M1=36→M2=48)

---

## [1.3.5]

### Behoben
- Kolon-Animation: Cross-Dissolve-Puls exakt wie Ziffern (14 Frames à 18 ms)
- Kolon blinkt nicht mehr — bleibt dauerhaft hell, kurzer Puls synchron zum Sekundenwechsel
- Kolon HH:MM: Position x=32 → je 2px Abstand zu beiden Zifferblöcken

---

## [1.3.4]

### Neu
- Kolon-Animation: Cross-Dissolve statt Blink (18 ms/Frame, 7 Helligkeitsstufen)
- Kolon Fade-In synchron zum Sekundenwechsel, Fade-Out nach 500 ms

### Behoben
- Kolon HH:MM: Position x=32 (war x=31)

---

## [1.3.3]

### Neu
- **Dottie-Font** (8×8 klassischer LED-Font) für alle Ziffern
- Farben im Webinterface konfigurierbar — 8 Farbwähler (Ziffern, Datum, Temperatur, Wochentag, Info, Sekundenbalken, 5s-Marke, 10s-Marke)

### Behoben
- HH:MM:SS: 8px-Ziffern, alle 7 Abstände je 1px garantiert (6×8 + 2×2 + 7×1 = 59px)
- Ghosting nach Neustart: `displayReset()` löscht RAM-Überbleibsel (`digitDirty`/`flip`)
- Neustart-Button: Restart sicher aus dem Main-Loop, kein Bootloop mehr
- Farbänderungen wirken sofort (`colorsInit()` wird korrekt aufgerufen)

---

## [1.3.2]

### Neu
- Dirty-Counter (2 Frames) stellt sicher, dass beide DMA-Buffer nach einer Animation korrekt aktualisiert werden

### Behoben
- HH:MM:SS: 9px-Ziffernbreite + 1px-Kolon → 1px Abstand zwischen allen Elementen garantiert
- Ghosting: kein `clearScreen()` mehr pro Frame — nur animierende Slots werden gelöscht

---

## [1.3.1]

### Neu
- HH:MM-Abstände: H1=7, M2=47 → 2px Lücke zwischen Zifferpaaren

### Behoben
- Ziffer „7" neu gezeichnet: voller Querbalken oben + 3-stufige Treppe zur Mitte
- Slide-Animation: Phasen-Formel korrigiert (`frame-6` statt `frame-7`) — kein Sprung mehr beim Übergang
- Gap-Clip korrigiert (`dy >= DIGIT_H-1`) — y=13 bleibt auch während der Animation frei

---

## [1.3.0]

### Neu
- **Komplettes Display-Redesign**: 10×14px Pixel-Bitmap-Font statt 7-Segment-Rechtecken
- Slide-Up- und Cross-Dissolve-Animation mit Bitmap-Ziffern
- **Sekundenanzeige**: vier Modi — Bar / Wanderpunkt (Pulse) / Dots / HH:MM:SS (6 Ziffern)
- HH:MM:SS-Modus (`seconds_mode=3`) mit 6 Ziffern kantenbündig
- Neues Layout: y=0–13 Uhr, y=14–15 Sekunden, y=16 Gap, y=17–23 Datum, y=24 Gap, y=25–31 Wetter

---

## [1.2.0]

### Neu
- OTA-Fortschrittsbalken auf dem Display während des Updates
- 12h/24h-Zeitformat umschaltbar (AM/PM-Indikator als 4×6px-Block)
- Datumsformat wählbar: `Mo TT.MM.JJ` / `Mo TT.MM` / `TT.MM.JJJJ` / `MM/TT/JJ` (US)
- Wetter-Standort im Status-Tab sichtbar (zur Diagnose von Temperaturabweichungen)

---

## [1.1.3]

### Neu
- Vorhersage statisch angezeigt (kein Scrollen)
- Temperatur in der Vorhersage ganzzahlig gerundet

---

## [1.1.2]

### Neu
- Scroll-Geschwindigkeit konfigurierbar (`scroll_speed_ms`, Standard: 80 ms/Pixel)

### Behoben
- Wetter-Scroll wartet auf vollständigen Durchlauf vor dem Moduswechsel
- Sekundenleiste zentriert (x=2..61), 1px/Sek, kein Cyan am Anfang
- 5s-Marken jetzt grün (besser von Amber unterscheidbar)
- Datum: 2px Abstand Wochentag ↔ Datum, Trennpunkte manuell mit 2px Abstand

---

## [1.1.1]

### Neu
- Aktiver NTP-Server im Webinterface angezeigt (`sntp_getreachability`)

### Behoben
- Wetterbeschreibung kommt direkt von der OWM-API (kein internes Mapping mehr)

---

## [1.1.0]

### Neu
- **Double-Buffering** → kein Flackern / Tearing mehr
- Sekunden-Fortschrittsbalken unter der Uhr
- IP-Adresse scrollt beim Start (wie bei Dottie)
- Flip-Clock zentriert in 64px
- Datum (Wochentag + Datum) und Wetter-Zeile getrennt, kein Überlapp

### Behoben
- Datumsformat mit 2-stelligem Jahr; Wochentag in Gelb

---

## [1.0.2]

### Behoben
- IP-Adresse auf 2 Zeilen aufgeteilt
- Flacker-Reduktion
- mDNS-Label ergänzt

---

## [1.0.1]

### Behoben
- `clearScreen()` löschte AP-Display
- `WiFi.disconnect()` fehlte vor AP-Start

---

## [1.0.0]

### Neu
- Initiale Veröffentlichung
- HUB75 64×32 Display-Ansteuerung via DMA
- NTP-Zeitsynchronisation
- Captive-Portal für WLAN-Erstkonfiguration
- OpenWeatherMap-Wetteranzeige
- Webinterface zur Konfiguration
