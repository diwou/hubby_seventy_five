# KiCad Hardware

Dieses Verzeichnis enthält die Platinenquellen und die exportierten Fertigungsdaten für den HUB75 Flip Clock.

## Struktur

- `source/` - bearbeitbare KiCad-Quellen für Revision `0.2`
- `fab/rev0.2/gerber/` - Gerber-, Bohr- und Layer-Exporte
- `fab/rev0.2/bom/` - BOM, Designatoren, Positionen und Netlist
- `fab/rev0.2/archive/` - kompletter Fertigungs-ZIP der Revision

## Hinweise

- Lokale KiCad-Backups bleiben per `.gitignore` ausgeschlossen.
- Die exportierten Fertigungsdaten sind bewusst im Repo, damit man sie ohne KiCad direkt bestellen oder prüfen kann.
