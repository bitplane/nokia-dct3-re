#!/usr/bin/env python3
"""Check an organic GSM 11.14 DISPLAY TEXT transaction."""

import argparse
import hashlib
from pathlib import Path


DISPLAY_TEXT_FRAME = "c7bd6cefccb59547637f778572ab0c37a2a1108fbb0bd31414262ec63698f27f"


def require_in_order(text: str, fragments: list[str]) -> None:
    position = 0
    for fragment in fragments:
        found = text.find(fragment, position)
        if found < 0:
            raise ValueError(f"missing or out-of-order SAT event: {fragment}")
        position = found + len(fragment)


def verify(log: str, frames: list[Path], require_state: bool = False,
           removal: bool = False) -> None:
    compact = log.replace("[:sim_card] ", "")
    if removal:
        require_in_order(compact, [
            "header cla=a0 ins=10 p1=00 p2=00 p3=05",
            "proactive DISPLAY TEXT ready",
            "sim_presence_fixture: present=0",
            "SIM lifecycle event=deactivate",
            "SIM lifecycle event=inactive",
        ])
        inactive = compact.index("SIM lifecycle event=inactive")
        if "header cla=a0 ins=12" in compact[inactive:]:
            raise ValueError("removed card delivered a stale proactive command")
        return
    require_in_order(compact, [
        "read-binary fid=6fae offset=0 length=1 first=03",
        "header cla=a0 ins=10 p1=00 p2=00 p3=05",
        "SIM status ins=10 sw=9000",
        "proactive DISPLAY TEXT ready",
        "SIM status ins=d6 sw=9116",
        "header cla=a0 ins=12 p1=00 p2=00 p3=16",
        "header cla=a0 ins=14 p1=00 p2=00 p3=0c",
        "terminal-response data=810301218002028281030100",
        "SIM status ins=14 sw=9000",
    ])
    if require_state and "state_roundtrip: result=pass" not in log:
        raise ValueError("save-state round trip did not pass")
    hashes = {hashlib.sha256(path.read_bytes()).hexdigest() for path in frames}
    if DISPLAY_TEXT_FRAME not in hashes:
        raise ValueError("firmware-rendered DCT3 SAT frame was not captured")


def main() -> None:
    parser = argparse.ArgumentParser()
    parser.add_argument("log", type=Path)
    parser.add_argument("frames", type=Path)
    parser.add_argument("--require-state", action="store_true")
    parser.add_argument("--removal", action="store_true")
    args = parser.parse_args()
    try:
        verify(args.log.read_text(errors="replace"),
               sorted(args.frames.glob("*.pgm")), args.require_state,
               args.removal)
    except ValueError as error:
        raise SystemExit(f"SIM TOOLKIT: FAIL: {error}") from error
    if args.removal:
        print("SIM TOOLKIT: PASS card removal cancelled the pending command")
    else:
        print("SIM TOOLKIT: PASS organic DISPLAY TEXT and terminal response")


if __name__ == "__main__":
    main()
