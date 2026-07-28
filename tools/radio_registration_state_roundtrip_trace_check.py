#!/usr/bin/env python3
"""Verify deterministic save/load replay across organic GSM registration."""

import argparse
import pathlib

try:
    from tools.radio_registration_trace_check import verify
    from tools.radio_state_roundtrip import verify_roundtrip
    from tools.radio_speech_media_trace_check import canonical_timeline
except ModuleNotFoundError:
    from radio_registration_trace_check import verify
    from radio_state_roundtrip import verify_roundtrip
    from radio_speech_media_trace_check import canonical_timeline


REPLAY_TOKENS = (
    "dsp_hle: radio peer RX type=",
    "dsp_hle: TX packet type=",
    "dspif_transport: RX enqueue type=",
    "dsp_hle: LAPDm ",
    "sim_device: update-binary fid=6f7e",
)


def check(text: str, profile: str = "nhm6") -> None:
    verify_roundtrip(text, REPLAY_TOKENS, "registration")
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
