"""Symbol lookup over ghidra/symbols/3210.csv (plus an optional extra CSV, which wins)."""
import csv
from pathlib import Path
ROOT = Path(__file__).resolve().parents[1]
def load(extra=None):
    names = {}
    for p in [ROOT / "ghidra/symbols/3210.csv"] + ([Path(extra)] if extra else []):
        if not p.exists(): continue
        for row in csv.reader(open(p)):
            if len(row) >= 3 and row[0] != "address":
                names[int(row[0], 16) & ~1] = row[2]
    return names
def is_auto(name):
    return name is None or name.startswith(("FUN_", "thunk_FUN_", "callback_", "func_"))
