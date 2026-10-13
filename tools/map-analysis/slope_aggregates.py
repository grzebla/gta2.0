#!/usr/bin/env python3
"""Bounded per-slope-code aggregates over the original GTA 2 maps (read-only, local use only).

Prints, for every slope code 0-63, only counts and percentages: cells, ground types, face presence
(non-zero tile, tile 1023, flat bit) and the most frequent neighbour classes in the four horizontal
directions and directly below/above. Codes 49-52 are also split by lid tile 1023. No block records,
tile images or coordinates are printed. Used by docs/specs/world-slopes.md.

With --inventory FILE (the saved output of `gta20-headless inventory`, T0010) the per-map,
per-code cell counts and lid-1023 counts are compared with the inventory; any mismatch fails.

Usage: python3 -I tools/map-analysis/slope_aggregates.py <game-dir> [--inventory FILE]
"""
import collections
import os
import re
import struct
import sys

W, H, LEVELS = 256, 256, 8
TOP_N = 3
# Directions follow docs/specs/world-geometry.md section 2: left/right/top/bottom = -x/+x/-y/+y.
DIRECTIONS = (("-x", -1, 0), ("+x", 1, 0), ("-y", 0, -1), ("+y", 0, 1))
GROUNDS = ("air", "road", "pavement", "field")
FAMILIES = ((1, 8, "1-8"), (9, 40, "9-40"), (41, 44, "41-44"), (45, 48, "45-48"), (49, 52, "49-52"),
            (53, 61, "53-61"), (62, 62, "62"), (63, 63, "63"))


def chunks(data):
    if data[:4] != b"GBMP" or struct.unpack_from("<H", data, 4)[0] != 500:
        raise ValueError("not a GBMP 500 map")
    at = 6
    while at + 8 <= len(data):
        kind = data[at:at + 4].decode("latin-1")
        size = struct.unpack_from("<I", data, at + 4)[0]
        yield kind, data[at + 8:at + 8 + size]
        at += 8 + size


