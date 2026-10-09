#!/usr/bin/env python3
"""Verify missing PCM hardware cannot be replaced by synthetic GSM speech."""

import argparse
import re
from pathlib import Path


ROUTE_RE = re.compile(
    r"dsp_shared_control: command=08 value=([0-9a-f]{4}) "
    r"commit=1 .*?t=([0-9.]+)"
)
BLOCKED_RE = re.compile(
    r"dsp_hle: speech blocked by unsupported PCM link "
    r"control=([0-9a-f]{4}) enabled=(\d) clock=(\d+)/(\d+) "
    r"shape=(\d+) .*?t=([0-9.]+)"
)
EXCHANGE_RE = re.compile(
    r"gsm_voice_peer: exchange=(\d+) .*?uplink_good=(\d) .*?t=([0-9.]+)"
)
DOORBELL_RE = re.compile(
    r"dsp_hle: doorbell pending=[0-9a-f]{4} wire=([0-9a-f]{4}) "
    r"speech_control=([0-9a-f]{4}) t=([0-9.]+)"
)


def check(path: Path, expected_link=(0, 520000, 8000, 65)) -> str:
    text = path.read_text(errors="replace")
    routes = list(ROUTE_RE.finditer(text))
    blocked = BLOCKED_RE.search(text)
    if not routes:
        raise ValueError("firmware did not request its organic speech field")
    if not blocked:
        raise ValueError("missing PCM link did not block the speech data plane")
    control = int(blocked.group(1), 16)
    actual_link = tuple(int(blocked.group(index)) for index in range(2, 6))
    if actual_link != expected_link:
        raise ValueError(f"unsupported PCM profile {actual_link} differs from {expected_link}")
    blocked_time = float(blocked.group(6))
    route = next((
        match for match in routes
        if int(match.group(1), 16) >> 12 in (0x00, 0x08)
        and int(match.group(1), 16) & 0x0fff == control
        and float(match.group(2)) <= blocked_time
    ), None)
    if route is None:
        wires = ", ".join(match.group(1) for match in routes)
        raise ValueError(
            f"command-08 wires [{wires}] did not reach DSP control "
            f"{control:04x}"
        )
    return check_absent_media(text)


def check_unclocked(path: Path, expected_control: int) -> str:
    """Check a product with no PCM clock, hence no speech-timer diagnostic."""
    text = path.read_text(errors="replace")
    requests = [
        (int(wire, 16), int(control, 16))
        for wire, control, _ in DOORBELL_RE.findall(text)
    ]
    if (0x8000 | expected_control, expected_control) not in requests:
        raise ValueError("own command-08 doorbell did not apply the expected speech control")
    return check_absent_media(text)


def check_absent_media(text: str) -> str:
    if "dsp_hle: speech tick" in text:
        raise ValueError("codec/PCM speech clock ran without the physical link")
    exchanges = [
        (int(count), int(uplink_good), float(time))
        for count, uplink_good, time in EXCHANGE_RE.findall(text)
    ]
    if not exchanges:
        raise ValueError("independent network downlink did not remain active")
    if any(uplink_good for _, uplink_good, _ in exchanges):
        raise ValueError("good uplink speech reached the network without PCM")
    if "dsp_hle: GSM service uplink sapi=0 pd=03 message=2a" not in text:
        raise ValueError("organic call did not reach Release Complete")
    return (
        "OK - firmware requested speech and completed call teardown while the "
        "missing PCM link emitted no codec or good uplink media and the "
        "independent network downlink remained active"
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
