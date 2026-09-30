#!/usr/bin/env python3
"""Everything needed to name one function: names, callers/callees, resolved
literal-pool references, decompiled C (cached), Thumb disassembly, and notes.

Usage: packet.py ADDR [--no-asm] [--no-c]
"""
import argparse, bisect, json, os, re, subprocess, sys
from pathlib import Path
sys.path.insert(0, str(Path(__file__).resolve().parent))
import names as N
ROOT = Path(__file__).resolve().parents[2]
FLASH = 0x200000

def entries():
    return sorted(int(l.split()[0], 16) for l in open(ROOT / "games/data/entry_candidates.txt") if l.strip())

def describe(value, names):
    if value in names: return names[value]
    if 0x100000 <= value < 0x180000: return "RAM"
    if FLASH <= value < FLASH + 0x200000: return "ROM" + (" (thumb ptr)" if value & 1 else "")
    return ""

def main():
    ap = argparse.ArgumentParser(); ap.add_argument("addr"); ap.add_argument("--no-asm", action="store_true"); ap.add_argument("--no-c", action="store_true")
    a = ap.parse_args(); addr = int(a.addr, 16) & ~1; key = f"{addr:08x}"
    names = N.load(); g = json.load(open(ROOT / "games/data/callgraph.json")); fns = g["functions"]
    ents = entries(); i = bisect.bisect_right(ents, addr); end = ents[i] if i < len(ents) else addr + 0x200
    rec = fns.get(key, {"callers": [], "callees": [], "boundary": None})
    def nm(h): return names.get(int(h, 16)) or h
    print(f"=== {key}  {names.get(addr, '<unnamed>')}  [{'boundary' if rec['boundary'] else 'inner'}]  extent ~{end-addr:#x} bytes")
    print("callers:", ", ".join(nm(c) for c in rec["callers"]) or "-")
    print("callees:", ", ".join(nm(c) for c in dict.fromkeys(rec["callees"])) or "-")
    notes = json.load(open(ROOT / "games/notes.json")) if (ROOT / "games/notes.json").exists() else {"blocks": []}
    for b in notes["blocks"]:
        if int(b["addr"], 16) == addr: print("notes:"); [print("   ", l) for l in b["lines"]]
    env = {**os.environ, "NOKI_BIN": str(ROOT / "roms/3210f600a_swap16.bin")}
    asm = subprocess.run([str(ROOT / ".venv/bin/python"), str(ROOT / "tools/disrom.py"), f"{addr:#x}:{end-addr:#x}"], capture_output=True, text=True, env=env).stdout
    lits = {}
    for m in re.finditer(r"\[([0-9a-f]{8})\] = ([0-9a-f]{8})", asm):
        v = int(m.group(2), 16); lits[m.group(1)] = (v, describe(v & ~1 if v & 1 else v, names))
    if lits:
        print("literal pool:")
        for k, (v, d) in sorted(lits.items()): print(f"   [{k}] = {v:08x}  {d}")
    c = ROOT / "games/data/decomp" / f"{key}.c"
    if not a.no_c:
        print("--- decompiled" if c.exists() else "--- (no cached decompile; run games/tools/decomp_refresh.sh)")
        if c.exists(): print(c.read_text())
    if not a.no_asm:
        print("--- disassembly"); print(asm)

if __name__ == "__main__":
    main()
