#!/usr/bin/env python3
"""Verify missing PCM hardware cannot be replaced by synthetic GSM speech."""

import argparse
import re
from pathlib import Path


ROUTE_RE = re.compile(
    r"dsp_shared_control: command=08 value=060b commit=1 .*?t=([0-9.]+)"
)
BLOCKED_RE = re.compile(
    r"dsp_hle: speech blocked by unsupported PCM link "
    r"control=060b enabled=0 clock=520000/8000 shape=65 .*?t=([0-9.]+)"
)


def check(path: Path) -> str:
    text = path.read_text(errors="replace")
    route = ROUTE_RE.search(text)
    blocked = BLOCKED_RE.search(text)
    if not route:
        raise ValueError("firmware did not request its organic speech field")
    if not blocked:
        raise ValueError("missing PCM link did not block the speech data plane")
    if float(blocked.group(1)) < float(route.group(1)):
        raise ValueError("PCM fault preceded the firmware speech request")
    if "dsp_hle: speech tick" in text:
        raise ValueError("codec/PCM speech clock ran without the physical link")
    if "gsm_voice_peer: exchange=" in text:
        raise ValueError("GSM speech reached the network without the physical link")
    if "dsp_hle: GSM service uplink sapi=0 pd=03 message=2a" not in text:
        raise ValueError("organic call did not reach Release Complete")
    return (
        "OK - firmware requested speech and completed call teardown while the "
        "missing PCM link emitted no codec or network media"
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
