#!/usr/bin/env python3
"""Per-phase function coverage diff from a coverage.lua error.log.

Usage: cov_diff.py RUN_DIR/error.log --new PHASE[,PHASE...] [--against PHASE,...] [--symbols CSV]

Prints functions first entered in the --new phases that never appeared in the
--against phases (default: every phase that precedes the first --new phase),
sorted by address with symbol names and address-cluster breaks.
"""
import argparse, collections, csv, re
from pathlib import Path

def parse(path):
    phase, seen, order = "boot", collections.OrderedDict(), []
    seen[phase] = set(); order.append(phase)
    for line in open(path, errors="replace"):
        m = re.search(r"COVPHASE (\w+)", line)
        if m:
            phase = m.group(1); seen.setdefault(phase, set()); order.append(phase); continue
        m = re.search(r"COV ([0-9a-fA-F]+)", line)
        if m: seen[phase].add(int(m.group(1), 16))
    return seen, order

def main():
    ap = argparse.ArgumentParser()
    ap.add_argument("log"); ap.add_argument("--new", required=True); ap.add_argument("--against")
    ap.add_argument("--symbols", default=str(Path(__file__).resolve().parents[2] / "ghidra/symbols/3210.csv"))
    ap.add_argument("--gap", type=lambda s: int(s, 0), default=0x800, help="cluster break distance")
    a = ap.parse_args()
    seen, order = parse(a.log)
    new = a.new.split(",")
    against = a.against.split(",") if a.against else order[:order.index(new[0])]
    names = {}
    with open(a.symbols) as f:
        for row in csv.reader(f):
            if len(row) >= 3 and row[0] != "address":
                names[int(row[0], 16) & ~1] = row[2]
    base = set().union(*(seen[p] for p in against))
    result = {}
    for p in new:
        for addr in seen[p] - base:
            result.setdefault(addr, p)
    print(f"# against={','.join(against)} ({len(base)} fns)  new={','.join(new)}  -> {len(result)} functions")
    last = None
    for addr in sorted(result):
        if last is not None and addr - last > a.gap: print("#" + "-" * 40)
        print(f"{addr:08x}  {result[addr]:10}  {names.get(addr, '')}")
        last = addr
    for p in new:
        print(f"# {p}: {len(seen[p])} total, {len(seen[p] - base)} not in baseline")

if __name__ == "__main__":
    main()
