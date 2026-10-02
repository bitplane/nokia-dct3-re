#!/usr/bin/env python3
"""Split an ExportFunctionsByAddress.java output file into one <address>.c per function.

Usage: split_decompile_export.py EXPORT.c OUT_DIR
The export file is removed afterwards; the per-function files are derived
firmware text and belong under an ignored run directory, never in the tree.
"""
import re, sys
from pathlib import Path

def main():
    src, out = Path(sys.argv[1]), Path(sys.argv[2])
    out.mkdir(parents=True, exist_ok=True)
    n = 0
    for m in re.finditer(r"/\* ==== (\w+) (\S+) ==== \*/\n(.*?)(?=/\* ==== |\Z)", src.read_text(), re.S):
        (out / f"{m.group(1)}.c").write_text(f"// {m.group(2)}\n{m.group(3).strip()}\n")
        n += 1
    src.unlink()
    print(f"decompiled {n} functions -> {out}")

if __name__ == "__main__":
    main()
