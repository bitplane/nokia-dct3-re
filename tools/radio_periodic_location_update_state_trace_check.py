#!/usr/bin/env python3
"""Verify deterministic save/load across firmware T3212 expiry."""

import argparse
import pathlib

try:
    from tools.radio_periodic_location_update_trace_check import verify
    from tools.radio_speech_media_trace_check import canonical_timeline
    from tools.radio_state_roundtrip import verify_roundtrip
except ModuleNotFoundError:
    from radio_periodic_location_update_trace_check import verify
    from radio_speech_media_trace_check import canonical_timeline
    from radio_state_roundtrip import verify_roundtrip


REPLAY_TOKENS = (
    "dsp_hle: radio peer RX type=",
    "dsp_hle: TX packet type=",
    "dspif_transport: RX enqueue type=80",
    "dspif_transport: RX enqueue type=83",
    "dspif_transport: RX enqueue type=86",
    "dspif_transport: RX enqueue type=89",
    "dsp_hle: GSM service establish ",
    "dsp_hle: LAPDm ",
)


def check(text: str) -> None:
    verify_roundtrip(text, REPLAY_TOKENS, "periodic Location Updating")
    verify(canonical_timeline(text))


def main() -> int:
    parser = argparse.ArgumentParser(
        description="verify save/load replay across organic T3212 expiry")
    parser.add_argument("log", type=pathlib.Path)
    args = parser.parse_args()
    try:
        check(args.log.read_text(errors="replace"))
    except ValueError as error:
        raise SystemExit(str(error)) from None
    print(
        "OK - T3212 expiry replayed identically and the restored handset "
        "completed one periodic update")
    return 0


if __name__ == "__main__":
    raise SystemExit(main())
