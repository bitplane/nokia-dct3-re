#!/usr/bin/env python3
"""Verify deterministic save/load replay during multipart SMS transport."""

import pathlib
import sys

try:
    from tools.radio_incoming_smart_message_trace_check import verify
    from tools.radio_speech_media_trace_check import canonical_timeline
    from tools.radio_state_roundtrip import verify_roundtrip
except ModuleNotFoundError:
    from radio_incoming_smart_message_trace_check import verify
    from radio_speech_media_trace_check import canonical_timeline
    from radio_state_roundtrip import verify_roundtrip


REPLAY_TOKENS = (
    "dsp_hle: radio peer RX type=",
    "dsp_hle: TX packet type=",
    "dspif_transport: RX enqueue type=",
    "dsp_hle: LAPDm ",
    "dsp_hle: GSM service ",
    "dsp_hle: PCH ",
)


def check(text: str, sim_nvram: bytes) -> None:
    verify_roundtrip(text, REPLAY_TOKENS, "SMS CP/RP closing exchange")
    verify(canonical_timeline(text), sim_nvram)


def main() -> int:
    if len(sys.argv) != 3:
        raise SystemExit(
            "usage: radio_incoming_smart_message_state_trace_check.py "
            "MAME_ERROR_LOG SIM_CARD_NVRAM")
    try:
        check(
            pathlib.Path(sys.argv[1]).read_text(errors="replace"),
            pathlib.Path(sys.argv[2]).read_bytes())
    except ValueError as error:
        raise SystemExit(str(error)) from None
    print(
        "OK - multipart SMS transaction and queued successor replayed "
        "deterministically across save-state restoration")
    return 0


if __name__ == "__main__":
    raise SystemExit(main())
