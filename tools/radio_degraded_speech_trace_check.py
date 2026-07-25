#!/usr/bin/env python3
"""Validate deterministic air-bit errors become bounded bad speech frames."""

import argparse
import re
from pathlib import Path

try:
    from tools.radio_speech_media_trace_check import check as check_clean_media
except ModuleNotFoundError:
    from radio_speech_media_trace_check import check as check_clean_media


IMPAIRMENT_RE = re.compile(
    r"radio_l1: direction=downlink impairment=invert-data "
    r"burst=(\d+) count=(\d+) fn=(\d+) t=([0-9.]+)"
)
BAD_RE = re.compile(
    r"radio_l1: direction=downlink kind=speech good=0 "
    r"count=(\d+) fn=(\d+) t=([0-9.]+)"
)
GOOD_MEDIA_RE = re.compile(
    r"dsp_hle: speech tick uplink=(\d+) downlink=(\d+) .*?t=([0-9.]+)"
)
CONCEALMENT_RE = re.compile(
    r"dsp_hle: speech tick .*?ear_peak=(\d+) .*?"
    r"concealed=(\d+) muted=(\d+) t=([0-9.]+)"
)


def check(path: Path) -> str:
    clean_result = check_clean_media(path)
    text = path.read_text(errors="replace")
    impairments = [
        (int(burst), int(count), int(frame), float(time))
        for burst, count, frame, time in IMPAIRMENT_RE.findall(text)
    ]
    bad = [
        (int(count), int(frame), float(time))
        for count, frame, time in BAD_RE.findall(text)
    ]
    if not impairments:
        raise ValueError("configured downlink burst impairment never occurred")
    if not bad:
        raise ValueError("impaired bursts never produced a bad speech frame")
    impaired_bad = [entry for entry in bad if entry[2] > impairments[0][3]]
    if not impaired_bad:
        raise ValueError("no bad speech frame followed the configured impairment")
    if any(right[1] != left[1] + 1
           for left, right in zip(impairments, impairments[1:])):
        raise ValueError("impairment counter is not contiguous")
    if any(right[0] != left[0] + 1 for left, right in zip(bad, bad[1:])):
        raise ValueError("bad-frame counter is not contiguous")
    media = [
        (int(uplink), int(downlink), float(time))
        for uplink, downlink, time in GOOD_MEDIA_RE.findall(text)
    ]
    if not media or not any(time > impaired_bad[-1][2] and downlink > 100
                            for _, downlink, time in media):
        raise ValueError("good downlink media did not recover after a bad frame")
    concealment = [
        (int(peak), int(concealed), int(muted), float(time))
        for peak, concealed, muted, time in CONCEALMENT_RE.findall(text)
    ]
    if not concealment or concealment[-1][1] == 0:
        raise ValueError("bad/FACCH-displaced speech never reached BFI concealment")
    if not any(peak and concealed and time > impaired_bad[0][2]
               for peak, concealed, _, time in concealment):
        raise ValueError("receiver did not substitute non-silent speech after BFI")
    return (
        f"{clean_result}; degraded link inverted {impairments[-1][1]} bursts "
        f"and observed {len(impaired_bad)} impairment-induced bad blocks "
        f"with {concealment[-1][1]} concealed intervals before recovery"
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
