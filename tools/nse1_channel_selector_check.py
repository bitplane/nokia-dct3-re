#!/usr/bin/env python3
"""Execute the NSE-1 v5.30 channel selector in isolation, not a handset boot.

This proves the byte-domain mapping of one ROM routine. It does not prove
that live channel assignment supplies those arguments or activates speech.
"""

import argparse
import hashlib
import json
import sys
from pathlib import Path

from unicorn import Uc, UC_ARCH_ARM, UC_MODE_BIG_ENDIAN, UC_MODE_THUMB
from unicorn.arm_const import (
    UC_ARM_REG_LR, UC_ARM_REG_PC, UC_ARM_REG_R0, UC_ARM_REG_R1,
)


ROM_SHA1 = "5aa0692c57bb726187c6686dff5789b928f8a26a"
BASE = 0x200000
ENTRY = 0x20e198
RETURN = 0x2ff000


def expected_selector(first, second):
    alternate = second in (0x20, 0x21)
    choices = {
        0: 0xb1 if alternate else 0xb0,
        1: 0xc0 if alternate else 0xc1,
        3: 0xd9,
        11: 0xd4 if alternate else 0xd5,
        15: 0xdb,
        19: 0xd2 if alternate else 0xd3,
        33: 0xc3,
    }
    # The unmatched subtract cascade returns its final residual, not zero.
    return choices.get(first, (first - 33) & 0xffffffff)


def check_image(raw):
    if len(raw) != 0x100000 or hashlib.sha1(raw).hexdigest() != ROM_SHA1:
        raise ValueError("requires the identified unswapped NSE-1 v5.30 image")
    cpu = Uc(UC_ARCH_ARM, UC_MODE_THUMB | UC_MODE_BIG_ENDIAN)
    cpu.mem_map(BASE, len(raw))
    cpu.mem_write(BASE, raw)
    count = 0
    speech_inputs = set()
    for first in range(256):
        for second in range(256):
            cpu.reg_write(UC_ARM_REG_R0, first)
            cpu.reg_write(UC_ARM_REG_R1, second)
            cpu.reg_write(UC_ARM_REG_LR, RETURN | 1)
            cpu.emu_start(ENTRY | 1, RETURN, count=64)
            if cpu.reg_read(UC_ARM_REG_PC) != RETURN:
                raise ValueError("selector did not return within instruction bound")
            actual = cpu.reg_read(UC_ARM_REG_R0)
            expected = expected_selector(first, second)
            if actual != expected:
                raise ValueError(
                    f"selector({first:#x}, {second:#x}): "
                    f"actual={actual:#x} expected={expected:#x}"
                )
            count += 1
            if actual == 0xc3:
                speech_inputs.add(first)
    if speech_inputs != {0x21, 0xe4}:
        raise ValueError(f"unexpected c3 input set: {speech_inputs}")
    return {"cases": count, "c3_first_arguments": sorted(speech_inputs),
            "explicit_c3_branch_input": 0x21,
            "unmatched_residual_c3_input": 0xe4,
            "c3_second_argument_range": [0, 255],
            "scope": "isolated ROM routine; live caller arguments unproved"}


def main():
    parser = argparse.ArgumentParser(description=__doc__)
    parser.add_argument("rom", type=Path)
    args = parser.parse_args()
    try:
        result = check_image(args.rom.read_bytes())
    except (OSError, ValueError) as error:
        print(str(error), file=sys.stderr)
        return 1
    print(json.dumps(result, sort_keys=True))
    return 0


if __name__ == "__main__":
    raise SystemExit(main())
