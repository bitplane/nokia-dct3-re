#!/usr/bin/env python3
"""Naming progress and worklist for the games closure (from games/data/callgraph.json).

Ranks unnamed inner functions: frontier first (called by a named function),
then by fan-in. Boundary callees are listed separately: they are the phone-OS
services the wrapper must shim, so naming them is the wrapper's API list.

Usage: worklist.py [--top N] [--boundary]
"""
import argparse, json, sys
from pathlib import Path
sys.path.insert(0, str(Path(__file__).resolve().parent))
import names as N
ROOT = Path(__file__).resolve().parents[2]

def main():
    ap = argparse.ArgumentParser(); ap.add_argument("--top", type=int, default=8); ap.add_argument("--boundary", action="store_true")
    a = ap.parse_args()
    g = json.load(open(ROOT / "games/data/callgraph.json")); fns = g["functions"]; names = N.load()
    def nm(h): return names.get(int(h, 16))
    inner = [f for f, r in fns.items() if not r["boundary"]]; bnd = g["boundary"]
    named_i = [f for f in inner if not N.is_auto(nm(f))]; named_b = [f for f in bnd if not N.is_auto(nm(f))]
    print(f"inner: {len(named_i)}/{len(inner)} named   boundary: {len(named_b)}/{len(bnd)} named")
    pool = bnd if a.boundary else inner
    rows = []
    for f in pool:
        if not N.is_auto(nm(f)): continue
        r = fns[f]; named_callers = [c for c in r["callers"] if not N.is_auto(nm(c))]
        rows.append((len(named_callers) > 0, len(r["callers"]), f, named_callers, r["callers"]))
    rows.sort(key=lambda x: (not x[0], -x[1], x[2]))
    for frontier, fanin, f, nc, allc in rows[: a.top]:
        tag = "F" if frontier else " "
        who = ", ".join(nm(c) or c for c in (nc or allc)[:4])
        print(f"  {tag} {f}  fan-in={fanin:2}  callers: {who}")
    if rows: print(f"NEXT {rows[0][2]}")

if __name__ == "__main__":
    main()
