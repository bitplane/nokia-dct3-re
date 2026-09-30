#!/usr/bin/env python3
"""Static call graph of the 3210 games code (Thumb-1, swap16 image).

Scans every BL pair in the image, then walks the closure from the given roots.
Functions inside the "inner" address ranges are descended into; anything
outside is recorded as a boundary callee (a phone-OS service the games rely
on) and not descended into. Function extents are approximated as
[entry, next entry) using the candidate entry list.

Usage:
  callgraph.py [--roots 0x... ...] [--inner 0x240600-0x244000,...] [--json out]
Defaults: roots = every candidate entry inside the inner ranges.
Output: JSON {functions: {addr: {callees:[], callers:[], boundary:bool, region:str}}, boundary:[...]}
"""
import argparse, bisect, json
from pathlib import Path

ROOT = Path(__file__).resolve().parents[2]
FLASH = 0x200000
DEFAULT_INNER = "0x240600-0x244000,0x2621c0-0x263500"

def load_entries(path):
    return sorted(int(l.split()[0], 16) for l in open(path) if l.strip())

def scan_bl(data):
    """Return dict caller_site -> target for every plausible BL pair."""
    hw = [int.from_bytes(data[i:i+2], "little") for i in range(0, len(data) - 1, 2)]
    calls = {}
    for i in range(len(hw) - 1):
        h, l = hw[i], hw[i+1]
        if 0xF000 <= h <= 0xF7FF and 0xF800 <= l <= 0xFFFF:
            off = ((h & 0x7FF) << 12) | ((l & 0x7FF) << 1)
            if off & 0x400000: off -= 0x800000
            pc = FLASH + i * 2 + 4
            t = pc + off
            if FLASH <= t < FLASH + len(data): calls[FLASH + i * 2] = t
    return calls

def parse_ranges(s):
    out = []
    for part in s.split(","):
        a, b = part.split("-"); out.append((int(a, 0), int(b, 0)))
    return out

def in_ranges(a, ranges):
    return any(lo <= a < hi for lo, hi in ranges)

def build(image, entries_path, roots, inner):
    data = Path(image).read_bytes()
    entries = load_entries(entries_path)
    calls = scan_bl(data)
    def owner(site):
        i = bisect.bisect_right(entries, site) - 1
        return entries[i] if i >= 0 else None
    by_fn = {}
    for site, t in calls.items():
        o = owner(site)
        if o is not None: by_fn.setdefault(o, []).append((site, t))
    fns = {}
    work = list(roots)
    while work:
        f = work.pop()
        if f in fns: continue
        inside = in_ranges(f, inner)
        fns[f] = {"callees": [], "callers": [], "boundary": not inside}
        if not inside: continue
        for site, t in sorted(by_fn.get(f, [])):
            fns[f]["callees"].append(t)
            work.append(t)
    for f, rec in fns.items():
        for t in rec["callees"]:
            if t in fns and f not in fns[t]["callers"]: fns[t]["callers"].append(f)
    return fns

def main():
    ap = argparse.ArgumentParser()
    ap.add_argument("--image", default=str(ROOT / "roms/3210f600a_swap16.bin"))
    ap.add_argument("--entries", default=str(ROOT / "games/data/entry_candidates.txt"))
    ap.add_argument("--roots", nargs="*")
    ap.add_argument("--inner", default=DEFAULT_INNER)
    ap.add_argument("--json", default=str(ROOT / "games/data/callgraph.json"))
    a = ap.parse_args()
    inner = parse_ranges(a.inner)
    entries = load_entries(a.entries)
    roots = [int(r, 0) for r in a.roots] if a.roots else [e for e in entries if in_ranges(e, inner)]
    fns = build(a.image, a.entries, roots, inner)
    inner_fns = [f for f, r in fns.items() if not r["boundary"]]
    boundary = sorted(f for f, r in fns.items() if r["boundary"])
    out = {"inner_ranges": a.inner, "roots": [f"{r:08x}" for r in roots],
           "functions": {f"{f:08x}": {"callees": [f"{t:08x}" for t in r["callees"]],
                                      "callers": [f"{t:08x}" for t in r["callers"]],
                                      "boundary": r["boundary"]} for f, r in sorted(fns.items())},
           "boundary": [f"{f:08x}" for f in boundary]}
    Path(a.json).write_text(json.dumps(out, indent=1))
    print(f"inner functions: {len(inner_fns)}  boundary callees: {len(boundary)}  -> {a.json}")

if __name__ == "__main__":
    main()
