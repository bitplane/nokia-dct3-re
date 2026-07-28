#!/usr/bin/env python3
"""Verify deterministic save/load replay across organic GSM registration."""

import argparse
import pathlib
import re

try:
    from tools.radio_registration_trace_check import verify
    from tools.radio_speech_media_trace_check import canonical_timeline
except ModuleNotFoundError:
    from radio_registration_trace_check import verify
    from radio_speech_media_trace_check import canonical_timeline


ROUNDTRIP_RE = re.compile(
    r"state_roundtrip: result=(\w+) .*?requested_at=([0-9.]+) t=([0-9.]+)"
)
MARKER_RE = re.compile(
    r"state_replay: phase=(reference|restored) event=(begin|end) t=([0-9.]+)"
)
REPLAY_TOKENS = (
    "dsp_hle: radio peer RX type=",
    "dsp_hle: TX packet type=",
    "dspif_transport: RX enqueue type=",
    "dsp_hle: LAPDm ",
    "sim_device: update-binary fid=6f7e",
)


def records(lines: list[str], begin: int, end: int) -> list[str]:
    result = []
    for line in lines[begin + 1:end]:
        for token in REPLAY_TOKENS:
            if token in line:
                result.append(line[line.index(token):])
                break
    return result


def check(text: str, profile: str = "nhm6") -> None:
    roundtrip = ROUNDTRIP_RE.search(text)
    if not roundtrip or roundtrip.group(1) != "pass":
        raise ValueError("missing successful emulator save/load round trip")
    requested = float(roundtrip.group(2))
    restored = float(roundtrip.group(3))
    if restored < requested - 0.000_001 or restored > requested + 0.012:
        raise ValueError(
            f"restore did not resume the saved timeline: {requested} -> {restored}"
        )

    lines = text.splitlines()
    markers = [
        (match.group(1), match.group(2), float(match.group(3)), index)
        for index, line in enumerate(lines)
        if (match := MARKER_RE.search(line))
    ]
    expected = [
        ("reference", "begin"),
        ("reference", "end"),
        ("restored", "begin"),
        ("restored", "end"),
    ]
    if [(phase, event) for phase, event, _, _ in markers] != expected:
        raise ValueError("missing or misordered deterministic replay markers")

    reference = records(lines, markers[0][3], markers[1][3])
    replayed = records(lines, markers[2][3], markers[3][3])
    if not reference:
        raise ValueError("registration replay interval contained no radio records")
    if replayed != reference:
        raise ValueError("restored registration trace diverged from the reference interval")

    verify(canonical_timeline(text), profile)


def main() -> int:
    parser = argparse.ArgumentParser(
        description="verify save/load replay across organic GSM registration")
    parser.add_argument("log", type=pathlib.Path)
    parser.add_argument("--profile", default="nhm6")
    args = parser.parse_args()
    try:
        check(args.log.read_text(errors="replace"), args.profile)
    except ValueError as error:
        raise SystemExit(str(error)) from None
    print("OK - registration replayed identically after save-state restoration")
    return 0


if __name__ == "__main__":
    raise SystemExit(main())
