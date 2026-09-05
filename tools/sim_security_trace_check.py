#!/usr/bin/env python3
"""Validate organic SIM CHV transactions and their persistent card state."""

import argparse
import re
import sys
from pathlib import Path


CHV_OFFSET = 3484
PUK_OFFSET = 3500
ATTEMPTS_OFFSET = 3516
ENABLED_OFFSET = 3520
NVRAM_SIZE = 3524


def padded_pin(pin: str) -> bytes:
    return pin.encode("ascii").ljust(8, b"\xff")


def statuses(trace: str, instruction: int) -> list[tuple[int, int, int]]:
    pattern = re.compile(
        rf"SIM status ins={instruction:02x} sw=([0-9a-f]{{4}}) "
        r"chv=(\d+)/(\d+) puk=(\d+)/(\d+) enabled=(\d+)", re.I)
    return [(int(match[1], 16), int(match[2]), int(match[4]))
            for match in pattern.finditer(trace)]


def validate(trace: str, nvram: bytes, mode: str, expected_pin: str,
             previous_trace: str = "") -> None:
    if len(nvram) != NVRAM_SIZE:
        raise ValueError(f"SIM NVRAM has {len(nvram)} bytes, expected {NVRAM_SIZE}")
    if mode != "disabled" and nvram[ENABLED_OFFSET] != 1:
        raise ValueError("CHV1 is not persistently enabled")

    verify = statuses(trace, 0x20)
    unblock = statuses(trace, 0x2C)
    if mode == "verify":
        if verify != [(0x9000, 3, 10)]:
            raise ValueError(f"expected one successful VERIFY, got {verify}")
        if nvram[CHV_OFFSET:CHV_OFFSET + 8] != padded_pin(expected_pin):
            raise ValueError("VERIFY changed the persistent PIN")
    elif mode == "retry-restore":
        expected_verify = [(0x9804, 2, 10), (0x9000, 3, 10)]
        if verify != expected_verify:
            raise ValueError(f"save/restore retry sequence is {verify}, expected {expected_verify}")
        if "state_roundtrip: result=pass" not in trace:
            raise ValueError("machine state round trip did not pass")
        if nvram[ATTEMPTS_OFFSET:ATTEMPTS_OFFSET + 4] != bytes((3, 3, 10, 10)):
            raise ValueError("successful VERIFY did not restore the persistent retry count")
    elif mode == "removal":
        if verify != [(0x9000, 3, 10)]:
            raise ValueError(f"expected authentication before removal, got {verify}")
        if "SIM lifecycle event=deactivate verified_before=1/0" not in trace:
            raise ValueError("card was not removed after CHV1 verification")
        if "SIM lifecycle event=inactive verified=0/0" not in trace:
            raise ValueError("card removal did not clear session authorization")
    elif mode == "block-unblock":
        expected_verify = [(0x9804, 2, 10), (0x9804, 1, 10), (0x9840, 0, 10)]
        if verify != expected_verify:
            raise ValueError(f"wrong-PIN retry sequence is {verify}, expected {expected_verify}")
        if unblock != [(0x9000, 3, 10)]:
            raise ValueError(f"expected one successful UNBLOCK, got {unblock}")
        if nvram[CHV_OFFSET:CHV_OFFSET + 8] != padded_pin(expected_pin):
            raise ValueError("UNBLOCK did not persist the replacement PIN")
        if nvram[ATTEMPTS_OFFSET:ATTEMPTS_OFFSET + 4] != bytes((3, 3, 10, 10)):
            raise ValueError("UNBLOCK did not restore the persistent retry counters")
    elif mode == "toggle":
        disable = statuses(previous_trace, 0x26)
        enable = statuses(trace, 0x28)
        if disable != [(0x9000, 3, 10)]:
            raise ValueError(f"expected one successful DISABLE, got {disable}")
        if enable != [(0x9000, 3, 10)]:
            raise ValueError(f"expected one successful ENABLE, got {enable}")
        if statuses(trace, 0x20):
            raise ValueError("disabled-card reboot unexpectedly required startup VERIFY")
    elif mode == "change":
        old_verify = statuses(previous_trace, 0x20)
        change = statuses(previous_trace, 0x24)
        new_verify = statuses(trace, 0x20)
        if old_verify != [(0x9000, 3, 10)]:
            raise ValueError(f"expected startup VERIFY before CHANGE, got {old_verify}")
        if change != [(0x9000, 3, 10)]:
            raise ValueError(f"expected one successful CHANGE, got {change}")
        if new_verify != [(0x9000, 3, 10)]:
            raise ValueError(f"replacement PIN did not VERIFY after reboot: {new_verify}")
        if nvram[CHV_OFFSET:CHV_OFFSET + 8] != padded_pin(expected_pin):
            raise ValueError("CHANGE did not persist the replacement PIN")
    elif mode == "change-reject":
        verify = statuses(trace, 0x20)
        change = statuses(trace, 0x24)
        if verify != [(0x9000, 3, 10)]:
            raise ValueError(f"expected successful startup VERIFY, got {verify}")
        if change != [(0x9804, 2, 10)]:
            raise ValueError(f"expected rejected CHANGE and one consumed retry, got {change}")
        if nvram[CHV_OFFSET:CHV_OFFSET + 8] != padded_pin(expected_pin):
            raise ValueError("rejected CHANGE modified the persistent PIN")
        if nvram[ATTEMPTS_OFFSET:ATTEMPTS_OFFSET + 4] != bytes((2, 3, 10, 10)):
            raise ValueError("rejected CHANGE did not persist exactly one consumed retry")
    else:
        raise ValueError(f"unknown mode {mode}")


def main() -> int:
    parser = argparse.ArgumentParser()
    parser.add_argument("mode", choices=("verify", "retry-restore", "removal", "block-unblock", "toggle", "change", "change-reject"))
    parser.add_argument("trace", type=Path)
    parser.add_argument("nvram", type=Path)
    parser.add_argument("--expected-pin", default="1234")
    parser.add_argument("--previous-trace", type=Path)
    args = parser.parse_args()
    try:
        previous_trace = (args.previous_trace.read_text(errors="replace")
                          if args.previous_trace else "")
        validate(args.trace.read_text(errors="replace"), args.nvram.read_bytes(),
                 args.mode, args.expected_pin, previous_trace)
    except (OSError, UnicodeEncodeError, ValueError) as error:
        print(f"error: {error}", file=sys.stderr)
        return 1
    print(f"OK - organic SIM security lifecycle '{args.mode}' passed")
    return 0


if __name__ == "__main__":
    raise SystemExit(main())
