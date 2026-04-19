# KiCad Hardware

Dieses Verzeichnis enthält die Platinenquellen und die exportierten Fertigungsdaten für den HUB75 Flip Clock.

## Struktur

- `source/` - bearbeitbare KiCad-Quellen für Revision `0.2`
- `fab/rev0.2/gerber/` - Gerber-, Bohr- und Layer-Exporte
- `fab/rev0.2/bom/` - BOM, Designatoren, Positionen und Netlist
- `fab/rev0.2/Hubby_Seventy_Five_0.2.zip` - komplettes Fertigungspaket für den Bestelldienstleister

## Kurz-BOM

| Bereich | Bauteile | Menge | Zweck |
|---|---|---:|---|
| MCU | `U5` ESP32-S3-WROOM-1 | 1 | Hauptprozessor, WLAN und Steuerlogik |
| Takt / Sensorik | `U2` DS3231S, `U4` BME680, `U21` VEML7700-TT | 3 | Echtzeituhr, Temperatur/Luftfeuchte/Luftdruck, Helligkeit |
| Pegelwandler | `U3`, `U6` 74HCT245 | 2 | 3,3 V auf 5 V fuer HUB75-Signale |
| Spannungsversorgung | `U1` TLV76133DCY, `D1` SS14, `BT1` CR2032 | 3 | 3,3-V-Regler, Schutzdiode, Backup-Batterie |
| Anschlüsse | `J1` USB-C, `J2` JST-VH 4-polig, `J3` IDC 2x8 HUB75 | 3 | Strom, USB, Display-Anschluss |
| Taster | `SW1` Reset, `SW2` Factory Reset, `SW3` Boot | 3 | Bedienung und Flash-Modus |
| Widerstände | `R1/R2` 5k1, `R3/R4/R5` 4k7, `R6/R8/R9/R10/R11` 10k, `R7` 1k, `R27` 10R | 12 | Pullups, LED-/Signalbegrenzung, Schutz |
| Kondensatoren | 10x 100n, 4x 10u, 1x 1u | 15 | Abblockung und Pufferung |
| Anzeige / Status | `D2` 0805 PWR | 1 | Power-LED |

Hinweise:
- `H1` bis `H4` sind Montagebohrungen und tauchen deshalb nicht in der BOM-CSV auf.
- Die vollständige, exportierte Stückliste liegt zusätzlich als CSV unter `fab/rev0.2/bom/`.

## Hinweise

- Lokale KiCad-Backups bleiben per `.gitignore` ausgeschlossen.
- Die exportierten Fertigungsdaten sind bewusst im Repo, damit man sie ohne KiCad direkt bestellen oder prüfen kann.
