#!/usr/bin/env python3
"""Verify deterministic save/load replay across organic GSM reselection."""

import argparse
import pathlib

try:
    from tools.radio_reselection_trace_check import (
        verify_different_lac,
        verify_all_cell_loss,
        verify_loss_recovery,
        verify_same_lac,
    )
    from tools.radio_speech_media_trace_check import canonical_timeline
    from tools.radio_state_roundtrip import verify_roundtrip
except ModuleNotFoundError:
    from radio_reselection_trace_check import (
        verify_different_lac,
        verify_all_cell_loss,
        verify_loss_recovery,
        verify_same_lac,
    )
    from radio_speech_media_trace_check import canonical_timeline
    from radio_state_roundtrip import verify_roundtrip


REPLAY_TOKENS = (
    "dsp_hle: radio peer RX type=",
    "dsp_hle: TX packet type=",
    "dspif_transport: RX enqueue type=",
    "dsp_hle: LAPDm ",
    "sim_device: update-binary fid=6f7e",
    "dsp_hle: receiver tuned ",
    "dsp_hle: serving cell selected ",
)


def check(
        text: str, profile: str, radio_profile: str = "nse8",
        serving_arfcn: int = 1, neighbour_arfcn: int = 2) -> None:
    verify_roundtrip(text, REPLAY_TOKENS, "reselection")
    timeline = canonical_timeline(text)
    if profile == "same-lac":
        verify_same_lac(
            timeline, serving_arfcn, neighbour_arfcn, radio_profile)
    elif profile == "different-lac":
        verify_different_lac(
            timeline, radio_profile, serving_arfcn, neighbour_arfcn)
    elif profile == "loss-recovery":
        verify_loss_recovery(timeline, radio_profile)
    else:
        verify_all_cell_loss(timeline, radio_profile)


def main() -> int:
    parser = argparse.ArgumentParser(
        description="verify save/load replay across organic GSM reselection")
    parser.add_argument("log", type=pathlib.Path)
    parser.add_argument(
        "--profile",
        choices=("same-lac", "different-lac", "loss-recovery", "all-cell-loss"),
        default="different-lac",
    )
    parser.add_argument(
        "--radio-profile",
        choices=("nse8", "nhm5", "nhm6", "nhm2"), default="nse8")
    parser.add_argument("--serving-arfcn", type=int, default=1)
    parser.add_argument("--neighbour-arfcn", type=int, default=2)
    args = parser.parse_args()
    try:
        check(
            args.log.read_text(errors="replace"),
            args.profile,
            args.radio_profile,
            args.serving_arfcn,
            args.neighbour_arfcn,
        )
    except ValueError as error:
        raise SystemExit(str(error)) from None
    print(
        "OK - reselection replayed identically after save-state restoration "
        f"({args.profile})"
    )
    return 0


if __name__ == "__main__":
    raise SystemExit(main())
