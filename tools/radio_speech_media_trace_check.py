#!/usr/bin/env python3
"""Check organic call media crosses the radio/DSP/COBBA boundaries."""

import argparse
import re
from pathlib import Path


SPEECH_RE = re.compile(
    r"dsp_hle: speech tick uplink=(\d+) downlink=(\d+) .*?t=([0-9.]+)"
)
PCM_RE = re.compile(
    r"dsp_hle: speech tick .*?pcm=(\d+) pcm_clock=(\d+)/(\d+) pcm_shape=(\d+) "
    r"serial_clocks=(\d+)/(\d+)"
)
ROUTE_RE = re.compile(
    r"dsp_shared_control: command=08 value=([0-9a-f]{4}) commit=1 .*?t=([0-9.]+)"
)
STOP_RE = re.compile(r"dsp_hle: speech stop control=([0-9a-f]{4}) .*?t=([0-9.]+)")
ENERGY_RE = re.compile(
    r"dsp_hle: speech tick .*?mic_peak=(\d+) ear_peak=(\d+) "
    r"nonzero=(\d+)/(\d+)"
)
VOICE_PEER_RE = re.compile(
    r"gsm_voice_peer: exchange=(\d+) uplink_peak=(\d+) "
    r"downlink_peak=(\d+) source=lab-1khz"
)
FACCH_RE = re.compile(
    r"radio_l1: direction=(uplink|downlink) kind=facch good=(\d) "
    r"count=(\d+) fn=(\d+)"
)


def check(path: Path) -> str:
    text = path.read_text(errors="replace")
    if "speech blocked by unsupported PCM link" in text:
        raise ValueError("product PCM link was not supported")
    if "speech PCM transfer rejected" in text:
        raise ValueError("MAD2/COBBA rejected a speech transfer")
    samples = [
        (int(match.group(1)), int(match.group(2)), float(match.group(3)))
        for match in SPEECH_RE.finditer(text)
    ]
    controls = [
        (match.group(1), float(match.group(2)))
        for match in ROUTE_RE.finditer(text)
    ]
    routes = [time for value, time in controls if value == "060b"]
    if not routes:
        raise ValueError("missing firmware-committed NSE-8 speech-route field")
    route_time = routes[0]
    if len(samples) < 5:
        raise ValueError("too few DSP speech-clock observations")
    pcm = [
        tuple(map(int, match.groups()))
        for match in PCM_RE.finditer(text)
    ]
    if not pcm or pcm[-1][0] < 100 or pcm[-1][1:4] != (520_000, 8_000, 65):
        raise ValueError("MAD2/COBBA PCM clock boundary was not sustained")
    blocks, _, _, _, serial_clocks, idle_clocks = pcm[-1]
    if serial_clocks != blocks * 160 * 65 or idle_clocks != blocks * 160 * 48:
        raise ValueError("MAD2/COBBA serial word/idle placement was not sustained")
    if samples[0][:2] != (1, 0):
        raise ValueError(f"speech did not start at a fresh codec state: {samples[0]}")
    if samples[0][2] < route_time:
        raise ValueError("PCM crossed COBBA before firmware enabled the DSP speech route")
    if samples[-1][0] < 100 or samples[-1][1] < 100:
        raise ValueError(f"bidirectional media did not continue: {samples[-1]}")
    energy = [tuple(map(int, match.groups())) for match in ENERGY_RE.finditer(text)]
    if not energy or energy[-1][3] < 100 or energy[-1][1] == 0:
        raise ValueError("decoded downlink never reached COBBA with non-zero energy")
    voice_peer = [
        tuple(map(int, match.groups()))
        for match in VOICE_PEER_RE.finditer(text)
    ]
    if not voice_peer or voice_peer[-1][0] < 100 or voice_peer[-1][2] != 4096:
        raise ValueError("laboratory remote source did not sustain GSM-FR downlink")
    facch = [
        (direction, int(good), int(count), int(frame))
        for direction, good, count, frame in FACCH_RE.findall(text)
    ]
    if not any(direction == "downlink" and good and count
               for direction, good, count, _ in facch):
        raise ValueError("organic downlink FACCH did not cross the burst decoder")
    if any(right[0] <= left[0] or right[1] < left[1]
           for left, right in zip(samples, samples[1:])):
        raise ValueError("speech counters are not monotonic")
    first_ticks = samples[:3]
    if any(abs((right[2] - left[2]) - 0.020) > 0.000_001
           for left, right in zip(first_ticks, first_ticks[1:])):
        raise ValueError(f"DSP speech clock is not 20 ms: {first_ticks}")
    disables = [
        time for value, time in controls
        if value == "040a" and time > route_time
    ]
    if disables:
        stops = [
            (value, float(time))
            for value, time in STOP_RE.findall(text)
        ]
        if not stops:
            raise ValueError("PCM did not stop during organic call teardown")
        value, stop_time = min(stops, key=lambda item: abs(item[1] - disables[0]))
        if value not in ("060b", "040a") or abs(stop_time - disables[0]) > 0.050:
            raise ValueError(
                "PCM stop did not coincide with TCH/firmware-route teardown"
            )
    return (
        f"OK - organic call carried {samples[-1][0]} encoded microphone frames "
        f"and {samples[-1][1]} decoded earpiece frames, including "
        f"{energy[-1][3]} non-silent COBBA blocks"
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