def load_dmap(path):
    with open(path, "rb") as f:
        data = f.read()
    dmap = dict(chunks(data)).get("DMAP")
    if dmap is None:
        raise ValueError(path + ": no DMAP chunk")
    base = struct.unpack_from("<%dI" % (W * H), dmap, 0)
    at = W * H * 4
    column_words = struct.unpack_from("<I", dmap, at)[0]
    columns = dmap[at + 4:at + 4 + column_words * 4]
    at += 4 + column_words * 4
    num_blocks = struct.unpack_from("<I", dmap, at)[0]
    blocks = [struct.unpack_from("<5H2B", dmap, at + 4 + 12 * i) for i in range(num_blocks)]
    grid = [[[0] * W for _ in range(H)] for _ in range(LEVELS)]
    for c in range(W * H):
        off = base[c] * 4
        height, offset = columns[off], columns[off + 1]
        for z in range(offset, height):
            grid[z][c // W][c % W] = struct.unpack_from("<I", columns, off + 4 + 4 * (z - offset))[0]
    return blocks, grid


def empty(b):
    return not any(b)


def neighbour_class(blocks, grid, x, y, z):
    if not (0 <= x < W and 0 <= y < H and 0 <= z < LEVELS):
        return "edge"
    b = blocks[grid[z][y][x]]
    if empty(b):
        return "empty"
    code = b[6] >> 2
    if code == 0:
        return "0+lid" if b[4] & 0x3FF else "0-nolid"
    return str(code)


def family(cls):
    if not cls.isdigit():
        return cls
    code = int(cls)
    return next(name for low, high, name in FAMILIES if low <= code <= high)


def pct(n, total):
    return "%.1f%%" % (100.0 * n / total) if total else "-"


def top(counter, total):
    return " ".join("%s:%s" % (k, pct(v, total)) for k, v in counter.most_common(TOP_N))


def add_cell(s, blocks, grid, b, x, y, z):
    left, right, top_, bottom, lid, _arrows, slope_type = b
    s["n"]["cells"] += 1
    s["ground"][GROUNDS[slope_type & 3]] += 1
    lid_tile = lid & 0x3FF
    s["face"]["lid!=0"] += lid_tile != 0
    s["face"]["lid=1023"] += lid_tile == 1023
    s["face"]["lid flat"] += (lid & 0x1000) != 0
    for label, word in (("left", left), ("right", right), ("top", top_), ("bottom", bottom)):
        s["face"][label + "!=0"] += (word & 0x3FF) != 0
        s["face"][label + " flat"] += (word & 0x1000) != 0
    if slope_type >> 2 == 0:
        return  # neighbour statistics only for slope codes
    for label, dx, dy in DIRECTIONS:
        s[label][neighbour_class(blocks, grid, x + dx, y + dy, z)] += 1
    below = neighbour_class(blocks, grid, x, y, z - 1)
    above = neighbour_class(blocks, grid, x, y, z + 1)
    s["below"][below] += 1
    s["above"][above] += 1
    s["below family"][family(below)] += 1
    s["above family"][family(above)] += 1


def print_group(name, s, maps):
    n = s["n"]["cells"]
    print("%s: cells %d in %d maps; ground %s" % (name, n, maps, top(s["ground"], n)))
    print("  faces " + " ".join("%s=%s" % (k, pct(s["face"][k], n)) for k in sorted(s["face"])) +
          " (lid=1023 cells %d)" % s["face"]["lid=1023"])
    if "below" in s:
        for label in ("-x", "+x", "-y", "+y", "below", "above", "below family", "above family"):
            print("  %-12s %s" % (label, top(s[label], n)))


def parse_inventory(path):
    """Returns {map name (lower case): (per-code cells, lid-1023 cells)} from an inventory report."""
    result, current = {}, None
    with open(path) as f:
        for line in f:
            m = re.match(r"map data/(\S+): ", line)
            if m:
                current = m.group(1).lower()
                result[current] = [collections.Counter(), None]
            elif current and line.startswith("  slope codes (cells):"):
                for code, cells in re.findall(r"(\d+)=(\d+)", line.split(":", 1)[1]):
                    result[current][0][int(code)] = int(cells)
            elif current and line.startswith("  reserved faces (cells):"):
                result[current][1] = int(re.search(r"lid 1023=(\d+)", line).group(1))
    return result


def main(argv):
    args = argv[1:]
    inventory = None
    if len(args) == 3 and args[1] == "--inventory":
        inventory = parse_inventory(args[2])
        args = args[:1]
    if len(args) != 1:
        print("usage: slope_aggregates.py <game-dir> [--inventory FILE]", file=sys.stderr)
        return 2
    data_dir = os.path.join(args[0], "data")
    available = {n.lower(): n for n in os.listdir(data_dir) if n.lower().endswith(".gmp")}
    stats = collections.defaultdict(lambda: collections.defaultdict(collections.Counter))
    maps_with = collections.defaultdict(set)
    per_map = {}
    for name in sorted(available):
        blocks, grid = load_dmap(os.path.join(data_dir, available[name]))
        codes, lid_markers = collections.Counter(), 0
        for z in range(LEVELS):
            for y in range(H):
                for x in range(W):
                    b = blocks[grid[z][y][x]]
                    if empty(b):
                        continue
                    code = b[6] >> 2
                    codes[code] += 1
                    lid_markers += (b[4] & 0x3FF) == 1023
                    maps_with[code].add(name)
                    add_cell(stats[code], blocks, grid, b, x, y, z)
                    if 49 <= code <= 52:
                        split = "%d lid=1023" % code if (b[4] & 0x3FF) == 1023 else "%d lid!=1023" % code
                        maps_with[split].add(name)
                        add_cell(stats[split], blocks, grid, b, x, y, z)
        per_map[name] = (codes, lid_markers)
    print("maps: %d (%s)" % (len(per_map), " ".join(sorted(per_map))))
    for code in range(64):
        if code not in stats:
            print("code %d: cells 0" % code)
            continue
        print_group("code %d" % code, stats[code], len(maps_with[code]))
    for code in range(49, 53):
        for split in ("%d lid=1023" % code, "%d lid!=1023" % code):
            print_group("split " + split, stats[split], len(maps_with[split]))
    if inventory is None:
        return 0
    problems = []
    if sorted(inventory) != sorted(per_map):
        problems.append("map sets differ: inventory %s, here %s" % (sorted(inventory), sorted(per_map)))
    for name in sorted(set(inventory) & set(per_map)):
        inv_codes, inv_markers = inventory[name]
        codes, markers = per_map[name]
        if +inv_codes != +codes:
            problems.append("%s: slope-code cells differ" % name)
        if inv_markers != markers:
            problems.append("%s: lid 1023 cells %s (inventory) != %d" % (name, inv_markers, markers))
    total = sum((c for c, _ in per_map.values()), collections.Counter())
    print("inventory cross-check: %d maps, %d cells, %d slope codes present, %d lid-1023 cells: %s" %
          (len(per_map), sum(total.values()), len(total), sum(m for _, m in per_map.values()),
           "PASS" if not problems else "FAIL"))
    for problem in problems[:20]:
        print("  " + problem)
    return 0 if not problems else 1


if __name__ == "__main__":
    sys.exit(main(sys.argv))
