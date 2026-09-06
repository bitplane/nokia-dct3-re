#!/usr/bin/env python3
"""Verify organic dedicated-mode handover success or rollback."""

import argparse
import re
from pathlib import Path


EVENTS = {
    "command": re.compile(r"gsm_session: handover command target=2 reference=5a"),
    "to_target": re.compile(r"receiver tuned old_arfcn=1 new_arfcn=2"),
    "physical_information": re.compile(
        r"RX enqueue type=80 .*data=b0[0-9a-f]*062d00"),
    "complete_message": re.compile(
        r"GSM service uplink sapi=0 pd=06 message=2c .*data=062c00"),
    "complete": re.compile(r"gsm_session: handover complete serving=2"),
    "to_serving": re.compile(r"receiver tuned old_arfcn=2 new_arfcn=1"),
    "rollback": re.compile(r"gsm_session: handover rollback serving=1"),
}
SPEECH = re.compile(
    r"dsp_hle: speech tick uplink=(\d+) downlink=(\d+) .*?t=([0-9.]+)")
ROUNDTRIP = re.compile(
    r"state_roundtrip: result=(\w+) .*?requested_at=([0-9.]+) t=([0-9.]+)")


def _positions(text: str, names: tuple[str, ...]) -> list[int]:
    positions = []
    for name in names:
        match = EVENTS[name].search(text)
        if not match:
            raise ValueError(f"missing handover event: {name}")
        positions.append(match.start())
    if positions != sorted(positions):
        raise ValueError("handover events are out of order")
    return positions


def _event_time(text: str, position: int) -> float:
    line_end = text.find("\n", position)
    line = text[position:line_end if line_end >= 0 else len(text)]
    match = re.search(r" t=([0-9.]+)", line)
    if not match:
        raise ValueError("handover event has no emulated timestamp")
    return float(match.group(1))


def check(path: Path, outcome: str, require_state: bool = False) -> str:
    text = path.read_text(errors="replace")
    if outcome == "success":
        positions = _positions(text, (
            "command", "to_target", "physical_information",
            "complete_message", "complete"))
        if EVENTS["to_serving"].search(text) or EVENTS["rollback"].search(text):
            raise ValueError("successful handover unexpectedly rolled back")
        terminal = EVENTS["complete"].search(text)
        serving = 2
    else:
        positions = _positions(
            text, ("command", "to_target", "to_serving", "rollback"))
        if EVENTS["physical_information"].search(text):
            raise ValueError("failure fixture received target Physical Information")
        if EVENTS["complete"].search(text):
            raise ValueError("failed handover unexpectedly completed")
        terminal = EVENTS["rollback"].search(text)
        serving = 1

    if require_state:
        roundtrip = ROUNDTRIP.search(text)
        if not roundtrip or roundtrip.group(1) != "pass":
            raise ValueError("missing successful emulator save/load round trip")
        requested = float(roundtrip.group(2))
        target_time = _event_time(text, positions[1])
        terminal_time = _event_time(text, positions[-1])
        if not target_time < requested < terminal_time:
            raise ValueError("save/load did not occur during handover")

    assert terminal is not None
    speech_after = [
        (int(match.group(1)), int(match.group(2)))
        for match in SPEECH.finditer(text, terminal.end())
    ]
    if not speech_after or max(up for up, _ in speech_after) < 50 or \
            max(down for _, down in speech_after) < 40:
        raise ValueError("bidirectional speech did not continue after handover")
    return f"OK - handover {outcome} left the call active on ARFCN {serving}"


def main() -> None:
    parser = argparse.ArgumentParser()
    parser.add_argument("log", type=Path)
    parser.add_argument("--outcome", choices=("success", "failure"), required=True)
    parser.add_argument("--require-state", action="store_true")
    args = parser.parse_args()
    try:
        print(check(args.log, args.outcome, args.require_state))
    except ValueError as error:
        raise SystemExit(f"FAIL - {error}") from error


if __name__ == "__main__":
    main()
