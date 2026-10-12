#!/usr/bin/env python3
"""Print a progress summary from an objdiff report (build/report.json)."""
import json
import sys


def pct(part, whole):
    return 100.0 * part / whole if whole else 0.0


def main() -> None:
    report = json.load(open(sys.argv[1] if len(sys.argv) > 1 else "build/report.json"))
    m = report["measures"]
    code, matched = int(m.get("total_code", 0)), int(m.get("matched_code", 0))
    funcs, matched_funcs = int(m.get("total_functions", 0)), int(m.get("matched_functions", 0))
    data, matched_data = int(m.get("total_data", 0)), int(m.get("matched_data", 0))
    print(f"Code:      {matched:>9,} / {code:,} bytes  ({pct(matched, code):.3f}%)")
    print(f"Functions: {matched_funcs:>9,} / {funcs:,}  ({pct(matched_funcs, funcs):.3f}%)")
    print(f"Data:      {matched_data:>9,} / {data:,} bytes  ({pct(matched_data, data):.3f}%)")
    print(f"Fuzzy:     {m.get('fuzzy_match_percent', 0):.3f}%")
    print(f"decomp.dev lists a project once it reaches 0.5% matched code ({int(code * 0.005):,} bytes).")


if __name__ == "__main__":
    main()
