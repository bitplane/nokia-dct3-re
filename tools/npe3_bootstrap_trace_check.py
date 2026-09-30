"""Check NPE-3's bounded upload frontier, not usable-phone acceptance."""

import argparse
from pathlib import Path
import re
import sys


def check(text, summary):
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
    except (OSError, ValueError) as error:
        print(f"FAIL - NPE-3 bootstrap: {error}", file=sys.stderr)
        return 1
    print("OK - NPE-3 CCONT, 232 handoffs and fail-closed final DSP wait")
    return 0


if __name__ == "__main__":
    raise SystemExit(main())
