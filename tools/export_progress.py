#!/usr/bin/env python3
"""Export the objdiff report as flat JSON tables for the progress dashboard.

Usage: python3 tools/export_progress.py [build/report.json] [outdir]

Writes, into outdir (default build/progress/):
  functions.json  one row per function: unit, name, address, size, match
  units.json      one row per translation unit with code totals
  summary.json    one row with the project-wide measures
"""
import json
import re
import subprocess
import sys
from datetime import datetime, timezone
from pathlib import Path

NM = "tools/binutils/mips-ps2-decompals-nm"
ELF = "build/SLUS_204.48.elf"


def symbol_addresses() -> dict[str, int]:
    """Absolute address of every function symbol in the linked ELF."""
    out = subprocess.run([NM, "--defined-only", ELF], capture_output=True, text=True, check=True).stdout
    addrs = {}
    for line in out.splitlines():
        parts = line.split()
        if len(parts) == 3 and parts[1] in "Tt":
            addrs.setdefault(parts[2], int(parts[0], 16))
    return addrs


def main() -> None:
    report_path = Path(sys.argv[1] if len(sys.argv) > 1 else "build/report.json")
    out_dir = Path(sys.argv[2] if len(sys.argv) > 2 else "build/progress")
    out_dir.mkdir(parents=True, exist_ok=True)
    report = json.loads(report_path.read_text())
    addrs = symbol_addresses()

    functions, units = [], []
    for unit in report["units"]:
        m = unit.get("measures", {})
        total_code = int(m.get("total_code", 0))
        if not total_code:
            continue  # data-only units have nothing to draw
        unit_funcs = []
        for f in unit.get("functions", []):
            size = int(f.get("size", 0))
            if not size:
                continue
            name = f["name"]
            unit_funcs.append(
                {
                    "unit": unit["name"],
                    "name": f.get("metadata", {}).get("demangled_name") or name,
                    "symbol": name,
                    "address": f"0x{addrs[name]:08X}" if name in addrs else "",
                    "size": size,
                    "match": round(float(f.get("fuzzy_match_percent", 0.0)), 2),
                }
            )
        unit_funcs.sort(key=lambda r: r["address"])
        functions.extend(unit_funcs)
        start = min((r["address"] for r in unit_funcs if r["address"]), default="")
        units.append(
            {
                "unit": unit["name"],
                "start": start,
                "code_bytes": total_code,
                "matched_bytes": int(m.get("matched_code", 0)),
                "functions": int(m.get("total_functions", 0)),
                "matched_functions": int(m.get("matched_functions", 0)),
                "match": round(float(m.get("fuzzy_match_percent", 0.0)), 2),
                "has_source": bool(unit.get("metadata", {}).get("source_path")),
            }
        )

    pm = report["measures"]
    summary = [
        {
            "generated": datetime.now(timezone.utc).strftime("%Y-%m-%dT%H:%M:%SZ"),
            "code_bytes": int(pm.get("total_code", 0)),
            "matched_bytes": int(pm.get("matched_code", 0)),
            "functions": int(pm.get("total_functions", 0)),
            "matched_functions": int(pm.get("matched_functions", 0)),
            "units": int(pm.get("total_units", 0)),
            "complete_units": int(pm.get("complete_units", 0)),
            "fuzzy_match": round(float(pm.get("fuzzy_match_percent", 0.0)), 3),
        }
    ]

    for name, rows in (("functions", functions), ("units", units), ("summary", summary)):
        path = out_dir / f"{name}.json"
        path.write_text(json.dumps(rows, separators=(",", ":")))
        print(f"wrote {path} ({len(rows)} rows, {path.stat().st_size:,} bytes)")


if __name__ == "__main__":
    main()
