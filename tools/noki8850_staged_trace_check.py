#!/usr/bin/env python3
"""Check native 8850 upload execution, not graphical phone acceptance."""

import argparse
from pathlib import Path
import re
import sys


STAGES = (
    ("verifier release", r"staged_dsp: release entry=0f00 words=223 prom_input=0006 .*stage=verifier"),
    ("native verifier publication", r"staged_dsp: publication word0=0000 word1=0006 word2=0006 word3=0006"),
    ("loader release", r"staged_dsp: release entry=0f00 words=126 prom_input=0006 .*stage=loader"),
    ("product-local loader verification", r"staged_dsp: loader2_verified words=613 entry=0a00"),
    ("loader control RMW", r"staged_dsp: control_write port=001c data=0200"),
    ("missing mask boundary", r"staged_dsp: outside_uploaded_code pc=2c75"),
    ("fail-closed suspension", r"staged_dsp: observation_halt pc=2c75 ownership_retained=1"),
    ("firmware readiness", r"8850_ready: state=01 shared_e4=0000"),
)


def check_trace(text: str) -> list[str]:
    errors = []
    cursor = 0
    for name, pattern in STAGES:
        match = re.compile(pattern).search(text, cursor)
        if match is None:
            errors.append(f"missing or out-of-order {name}")
        else:
            cursor = match.end()
    if "Fatal error:" in text or "runtime_hle_handoff" in text:
        errors.append("native-only observation contains a fatal error or HLE handoff")
    return errors


def main() -> int:
    parser = argparse.ArgumentParser(description=__doc__)
    parser.add_argument("log", type=Path)
    args = parser.parse_args()
    try:
        errors = check_trace(args.log.read_text(encoding="utf-8", errors="replace"))
    except OSError as error:
        print(f"FAIL: {error}", file=sys.stderr)
        return 1
    for error in errors:
        print(f"FAIL: {error}", file=sys.stderr)
    if errors:
        return 1
    print("OK - 8850 native uploads reach the missing-mask boundary; phone boot not proven")
    return 0


if __name__ == "__main__":
    raise SystemExit(main())
