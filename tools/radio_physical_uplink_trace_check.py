#!/usr/bin/env python3
"""Check non-silent physical microphone audio reaches the network decoder."""

import argparse
import re
from pathlib import Path


ENERGY_RE = re.compile(
    r"dsp_hle: speech tick uplink=(\d+) downlink=(\d+) .*?"
    r"mic_peak=(\d+) ear_peak=(\d+) nonzero=(\d+)/(\d+)"
)
VOICE_PEER_RE = re.compile(
    r"gsm_voice_peer: exchange=(\d+) uplink_peak=(\d+) "
    r"downlink_peak=(\d+) source=lab-1khz"
)


def check(path: Path) -> str:
    text = path.read_text(errors="replace")
    energy = [tuple(map(int, match.groups())) for match in ENERGY_RE.finditer(text)]
    peer = [tuple(map(int, match.groups())) for match in VOICE_PEER_RE.finditer(text)]
    if not energy or energy[-1][0] < 100:
        raise ValueError("physical microphone path did not sustain 100 DSP frames")

    uplink, _, _, _, nonzero_mic, _ = energy[-1]
    mic_peak = max(sample[2] for sample in energy)
    if nonzero_mic < 100 or mic_peak == 0:
        raise ValueError("COBBA microphone stream remained silent")
    if mic_peak >= 32768:
        raise ValueError("COBBA microphone stimulus clipped")
    if not peer or peer[-1][0] < 100:
        raise ValueError("network peer did not sustain uplink speech")
    peer_peak = max(sample[1] for sample in peer)
    if peer_peak <= 16:
        raise ValueError("network peer did not decode non-silent uplink speech")
    if peer_peak >= 32768:
        raise ValueError("decoded uplink speech clipped")

    return (
        f"OK - {nonzero_mic}/{uplink} COBBA microphone blocks were non-silent; "
        f"microphone/network peaks={mic_peak}/{peer_peak}"
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
