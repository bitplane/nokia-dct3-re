#!/usr/bin/env python3
"""Contact sheet of the distinct informative LCD frames in a run dir (capture order).

Usage: frame_sheet.py RUN_DIR [OUT.png] [SCALE] [FROM:TO]   (FROM:TO slices the distinct list)
"""
import hashlib, subprocess, sys
from pathlib import Path
run = Path(sys.argv[1]); out = sys.argv[2] if len(sys.argv) > 2 else str(run / "sheet.png")
scale = sys.argv[3] if len(sys.argv) > 3 else "2"
rng = sys.argv[4] if len(sys.argv) > 4 else None
frames = sorted(p for p in run.glob("nokia_dct3_lcdmirror_*.pgm") if "_z504_" not in p.name and "_ff504_" not in p.name)
keep, last = [], None
for p in frames:
    h = hashlib.sha1(p.read_bytes()).hexdigest()
    if h != last: keep.append(p)
    last = h
print(f"{len(frames)} frames, {len(keep)} distinct")
sel = keep
if rng:
    a, b = rng.split(":"); sel = keep[int(a or 0):int(b) if b else None]
if sel:
    subprocess.check_call([sys.executable, str(Path(__file__).parent / "pgm2png.py"), "--scale", scale, "--sheet", out, *map(str, sel)])
    for i, p in enumerate(keep): print(i, p.name)
