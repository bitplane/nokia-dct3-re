#!/usr/bin/env python3
"""Verify reserved SACCH/TF slots coexist with continuing TCH/F speech."""

import argparse
import re
from pathlib import Path

try:
    from tools.radio_speech_media_trace_check import canonical_timeline
except ModuleNotFoundError:
    from radio_speech_media_trace_check import canonical_timeline


SACCH_RE = re.compile(
    r"radio_l1: kind=sacch slot=(\d+) phase=(\d) "
    r"uplink_pending=([01]) downlink_pending=([01]) "
    r"fn=(\d+) t=([0-9.]+)"
)
SPEECH_RE = re.compile(
    r"dsp_hle: speech tick uplink=(\d+) downlink=(\d+) .*?t=([0-9.]+)"
)


def check(path: Path) -> str:
    text = canonical_timeline(path.read_text(errors="replace"))
    all_sacch = [
        tuple(map(int, match.groups()[:-1])) + (float(match.group(6)),)
        for match in SACCH_RE.finditer(text)
    ]
    speech = [
        (int(match.group(1)), int(match.group(2)), float(match.group(3)))
        for match in SPEECH_RE.finditer(text)
    ]
    if len(speech) < 2:
        raise ValueError("fewer than two live speech observations")
    sacch = [
        slot for slot in all_sacch
        if speech[0][2] < slot[5] < speech[-1][2]
    ]
    if len(sacch) < 8:
        raise ValueError("fewer than eight live SACCH/TF reservations")
    if any(right[0] != left[0] + 1 for left, right in zip(sacch, sacch[1:])):
        raise ValueError("SACCH slot counter is not contiguous")
    if any(right[4] - left[4] != 26 for left, right in zip(sacch, sacch[1:])):
        raise ValueError("SACCH reservations are not one per 26-frame multiframe")
    if any(
        right[1] != ((left[1] + 1) & 3)
        for left, right in zip(sacch, sacch[1:])
    ):
        raise ValueError("SACCH four-multiframe phase did not rotate")

    first_time = sacch[0][5]
    last_time = sacch[-1][5]
    before = [sample for sample in speech if sample[2] < first_time]
    after = [sample for sample in speech if sample[2] > last_time]
    if not before or not after:
        raise ValueError("SACCH reservations were not bracketed by live speech")
    if after[-1][0] <= before[-1][0] or after[-1][1] <= before[-1][1]:
        raise ValueError("bidirectional speech did not continue across SACCH slots")
    return (
        f"OK - {len(sacch)} SACCH/TF slots rotated through four phases while "
        f"speech advanced from {before[-1][0]}/{before[-1][1]} to "
        f"{after[-1][0]}/{after[-1][1]} frames"
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
