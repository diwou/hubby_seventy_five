# KiCad Hardware

Dieses Verzeichnis enthält die Platinenquellen und die exportierten Fertigungsdaten für den HUB75 Flip Clock.

## Struktur

- `source/` - bearbeitbare KiCad-Quellen für Revision `0.2`
- `fab/rev0.2/gerber/` - Gerber-, Bohr- und Layer-Exporte
- `fab/rev0.2/bom/` - BOM, Designatoren, Positionen und Netlist
- `fab/rev0.2/Hubby_Seventy_Five_0.2.zip` - komplettes Fertigungspaket für den Bestelldienstleister

## Kurz-BOM

_Abgeleitet aus `fab/rev0.2/bom/bom.csv` und per `scripts/render_kicad_bom_summary.py` generiert. Falls sich die Platine ändert, die Übersicht mit `python3 scripts/render_kicad_bom_summary.py --write-readme` neu schreiben._

<!-- BOM_SUMMARY_START -->

| Bereich | Bauteile | Bauform | Menge | Zweck |
|---|---|---|---:|---|
| MCU | `U5` ESP32-S3-WROOM-1 | ESP32-S3-WROOM-1 | 1 | Hauptprozessor, WLAN, Steuerlogik und PSRAM |
| Takt / Sensorik | `U2` DS3231S, `U4` BME680, `U21` VEML7700-TT | Bosch_LGA-8_3x3mm_P0.8mm_ClockwisePinNumbering, SOIC-16W_7.5x10.3mm_P1.27mm, XDCR_VEML7700-TT | 3 | Echtzeituhr, Temperatur/Luftfeuchte/Luftdruck, Helligkeit |
| Pegelwandler | `U3/U6` 74HCT245 | TSSOP-20_4.4x6.5mm_P0.65mm | 2 | 3,3 V auf 5 V fuer HUB75-Signale |
| Spannungsversorgung | `BT1` CR2032, `D1` SS14, `U1` TLV76133DCY | BatteryHolder_MYOUNG_BS-07-A1BJ001_CR2032, D_SMA, SOT-223-3_TabPin2 | 3 | 3,3-V-Regler, Schutzdiode, Backup-Batterie |
| Anschlüsse | `J1` USB_C_Receptacle_USB2.0_16P, `J2` PWR IN, `J3` HUB75 Connector | IDC-Header_2x08_P2.54mm_Vertical, JST_VH_B4P-VH_1x04_P3.96mm_Vertical, USB_C_Receptacle_GCT_USB4105-xx-A_16P_TopMnt_Horizontal | 3 | Strom, USB, Display-Anschluss |
| Taster | `SW1` Reset, `SW2` Factory Reset, `SW3` Boot | SW_PUSH_6mm | 3 | Bedienung und Flash-Modus |
| Widerstände | `R1/R2` 5k1, `R3/R4/R5` 4k7, `R7` 1k, `R6/R8/R9/R10/R11` 10k, `R27` 10 | 0805 | 12 | Pullups, LED-/Signalbegrenzung, Schutz |
| Kondensatoren | `C1/C2/C3/C4/C7/C9/C10/C11/C13/C28` 100n, `C12` 1µ, `C5/C6/C8/C27` 10µ | 0805 | 15 | Abblockung und Pufferung |
| Anzeige / Status | `D2` PWR | 0805 | 1 | Power-LED |

<!-- BOM_SUMMARY_END -->

Hinweise:
- `H1` bis `H4` sind Montagebohrungen und tauchen deshalb nicht in der BOM-CSV auf.
- Die vollständige, exportierte Stückliste liegt zusätzlich als CSV unter `fab/rev0.2/bom/`.
- `U5` muss ein ESP32-S3-WROOM-1 mit PSRAM sein; das Projekt nutzt die PSRAM-Variante des Moduls.

## Hinweise

- Lokale KiCad-Backups bleiben per `.gitignore` ausgeschlossen.
- Die exportierten Fertigungsdaten sind bewusst im Repo, damit man sie ohne KiCad direkt bestellen oder prüfen kann.
