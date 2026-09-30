#!/usr/bin/env bash
# Push games/symbols.csv into the Ghidra project, then (re)export decompiled C
# for every function in the games closure into games/data/decomp/<addr>.c.
set -euo pipefail
ROOT="$(cd "$(dirname "$0")/../.." && pwd)"; cd "$ROOT"
PROJ="${GHIDRA_PROJECTS:-$HOME/ghidra/projects}"; NAME=nokia3210; PROG=3210f600a_swap16.bin
ADDRS=$(python3 -c "
import json; g=json.load(open('games/data/callgraph.json')); print(' '.join('0x'+a for a in g['functions']))")
OUT=games/data/decomp; mkdir -p "$OUT"; TMP="$OUT/_all.c"
analyzeHeadless "$PROJ" "$NAME" -process "$PROG" -noanalysis -scriptPath "ghidra/scripts;games/ghidra" \
  -postScript ImportSymbolsCsv.java "$ROOT/games/symbols.csv" \
  -postScript ExportFunctionsByAddress.java "$ROOT/$TMP" $ADDRS 2>&1 | grep -E "ImportSymbolsCsv|ERROR" || true
python3 - "$TMP" "$OUT" <<'PY'
import re, sys
from pathlib import Path
src, out = Path(sys.argv[1]), Path(sys.argv[2])
text = src.read_text(); n = 0
for m in re.finditer(r"/\* ==== (\w+) (\S+) ==== \*/\n(.*?)(?=/\* ==== |\Z)", text, re.S):
    (out / f"{m.group(1)}.c").write_text(f"// {m.group(2)}\n{m.group(3).strip()}\n"); n += 1
src.unlink(); print(f"decompiled {n} functions -> {out}")
PY
