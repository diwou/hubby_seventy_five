# Releasing

Diese Datei ist die dauerhafte Checkliste fuer KiCad- und Projekt-Releases.

## Grundsaetze

- KiCad-Backups, Cache-Dateien und lokale Zwischenstaende bleiben ausserhalb des Repos oder werden durch `.gitignore` ausgeschlossen.
- Die aktuelle bearbeitbare Revision liegt unter `kicad/source/`.
- Die zugehoerigen Fertigungsdaten liegen unter `kicad/fab/revX.Y/`.
- Pro Revision gehoeren Schaltplan, PCB, BOM, Positionsdaten, Netlist und ZIP-Paket zusammen.

## Dauerhafte ToDos

1. Vor dem Commit pruefen, ob nur die aktuelle Revision im Repo liegt.
2. Alle Backup-Ordner und Cache-Dateien entfernen oder ignorieren.
3. Sicherstellen, dass `README.md` und `kicad/README.md` auf die richtige Revision zeigen.
4. `CHANGELOG.md` um die inhaltliche Aenderung ergaenzen.
5. Wenn die KiCad-Revision wechselt, die neue Revision im gesamten Dokumentationssatz konsistent machen.
6. Vor einem Release das ZIP-Paket mit den exportierten Gerbern und Bestelldaten abgleichen.

## Aktueller Stand

- Aktive KiCad-Revision: `rev0.3`
- Bearbeitbare Quellen: `kicad/source/`
- Fertigungsdaten: `kicad/fab/rev0.3/`
- Zur Revision gehoeriges Paket: `kicad/fab/rev0.3/Hubby_Seventy_Five_0.3.zip`
