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


def check_trace(text: str, runtime_hle: bool = False) -> list[str]:
    errors = []
    cursor = 0
    stages = STAGES
    if runtime_hle:
        stages = STAGES[:6] + (
            ("exclusive HLE handoff", r"staged_dsp: runtime_hle_handoff pc=2c75 native_suspended=1"),
            ("own self-test request", r"dspif_transport: TX pending type=70 payload=2 data=0d00"),
            ("compact peer response", r"dspif_transport: RX enqueue type=74 payload=2 .*data=0d00"),
            ("firmware fault clearance", r"8850_faults: bytes=[0-9a-f]{30}000000[0-9a-f]{12}"),
            STAGES[-1],
        )
    for name, pattern in stages:
        match = re.compile(pattern).search(text, cursor)
        if match is None:
            errors.append(f"missing or out-of-order {name}")
        else:
            cursor = match.end()
    if "Fatal error:" in text or (not runtime_hle and "runtime_hle_handoff" in text):
        errors.append("native-only observation contains a fatal error or HLE handoff")
    return errors


def main() -> int:
    parser = argparse.ArgumentParser(description=__doc__)
    parser.add_argument("log", type=Path)
    parser.add_argument("--runtime-hle", action="store_true",
                        help="check native uploads followed by compact request-correlated HLE")
    args = parser.parse_args()
    try:
        errors = check_trace(args.log.read_text(encoding="utf-8", errors="replace"), args.runtime_hle)
    except OSError as error:
        print(f"FAIL: {error}", file=sys.stderr)
        return 1
    for error in errors:
        print(f"FAIL: {error}", file=sys.stderr)
    if errors:
        return 1
    print("OK - 8850 native uploads" +
          (" and compact runtime self-test" if args.runtime_hle else " reach the missing-mask boundary") +
          "; phone boot not proven")
    return 0


if __name__ == "__main__":
    raise SystemExit(main())
