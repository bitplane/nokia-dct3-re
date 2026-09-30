"""Validate the 7110 v5.01 upload frontier, not a completed handset boot."""

import argparse
from pathlib import Path
import re
import sys


def check(text, summary):
    writes = [(int(a, 16), int(b, 16)) for a, b in re.findall(
        r"dspif_transport: RAM W off=([0-9a-f]+) data=([0-9a-f]+) t=", text, re.I)]
    handoffs = [a for a, b in writes if a in (0xfe, 0x100) and b == 0]
    if handoffs != [0xfe, 0x100] * 114:
        raise ValueError("expected 228 alternating sparse-flash handoffs")
    if summary.get("final_pc", "").upper() not in {
            "00432F96", "00432F98", "00432F9A", "00432F9C"}:
        raise ValueError("not at the DSP-owned final verification wait")
    if summary.get("soft_resets") != "0":
        raise ValueError("unexpected baseband reset")
    if "bootstrap publication" in text or "bootstrap completion" in text:
        raise ValueError("unvalidated final DSP publication")


def main():
    parser = argparse.ArgumentParser(description=__doc__)
    parser.add_argument("log", type=Path)
    parser.add_argument("summary", type=Path)
    args = parser.parse_args()
    try:
        summary = dict(line.split("=", 1) for line in args.summary.read_text().splitlines()
                       if "=" in line)
        check(args.log.read_text(), summary)
    except (OSError, ValueError) as error:
        print(f"FAIL - NSE-5 bootstrap: {error}", file=sys.stderr)
        return 1
    print("OK - NSE-5 228 handoffs and fail-closed final DSP wait")
    return 0


if __name__ == "__main__":
    raise SystemExit(main())
