#!/usr/bin/env python3
"""Verify a save/load round trip occurred inside organic speech."""

import argparse
import re
from pathlib import Path


ROUNDTRIP_RE = re.compile(
    r"state_roundtrip: result=(\w+) .*?requested_at=([0-9.]+) t=([0-9.]+)"
)
SPEECH_RE = re.compile(
    r"dsp_hle: speech tick uplink=(\d+) downlink=(\d+) .*?t=([0-9.]+)"
)
STOP_RE = re.compile(
    r"dsp_hle: speech stop control=[0-9a-f]{4} "
    r"uplink=(\d+) downlink=(\d+) t=([0-9.]+)"
)


def check(path: Path) -> str:
    text = path.read_text(errors="replace")
    roundtrip = ROUNDTRIP_RE.search(text)
    if not roundtrip or roundtrip.group(1) != "pass":
        raise ValueError("missing successful emulator save/load round trip")
    requested = float(roundtrip.group(2))
    restored = float(roundtrip.group(3))
    if restored < requested - 0.000_001 or restored > requested + 0.012:
        raise ValueError(
            f"restore did not resume the saved timeline: {requested} -> {restored}"
        )

    speech = [
        (int(match.group(1)), int(match.group(2)), float(match.group(3)))
        for match in SPEECH_RE.finditer(text)
    ]
    before = [sample for sample in speech if sample[2] < requested]
    after = [sample for sample in speech if sample[2] > restored]
    if not before or not after:
        raise ValueError("save/load was not bracketed by active speech")
    if after[-1][0] < 100 or after[-1][1] < 100:
        raise ValueError(
            f"substantial bidirectional speech did not continue after restore: "
            f"{after[-1]}"
        )

    stops = [
        (int(match.group(1)), int(match.group(2)), float(match.group(3)))
        for match in STOP_RE.finditer(text)
    ]
    if not stops or stops[-1][2] <= restored:
        raise ValueError("organic call did not tear down after restore")
    return (
        "OK - active-call save/load resumed bidirectional speech and reached "
        f"clean teardown at {stops[-1][0]}/{stops[-1][1]} codec frames"
    )


def main() -> None:
    parser = argparse.ArgumentParser()
    parser.add_argument("log", type=Path)
    args = parser.parse_args()
    try:
        print(check(args.log))
    except ValueError as error:
        raise SystemExit(f"FAIL - {error}") from error


if __name__ == "__main__":
    main()
