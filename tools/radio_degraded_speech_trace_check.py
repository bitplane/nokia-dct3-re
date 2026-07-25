#!/usr/bin/env python3
"""Validate bidirectional air errors, BFIs, concealment and clean recovery."""

import argparse
import re
from pathlib import Path

try:
    from tools.radio_speech_media_trace_check import check as check_clean_media
except ModuleNotFoundError:
    from radio_speech_media_trace_check import check as check_clean_media


IMPAIRMENT_RE = re.compile(
    r"radio_l1: direction=(uplink|downlink) impairment=invert-data "
    r"burst=(\d+) count=(\d+) fn=(\d+) t=([0-9.]+)"
)
BAD_RE = re.compile(
    r"radio_l1: direction=(uplink|downlink) kind=speech good=0 "
    r"count=(\d+) fn=(\d+) t=([0-9.]+)"
)
GOOD_MEDIA_RE = re.compile(
    r"dsp_hle: speech tick uplink=(\d+) downlink=(\d+) .*?t=([0-9.]+)"
)
CONCEALMENT_RE = re.compile(
    r"dsp_hle: speech tick .*?ear_peak=(\d+) .*?"
    r"concealed=(\d+) muted=(\d+) t=([0-9.]+)"
)
PEER_CONCEALMENT_RE = re.compile(
    r"gsm_voice_peer: exchange=(\d+) uplink_peak=(\d+) "
    r"downlink_peak=(\d+) source=lab-1khz uplink_good=(\d) "
    r"concealed=(\d+) muted=(\d+) t=([0-9.]+)"
)


def check(path: Path) -> str:
    clean_result = check_clean_media(path)
    text = path.read_text(errors="replace")
    impairments = [
        (direction, int(burst), int(count), int(frame), float(time))
        for direction, burst, count, frame, time
        in IMPAIRMENT_RE.findall(text)
    ]
    bad = [
        (direction, int(count), int(frame), float(time))
        for direction, count, frame, time in BAD_RE.findall(text)
    ]
    for direction in ("uplink", "downlink"):
        direction_impairments = [
            entry for entry in impairments if entry[0] == direction
        ]
        direction_bad = [entry for entry in bad if entry[0] == direction]
        if not direction_impairments:
            raise ValueError(
                f"configured {direction} burst impairment never occurred"
            )
        if not direction_bad:
            raise ValueError(
                f"{direction} impairments never produced a bad speech frame"
            )
        if any(right[2] != left[2] + 1 for left, right in
               zip(direction_impairments, direction_impairments[1:])):
            raise ValueError(f"{direction} impairment counter is not contiguous")
        if any(right[1] != left[1] + 1 for left, right in
               zip(direction_bad, direction_bad[1:])):
            raise ValueError(f"{direction} bad-frame counter is not contiguous")
    downlink_impairments = [
        entry for entry in impairments if entry[0] == "downlink"
    ]
    impaired_bad = [
        entry for entry in bad
        if entry[0] == "downlink" and entry[3] > downlink_impairments[0][4]
    ]
    if not impaired_bad:
        raise ValueError(
            "no downlink bad speech frame followed the configured impairment"
        )
    media = [
        (int(uplink), int(downlink), float(time))
        for uplink, downlink, time in GOOD_MEDIA_RE.findall(text)
    ]
    if not media or not any(time > impaired_bad[-1][3] and downlink > 100
                            for _, downlink, time in media):
        raise ValueError("good downlink media did not recover after a bad frame")
    concealment = [
        (int(peak), int(concealed), int(muted), float(time))
        for peak, concealed, muted, time in CONCEALMENT_RE.findall(text)
    ]
    if not concealment or concealment[-1][1] == 0:
        raise ValueError("bad/FACCH-displaced speech never reached BFI concealment")
    if not any(peak and concealed and time > impaired_bad[0][3]
               for peak, concealed, _, time in concealment):
        raise ValueError("receiver did not substitute non-silent speech after BFI")
    peer_concealment = [
        tuple(map(int, match.groups()[:-1])) + (float(match.group(7)),)
        for match in PEER_CONCEALMENT_RE.finditer(text)
    ]
    if not peer_concealment or peer_concealment[-1][4] == 0:
        raise ValueError("uplink BFI never reached network-side concealment")
    last_uplink_bad_time = max(
        entry[3] for entry in bad if entry[0] == "uplink"
    )
    if not any(exchange > 100 and downlink_peak and time > last_uplink_bad_time
               for exchange, _, downlink_peak, _, _, _, time
               in peer_concealment):
        raise ValueError(
            "independent downlink did not continue after an uplink bad frame"
        )
    return (
        f"{clean_result}; bidirectional degraded link reached "
        f"{impairments[-1][2]} impaired bursts/direction "
        f"and observed {len(impaired_bad)} impairment-induced bad blocks "
        f"with handset/network concealment={concealment[-1][1]}/"
        f"{peer_concealment[-1][4]} before recovery"
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
