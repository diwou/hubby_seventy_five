#!/usr/bin/env python3
from __future__ import annotations

import argparse
import csv
import pathlib
import re
import sys


ROOT = pathlib.Path(__file__).resolve().parents[1]
DEFAULT_CSV = ROOT / "kicad" / "fab" / "rev0.2" / "bom" / "bom.csv"

CATEGORIES = [
    ("MCU", lambda ds: "U5" in ds, "Hauptprozessor, WLAN, Steuerlogik und PSRAM"),
    ("Takt / Sensorik", lambda ds: any(d in ds for d in ("U2", "U4", "U21")), "Echtzeituhr, Temperatur/Luftfeuchte/Luftdruck, Helligkeit"),
    ("Pegelwandler", lambda ds: any(d in ds for d in ("U3", "U6")), "3,3 V auf 5 V fuer HUB75-Signale"),
    ("Spannungsversorgung", lambda ds: any(d in ds for d in ("U1", "D1", "BT1")), "3,3-V-Regler, Schutzdiode, Backup-Batterie"),
    ("Anschlüsse", lambda ds: any(d in ds for d in ("J1", "J2", "J3")), "Strom, USB, Display-Anschluss"),
    ("Taster", lambda ds: any(d in ds for d in ("SW1", "SW2", "SW3")), "Bedienung und Flash-Modus"),
    ("Widerstände", lambda ds: all(d.startswith("R") for d in ds), "Pullups, LED-/Signalbegrenzung, Schutz"),
    ("Kondensatoren", lambda ds: all(d.startswith("C") for d in ds), "Abblockung und Pufferung"),
    ("Anzeige / Status", lambda ds: "D2" in ds, "Power-LED"),
]


def split_designators(text: str) -> list[str]:
    return [part.strip() for part in text.replace('"', "").split(",") if part.strip()]


def clean_label(text: str) -> str:
    return text.replace('"', "").strip()


def designator_sort_key(designator: str):
    match = re.match(r"([A-Za-z]+)(\d+)$", designator)
    if not match:
        return (designator, -1)
    prefix, number = match.groups()
    return (prefix, int(number))


def load_rows(csv_path: pathlib.Path):
    with csv_path.open("r", encoding="utf-8-sig", newline="") as fh:
        reader = csv.DictReader(fh)
        rows = []
        for row in reader:
            designators = split_designators(row["Designator"])
            rows.append({
                "designators": designators,
                "footprint": clean_label(row["Footprint"]),
                "quantity": int(row["Quantity"]),
                "value": clean_label(row["Value"]),
            })
        rows.sort(key=lambda row: designator_sort_key(row["designators"][0]))
        return rows


def build_summary(rows):
    summary = []
    for category, predicate, purpose in CATEGORIES:
        matched = [row for row in rows if predicate(set(row["designators"]))]
        if not matched:
            continue
        matched.sort(key=lambda row: designator_sort_key(row["designators"][0]))
        pieces = []
        footprints = []
        total_qty = 0
        for row in matched:
            designators = "/".join(sorted(row["designators"], key=designator_sort_key))
            pieces.append(f"`{designators}` {row['value']}")
            footprints.append(row["footprint"])
            total_qty += row["quantity"]
        summary.append((category, ", ".join(pieces), ", ".join(dict.fromkeys(sorted(footprints))), total_qty, purpose))
    return summary


def render_markdown(summary):
    lines = []
    lines.append("| Bereich | Bauteile | Bauform | Menge | Zweck |")
    lines.append("|---|---|---|---:|---|")
    for category, parts, footprints, qty, purpose in summary:
        lines.append(f"| {category} | {parts} | {footprints} | {qty} | {purpose} |")
    return "\n".join(lines)


def update_readme(readme_path: pathlib.Path, table_md: str) -> None:
    start_marker = "<!-- BOM_SUMMARY_START -->"
    end_marker = "<!-- BOM_SUMMARY_END -->"
    text = readme_path.read_text(encoding="utf-8")
    pattern = re.compile(
        rf"{re.escape(start_marker)}.*?{re.escape(end_marker)}",
        re.DOTALL,
    )
    replacement = f"{start_marker}\n\n{table_md}\n\n{end_marker}"
    if not pattern.search(text):
        raise SystemExit(
            f"Markers not found in {readme_path}. Expected {start_marker} and {end_marker}."
        )
    readme_path.write_text(pattern.sub(replacement, text, count=1), encoding="utf-8")


def parse_args(argv: list[str]) -> argparse.Namespace:
    parser = argparse.ArgumentParser(description="Render a BOM summary from KiCad CSV exports")
    parser.add_argument("csv_path", nargs="?", type=pathlib.Path, default=DEFAULT_CSV)
    parser.add_argument("--write-readme", action="store_true", help="Update kicad/README.md in place")
    parser.add_argument("--readme", type=pathlib.Path, default=ROOT / "kicad" / "README.md")
    return parser.parse_args(argv[1:])


def main(argv: list[str]) -> int:
    args = parse_args(argv)
    csv_path = args.csv_path
    if not csv_path.exists():
        print(f"CSV not found: {csv_path}", file=sys.stderr)
        return 1
    rows = load_rows(csv_path)
    summary = build_summary(rows)
    table_md = render_markdown(summary)
    if args.write_readme:
        update_readme(args.readme, table_md)
    else:
        print(table_md)
    return 0


if __name__ == "__main__":
    raise SystemExit(main(sys.argv))
