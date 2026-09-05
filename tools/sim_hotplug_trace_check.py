#!/usr/bin/env python3
"""Validate firmware-owned SIM removal and reinsertion through MAD2 FIQ7."""

import argparse
import re
from pathlib import Path


DETECT_RE = re.compile(r"sim_detect: present=(\d) control=([0-9a-fA-F]{2})")


def verify(log: str, phase: int, require_state: bool = False) -> None:
    compact = log.replace("[:sim_card] ", "").replace("[:] ", "")
    remove = compact.find("sim_presence_fixture: present=0")
    deactivate = compact.find("SIM lifecycle event=deactivate", remove)
    inactive = compact.find("SIM lifecycle event=inactive", deactivate)
    reinsert = compact.find("sim_presence_fixture: present=1", inactive)
    activate = compact.find("SIM lifecycle event=activate", reinsert)
    phase_read = compact.find(
        f"read-binary fid=6fae offset=0 length=1 first={phase:02x}", activate
    )
    if min(remove, deactivate, inactive, reinsert, activate, phase_read) < 0:
        raise ValueError("incomplete or out-of-order removal/reinsertion lifecycle")
    if phase == 3:
        profile = compact.find("header cla=a0 ins=10 p1=00 p2=00 p3=05", phase_read)
        accepted = compact.find("SIM status ins=10 sw=9000", profile)
        if profile < 0 or accepted < 0:
            raise ValueError("Phase 2+ reinsertion did not repeat TERMINAL PROFILE")

    detections = [
        (int(present), int(control, 16), match.start())
        for match in DETECT_RE.finditer(compact)
        for present, control in [match.groups()]
    ]
    removed = next((item for item in detections if item[0] == 0 and remove < item[2] < reinsert), None)
    inserted = next((item for item in detections if item[0] == 1 and item[2] > reinsert), None)
    if removed is None or not (removed[1] & 0x08):
        raise ValueError("removal edge did not expose absent-card status bit 3")
    if inserted is None or (inserted[1] & 0x08):
        raise ValueError("reinsertion edge did not clear absent-card status bit 3")

    # MAD2's interrupt and timer diagnostics both observe the same W1C write;
    # either may be the surviving line after a trace channel reaches its cap.
    first_ack = compact.find("event=ack mask=080", remove)
    second_ack = compact.find("event=ack mask=080", reinsert)
    if first_ack < 0 or second_ack < 0:
        raise ValueError("firmware did not acknowledge both FIQ7 edges")
    if require_state and "state_roundtrip: result=pass" not in compact:
        raise ValueError("save-state round trip did not pass")


def main() -> None:
    parser = argparse.ArgumentParser()
    parser.add_argument("log", type=Path)
    parser.add_argument("--phase", type=int, choices=(2, 3), default=2)
    parser.add_argument("--require-state", action="store_true")
    args = parser.parse_args()
    try:
        verify(args.log.read_text(errors="replace"), args.phase, args.require_state)
    except ValueError as error:
        raise SystemExit(f"SIM HOTPLUG: FAIL: {error}") from error
    print(f"SIM HOTPLUG: PASS Phase {args.phase} removal and reinsertion")


if __name__ == "__main__":
    main()
