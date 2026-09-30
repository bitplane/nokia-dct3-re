#!/usr/bin/env python3
"""Enumerate candidate Thumb function entry points in the swap16 firmware image.

Sources (union, deduplicated):
  bl     - targets of every plausible Thumb BL pair (hi 0xF000-0xF7FF, lo 0xF800-0xFFFF)
  ptr    - 32-bit little-endian words (after swap16 correction) that look like Thumb
           code pointers into flash (odd, inside the image)
  sym    - function entries from ghidra/symbols/3210.csv
  push   - halfwords decoding to `push {..., lr}` (0xB5xx) at even offsets

Output: one address per line (hex, even), sorted, with a source tag column.
"""
import argparse, csv, sys
from pathlib import Path

ROOT = Path(__file__).resolve().parents[1]
FLASH = 0x200000

def main():
    ap = argparse.ArgumentParser()
    ap.add_argument("--image", default=str(ROOT / "roms/3210f600a_swap16.bin"))
    ap.add_argument("--symbols", default=str(ROOT / "ghidra/symbols/3210.csv"))
    ap.add_argument("--out", default=str(ROOT / "run_games/entry_candidates.txt"))
    ap.add_argument("--no-push", action="store_true", help="omit the noisy push-prologue source")
    args = ap.parse_args()

    data = Path(args.image).read_bytes()
    n = len(data)
    hw = [int.from_bytes(data[i:i+2], "little") for i in range(0, n - 1, 2)]
    tags = {}
    def add(addr, tag):
        addr &= ~1
        if FLASH <= addr < FLASH + n:
            tags.setdefault(addr, set()).add(tag)

    # BL pairs
    for i in range(len(hw) - 1):
        h, l = hw[i], hw[i+1]
        if 0xF000 <= h <= 0xF7FF and 0xF800 <= l <= 0xFFFF:
            off = ((h & 0x7FF) << 12) | ((l & 0x7FF) << 1)
            if off & 0x400000:
                off -= 0x800000
            pc = FLASH + i * 2 + 4
            add(pc + off, "bl")

    # Thumb code pointers: image words are halfword-swapped relative to MCU view
    for i in range(0, n - 3, 2):
        raw = int.from_bytes(data[i:i+4], "little")
        word = ((raw & 0xFFFF) << 16) | (raw >> 16)
        if word & 1 and FLASH <= word < FLASH + n:
            add(word, "ptr")

    # symbol map
    with open(args.symbols) as f:
        for row in csv.DictReader(f):
            if row.get("kind") == "function":
                add(int(row["address"], 16), "sym")

    if not args.no_push:
        for i, h in enumerate(hw):
            if (h & 0xFF00) == 0xB500:
                add(FLASH + i * 2, "push")

    out = Path(args.out)
    out.parent.mkdir(parents=True, exist_ok=True)
    with open(out, "w") as f:
        for a in sorted(tags):
            f.write(f"{a:08x} {'+'.join(sorted(tags[a]))}\n")
    from collections import Counter
    c = Counter(t for s in tags.values() for t in s)
    print(f"{len(tags)} candidates -> {out}")
    for k, v in sorted(c.items()):
        print(f"  {k:5} {v}")

if __name__ == "__main__":
    main()
