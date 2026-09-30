"""Check NPE-3's bounded upload frontier, not usable-phone acceptance."""

import argparse
from pathlib import Path
import re
import sys

try:
    from tools.check_lcd_frame import read_pgm
except ModuleNotFoundError:
    from check_lcd_frame import read_pgm


def check_display(text):
    events = [(int(offset, 16), int(data, 16)) for offset, data in re.findall(
        r"display_io: off=(2e|6e) data=([0-9a-f]{2}) .*pc=004e(?:6564|657a|6582|658c|65a6)\b", text, re.I)]
    expected = [(0x6e, 0x24)]
    for bank in range(8):
        expected.extend([(0x6e, 0x40 | bank), (0x6e, 0x80)])
        expected.extend([(0x2e, 0)] * 96)
    expected.append((0x6e, 0x20))
    if events != expected:
        raise ValueError("LCD clear did not cover eight ordered 96-byte banks")


def check_frame(path):
    width, height, pixels = read_pgm(path)
    if (width, height) != (96, 60):
        raise ValueError("capture does not match the fitted 96x60 LCD")
    if any(pixel != 255 for pixel in pixels):
        raise ValueError("uncompleted bootstrap no longer has the blank cleared frame; reassess frontier")


def check(text, summary):
    check_display(text)
    writes = [(int(a, 16), int(b, 16)) for a, b in re.findall(
        r"dspif_transport: RAM W off=([0-9a-f]+) data=([0-9a-f]+) t=", text, re.I)]
    handoffs = [a for a, b in writes if a in (0xfe, 0x100) and b == 0]
    if handoffs != [0xfe, 0x100] * 116:
        raise ValueError("expected 232 alternating buffer handoffs")
    if not re.search(r"gensio: W off=2d data=22 .*pc=004ec7b0", text, re.I):
        raise ValueError("missing product-local CCONT selection")
    if not re.search(r"gensio: R off=6d data=07 pc=004ec7bc", text, re.I):
        raise ValueError("CCONT receive-ready was not observed")
    if summary.get("final_pc", "").upper() not in {
            "00426CC2", "00426CC4", "00426CC6", "00426CC8"}:
        raise ValueError("not at the final DSP verification wait")
    if summary.get("soft_resets") != "0":
        raise ValueError("unexpected reset")
    if "bootstrap completion" in text:
        raise ValueError("unvalidated DSP completion published")


def main():
    parser = argparse.ArgumentParser(description=__doc__)
    parser.add_argument("log", type=Path)
    parser.add_argument("summary", type=Path)
    args = parser.parse_args()
    try:
        summary = dict(line.split("=", 1) for line in args.summary.read_text().splitlines() if "=" in line)
        check(args.log.read_text(), summary)
        frames = sorted(args.log.parent.glob("nokia_dct3_lcdmirror_*.pgm"))
        if not frames:
            raise ValueError("missing LCD capture")
        check_frame(frames[-1])
    except (OSError, ValueError) as error:
        print(f"FAIL - NPE-3 bootstrap: {error}", file=sys.stderr)
        return 1
    print("OK - NPE-3 CCONT, 232 handoffs and fail-closed final DSP wait")
    return 0


if __name__ == "__main__":
    raise SystemExit(main())
