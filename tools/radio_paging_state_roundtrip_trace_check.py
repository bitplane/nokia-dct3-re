#!/usr/bin/env python3
"""Verify deterministic save/load replay across organic GSM paging."""

import pathlib
import sys

try:
    from tools.radio_paging_trace_check import verify
    from tools.radio_state_roundtrip import verify_roundtrip
    from tools.radio_speech_media_trace_check import canonical_timeline
except ModuleNotFoundError:
    from radio_paging_trace_check import verify
    from radio_state_roundtrip import verify_roundtrip
    from radio_speech_media_trace_check import canonical_timeline


REPLAY_TOKENS = (
    "dsp_hle: radio peer RX type=",
    "dsp_hle: TX packet type=",
    "dspif_transport: RX enqueue type=",
    "dsp_hle: LAPDm ",
    "dsp_hle: PCH ",
)


def check(text: str) -> None:
    verify_roundtrip(text, REPLAY_TOKENS, "paging")
    verify(canonical_timeline(text))


def main() -> int:
    if len(sys.argv) != 2:
        raise SystemExit(
            "usage: radio_paging_state_roundtrip_trace_check.py MAME_ERROR_LOG")
    try:
        check(pathlib.Path(sys.argv[1]).read_text(errors="replace"))
    except ValueError as error:
        raise SystemExit(str(error)) from None
    print("OK - paging replayed identically after save-state restoration")
    return 0


if __name__ == "__main__":
    raise SystemExit(main())
