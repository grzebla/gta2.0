#!/usr/bin/env python3
"""Checks the slope-code table in docs/specs/world-slopes.md (section 3).

Always: the table lists every code 0-63 exactly once. With an inventory report (saved output of
`gta20-headless inventory`, T0010) also: the per-code cell total over all maps, the number of maps
using the code and the wil/ste/bil cell counts equal the inventory. Prints only the verdict and
mismatching codes.

Usage: python3 -I tools/map-analysis/check_slope_table.py <world-slopes.md> [inventory.txt]
"""
import collections
import re
import sys

ROW = re.compile(r"^\| (\d+) \| [^|]+ \| [^|]+ \| ([\d ]+) \((\d+)\) \| ([\d ]+)/([\d ]+)/([\d ]+) \|")
CITIES = ("wil.gmp", "ste.gmp", "bil.gmp")


def table_rows(path):
    rows, inside = [], False
    with open(path, encoding="utf-8") as f:
        for line in f:
            if line.startswith("## "):
                inside = line.startswith("## 3.")
                continue
            m = ROW.match(line) if inside else None
            if m:
                rows.append([int(g.replace(" ", "")) for g in m.groups()])
    return rows


def inventory_counts(path):
    maps, current = {}, None
    with open(path) as f:
        for line in f:
            m = re.match(r"map data/(\S+): ", line)
            if m:
                current = m.group(1).lower()
                maps[current] = collections.Counter()
            elif current and line.startswith("  slope codes (cells):"):
                for code, cells in re.findall(r"(\d+)=(\d+)", line.split(":", 1)[1]):
                    maps[current][int(code)] = int(cells)
    return maps


def main(argv):
    if len(argv) not in (2, 3):
        print("usage: check_slope_table.py <world-slopes.md> [inventory.txt]", file=sys.stderr)
        return 2
    rows = table_rows(argv[1])
    codes = [r[0] for r in rows]
    problems = []
    if sorted(codes) != list(range(64)):
        duplicated = sorted(c for c, n in collections.Counter(codes).items() if n > 1)
        missing = sorted(set(range(64)) - set(codes))
        problems.append("codes: %d rows, duplicated %s, missing %s" % (len(rows), duplicated, missing))
    if len(argv) == 3:
        maps = inventory_counts(argv[2])
        if not all(c in maps for c in CITIES):
            problems.append("inventory lacks one of %s" % (CITIES,))
        else:
            for code, cells, used_in, *cities in rows:
                expected = [sum(m[code] for m in maps.values()), sum(1 for m in maps.values() if m[code])]
                expected += [maps[c][code] for c in CITIES]
                if [cells, used_in] + cities != expected:
                    problems.append("code %d: table %s, inventory %s" % (code, [cells, used_in] + cities, expected))
    print("slope table: %d rows, %s" % (len(rows), "PASS" if not problems else "FAIL"))
    for problem in problems[:20]:
        print("  " + problem)
    return 0 if not problems else 1


if __name__ == "__main__":
    sys.exit(main(sys.argv))
