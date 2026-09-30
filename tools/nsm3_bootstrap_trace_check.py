"""Validate the bounded NSM-3 bootstrap frontier, not a working-phone claim."""

import argparse
from pathlib import Path
import re
import sys


MCU_WRITE = re.compile(r"dspif_transport: RAM W off=([0-9a-f]+) data=([0-9a-f]+) t=", re.I)


def check(text, summary):
    writes = [(int(a, 16), int(b, 16)) for a, b in MCU_WRITE.findall(text)]
    counts = {offset: writes.count((offset, 0)) for offset in (0xfe, 0x100)}
    if counts != {0xfe: 58, 0x100: 58}:
        raise ValueError(f"expected 58 alternating pairs, got {counts}")
    handoffs = [a for a, b in writes if a in counts and b == 0]
    if handoffs != [0xfe, 0x100] * 58:
        raise ValueError("bootstrap handoff order differs")
    if not re.search(r"peer RAM W off=004 old=ffff data=0006", text):
        raise ValueError("ROM6 identity was not published after the sentinel")
    if summary.get("final_pc") != "002CADCE":
        raise ValueError("firmware did not reach the final verification wait")
    if summary.get("soft_resets") != "0":
        raise ValueError("unexpected reset")
    if "bootstrap completion" in text:
        raise ValueError("unvalidated final result was published")


def main():
    parser = argparse.ArgumentParser(description=__doc__)
    parser.add_argument("log", type=Path)
    parser.add_argument("summary", type=Path)
    args = parser.parse_args()
    try:
        summary = dict(line.split("=", 1) for line in args.summary.read_text().splitlines() if "=" in line)
        check(args.log.read_text(), summary)
    except (OSError, ValueError) as error:
        print(f"FAIL - NSM-3 bootstrap: {error}", file=sys.stderr)
        return 1
    print("OK - NSM-3 ROM6 identity, 58 upload pairs and fail-closed final verification wait")
    return 0


if __name__ == "__main__":
    raise SystemExit(main())
