#!/usr/bin/env python3
"""Verify native completion-driven NSE-6 TX publication, not DSP search execution."""

import argparse
from pathlib import Path
import re
import sys


def check(text):
    if "unimplemented C54x opcode" in text:
        raise ValueError("unsupported native opcode")
    counts = re.findall(r"nse6_stage_count: pc=002b5e78 count=(\d+)\b", text)
    if len(counts) != 1 or int(counts[0]) == 0:
        raise ValueError("missing observed FIQ1 TX wakeup")
    completions = re.findall(
        r"nse6_native_completion_bits: value=0002 pc=395e t=([0-9.]+)", text)
    if not completions:
        raise ValueError("missing native TX completion")
    queued = re.findall(
        r"nse6_tx_mailbox_object: object=([0-9a-f]+) prefix=0002 type=1a t=([0-9.]+)", text)
    spaces = re.findall(
        r"nse6_tx_space_check: object=([0-9a-f]+) type=1a length=44 available=(\d+) t=([0-9.]+)", text)
    writes = re.findall(
        r"nse6_tx_ring_request: source=([0-9a-f]+) type=1a length=68 lr=002d3701 t=([0-9.]+)", text)
    for obj, queued_at in queued:
        for checked_obj, available, checked_at in spaces:
            if checked_obj != obj or int(available) < 70:
                continue
            for source, written_at in writes:
                if int(source, 16) != int(obj, 16) + 3:
                    continue
                if min(map(float, completions)) < float(queued_at) <= float(checked_at) <= float(written_at):
                    return {"wakeups": int(counts[0]), "published": float(written_at)}
    raise ValueError("missing ordered same-object mailbox/space/ring publication")


def main():
    parser = argparse.ArgumentParser(description=__doc__)
    parser.add_argument("log", type=Path)
    args = parser.parse_args()
    try:
        result = check(args.log.read_text(errors="replace"))
    except (OSError, ValueError) as error:
        print(f"NSE-6 native TX wakeup: FAIL {error}", file=sys.stderr)
        return 1
    print(f"NSE-6 native TX wakeup: PASS wakeups={result['wakeups']} published={result['published']:.9f} search_execution_claim=0")
    return 0


if __name__ == "__main__":
    sys.exit(main())
