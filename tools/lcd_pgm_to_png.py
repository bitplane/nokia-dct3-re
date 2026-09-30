#!/usr/bin/env python3
"""Convert the harness's binary PGM LCD mirrors to PNG (no PIL), optionally scaled.

Usage: lcd_pgm_to_png.py [--scale N] file.pgm [more.pgm ...]   -> writes file.png next to each
       lcd_pgm_to_png.py --sheet out.png [--scale N] a.pgm b.pgm ...  -> one contact sheet, left to right
"""
import argparse, struct, zlib
from pathlib import Path

def read_pgm(path):
    data = Path(path).read_bytes()
    tokens, pos = [], 0
    while len(tokens) < 4:
        while data[pos:pos+1].isspace(): pos += 1
        if data[pos:pos+1] == b'#':
            while data[pos:pos+1] not in (b'\n', b''): pos += 1
            continue
        start = pos
        while not data[pos:pos+1].isspace(): pos += 1
        tokens.append(data[start:pos])
    assert tokens[0] == b'P5', tokens[0]
    w, h, maxv = int(tokens[1]), int(tokens[2]), int(tokens[3])
    pos += 1
    px = data[pos:pos + w * h]
    return w, h, px

def write_png(path, w, h, rows):
    raw = b''.join(b'\x00' + r for r in rows)
    def chunk(t, d): return struct.pack('>I', len(d)) + t + d + struct.pack('>I', zlib.crc32(t + d) & 0xffffffff)
    png = b'\x89PNG\r\n\x1a\n' + chunk(b'IHDR', struct.pack('>IIBBBBB', w, h, 8, 0, 0, 0, 0)) \
        + chunk(b'IDAT', zlib.compress(raw, 9)) + chunk(b'IEND', b'')
    Path(path).write_bytes(png)

def scale_rows(w, h, px, s):
    rows = []
    for y in range(h):
        r = px[y*w:(y+1)*w]
        rs = bytes(v for v in r for _ in range(s))
        rows.extend([rs] * s)
    return w*s, h*s, rows

def main():
    ap = argparse.ArgumentParser()
    ap.add_argument('--scale', type=int, default=4)
    ap.add_argument('--sheet')
    ap.add_argument('files', nargs='+')
    a = ap.parse_args()
    if a.sheet:
        imgs = [scale_rows(*read_pgm(f), a.scale) for f in a.files]
        h = max(i[1] for i in imgs); gap = 8
        rows = []
        for y in range(h):
            row = b''
            for (w, ih, r) in imgs:
                row += (r[y] if y < ih else b'\x80' * w) + b'\x80' * gap
            rows.append(row)
        write_png(a.sheet, len(rows[0]), h, rows)
        print(a.sheet)
    else:
        for f in a.files:
            w, h, rows = scale_rows(*read_pgm(f), a.scale)
            out = Path(f).with_suffix('.png'); write_png(out, w, h, rows); print(out)

if __name__ == '__main__':
    main()
