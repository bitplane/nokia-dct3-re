#!/usr/bin/env python3
"""Verify organic FACCH steals bidirectional speech and both sides recover."""

import argparse
import re
from pathlib import Path

try:
    from tools.radio_speech_media_trace_check import (
        canonical_timeline,
        DOORBELL_ROUTE_RE,
    )
except ModuleNotFoundError:
    from radio_speech_media_trace_check import (
        canonical_timeline,
        DOORBELL_ROUTE_RE,
    )


ROUTE_RE = re.compile(
    r"dsp_shared_control: command=08 value=([0-9a-f]{4}) "
    r"commit=1 .*?t=([0-9.]+)"
)
STOP_RE = re.compile(r"dsp_hle: speech stop .*?t=([0-9.]+)")
FACCH_RE = re.compile(
    r"radio_l1: direction=(uplink|downlink) kind=facch good=(\d) "
    r"count=(\d+) fn=(\d+) t=([0-9.]+)"
)
HANDSET_RE = re.compile(
    r"dsp_hle: speech tick uplink=\d+ downlink=(\d+) .*?"
    r"ear_peak=(\d+) .*?concealed=(\d+) .*?t=([0-9.]+)"
)
PEER_RE = re.compile(
    r"gsm_voice_peer: exchange=(\d+) uplink_peak=(\d+) .*?"
    r"uplink_good=(\d) concealed=(\d+) .*?t=([0-9.]+)"
)


def check(path: Path) -> str:
    text = canonical_timeline(path.read_text(errors="replace"))
    routes = [
        float(time)
        for value, time in ROUTE_RE.findall(text)
        if int(value, 16) & 0x0fff == 0x060b
    ]
    routes.extend(
        float(time)
        for value, time in DOORBELL_ROUTE_RE.findall(text)
        if int(value, 16) & 0x0fff == 0x060b
    )
    routes.sort()
    if not routes:
        raise ValueError("missing firmware speech-route request")
    route_time = routes[0]
    stops = [
        float(time) for time in STOP_RE.findall(text)
        if float(time) > route_time
    ]
    if not stops:
        raise ValueError("missing organic speech teardown")
    stop_time = stops[-1]

    facch = [
        (direction, int(good), int(count), int(frame), float(time))
        for direction, good, count, frame, time in FACCH_RE.findall(text)
        if route_time < float(time) < stop_time
    ]
    for direction in ("uplink", "downlink"):
        blocks = [entry for entry in facch if entry[0] == direction]
        if not blocks or not all(entry[1] for entry in blocks):
            raise ValueError(
                f"no good {direction} FACCH decoded while speech was active"
            )

    handset = [
        (int(downlink), int(peak), int(concealed), float(time))
        for downlink, peak, concealed, time in HANDSET_RE.findall(text)
    ]
    downlink_facch = [entry for entry in facch if entry[0] == "downlink"]
    first_downlink_facch = downlink_facch[0][4]
    last_downlink_facch = downlink_facch[-1][4]
    handset_before = [
        entry for entry in handset if entry[3] < first_downlink_facch
    ]
    handset_after = [
        entry for entry in handset if entry[3] > first_downlink_facch
    ]
    baseline = handset_before[-1][2] if handset_before else 0
    if not handset_after or handset_after[0][2] <= baseline:
        raise ValueError("downlink FACCH did not enter handset BFI concealment")
    if not any(
        downlink > 100 and peak > 0 and time > last_downlink_facch
        for downlink, peak, _, time in handset
    ):
        raise ValueError("handset speech did not recover after downlink FACCH")

    peer = [
        (int(exchange), int(peak), int(good), int(concealed), float(time))
        for exchange, peak, good, concealed, time in PEER_RE.findall(text)
    ]
    uplink_facch = [entry for entry in facch if entry[0] == "uplink"]
    first_uplink_facch = uplink_facch[0][4]
    last_uplink_facch = uplink_facch[-1][4]
    peer_before = [entry for entry in peer if entry[4] < first_uplink_facch]
    peer_after = [entry for entry in peer if entry[4] > first_uplink_facch]
    baseline = peer_before[-1][3] if peer_before else 0
    if not peer_after or peer_after[0][3] <= baseline:
        raise ValueError("uplink FACCH did not enter network BFI concealment")
    if not any(
        exchange > 100 and peak > 0 and good and time > last_uplink_facch
        for exchange, peak, good, _, time in peer
    ):
        raise ValueError("network speech did not recover after uplink FACCH")

    return (
        "OK - organic FACCH stole "
        f"{len(uplink_facch)}/{len(downlink_facch)} uplink/downlink speech "
        "blocks, entered both BFI boundaries and recovered bidirectional media"
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
