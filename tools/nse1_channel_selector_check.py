#!/usr/bin/env python3
"""Execute NSE-1 v5.30 channel selection and packet copy in isolation.

This proves the selector byte domain and ring copy at every start cursor.
It does not prove
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
    UC_ARM_REG_SP,
    UC_ARM_REG_R4, UC_ARM_REG_R5, UC_ARM_REG_R6,
    UC_ARM_REG_R9,
    UC_ARM_REG_R8,
    UC_ARM_REG_R2,
)


ROM_SHA1 = "5aa0692c57bb726187c6686dff5789b928f8a26a"
BASE = 0x200000
ENTRY = 0x20e198
RETURN = 0x2ff000


def check_ring_copy(raw):
    """Execute packet wrapper through producer commit, before scheduler calls."""
    cpu = Uc(UC_ARCH_ARM, UC_MODE_THUMB | UC_MODE_BIG_ENDIAN)
    cpu.mem_map(BASE, len(raw))
    cpu.mem_write(BASE, raw)
    cpu.mem_map(0x100000, 0x10000)
    cpu.mem_map(0x10000, 0x1000)
    payload = bytes(range(20))
    obj = 0x10b000
    cases = 0
    for cursor in range(0x52):
        cpu.mem_write(0x10000, bytes(0x1000))
        cpu.mem_write(0x100a4, cursor.to_bytes(2, "big"))
        cpu.mem_write(obj, bytes((0, 2, 20, 2)) + payload)
        cpu.reg_write(UC_ARM_REG_SP, 0x10f000)
        cpu.reg_write(UC_ARM_REG_R0, len(payload))
        cpu.reg_write(UC_ARM_REG_R1, obj + 3)
        cpu.reg_write(UC_ARM_REG_LR, RETURN | 1)
        cpu.emu_start(0x27185b, 0x271840, count=1000)
        if cpu.reg_read(UC_ARM_REG_PC) != 0x271840:
            raise ValueError("packet copy did not reach producer commit")
        expected_words = [0x1402] + [
            int.from_bytes(payload[i:i + 2], "big") for i in range(0, 20, 2)
        ]
        for index, word in enumerate(expected_words):
            address = 0x10000 + 2 * ((cursor + index) % 0x52)
            actual = int.from_bytes(cpu.mem_read(address, 2), "big")
            if actual != word:
                raise ValueError(f"ring cursor {cursor}: word {index} differs")
        producer = int.from_bytes(cpu.mem_read(0x100a4, 2), "big")
        if producer != (cursor + len(expected_words)) % 0x52:
            raise ValueError("packet copy published incorrect producer")
        cases += 1
    return cases


def check_request_routes(cpu):
    """Stop at selected handler entry, before timer/allocator side effects."""
    cpu.mem_map(0x100000, 0x10000)
    routes = {0x03ed: 0x20f102, 0x03ef: 0x20f0f2,
              0x03f1: 0x20f0ec, 0x03f2: 0x20f0e8}
    for request, target in routes.items():
        cpu.mem_write(0x100000, request.to_bytes(2, "big"))
        cpu.reg_write(UC_ARM_REG_SP, 0x100f00)
        cpu.reg_write(UC_ARM_REG_R0, 0x100000)
        cpu.emu_start(0x20f095, target, count=64)
        if cpu.reg_read(UC_ARM_REG_PC) != target:
            raise ValueError(f"request {request:#x} did not select {target:#x}")
    return len(routes)


def check_request_fields(cpu, raw):
    base = int.from_bytes(raw[0x6c6d8:0x6c6dc], "big")
    entry = 0x6c3dc + 4 * (0x03ed - base)
    if int.from_bytes(raw[entry:entry + 4], "big") != 0x26c5b0:
        raise ValueError("03ed constructor table entry differs")
    cases = 0
    for channel in range(256):
        for mode in (0, 1, 0x21, 0xe4):
            cpu.mem_write(0x10b000, bytes([0xee]) * 16)
            cpu.mem_write(0x109650, bytes([channel]))
            cpu.mem_write(0x109739, bytes([mode]))
            cpu.reg_write(UC_ARM_REG_R4, 0x10b000)
            cpu.reg_write(UC_ARM_REG_R5, 0x109736)
            cpu.reg_write(UC_ARM_REG_R6, 0x109650)
            cpu.emu_start(0x26c5f9, 0x26c614, count=32)
            if cpu.reg_read(UC_ARM_REG_PC) != 0x26c614:
                raise ValueError("request field constructor did not finish")
            enabled = channel in (0x20, 0x21, 0x2f)
            actual = bytes(cpu.mem_read(0x10b006, 2))
            expected = bytes((int(enabled), mode if enabled else 0xee))
            if actual != expected:
                raise ValueError(f"request fields differ for channel {channel:#x}")
            cases += 1
    return cases


def check_mode_writer(cpu):
    cpu.reg_write(UC_ARM_REG_SP, 0x100f00)
    cpu.emu_start(0x24ea71, 0x24ea90, count=32)
    if cpu.reg_read(UC_ARM_REG_PC) != 0x24ea90:
        raise ValueError("source binding did not reach initialization boundary")
    if (cpu.reg_read(UC_ARM_REG_R8), cpu.reg_read(UC_ARM_REG_R9)) != (
            0x10961a, 0x10963c):
        raise ValueError("channel/mode staging record bindings differ")
    for mode in range(256):
        cpu.mem_write(0x10963c, bytes((0, mode, mode ^ 0xff)))
        cpu.reg_write(UC_ARM_REG_R0, 0x109739)
        cpu.reg_write(UC_ARM_REG_R9, 0x10963c)
        cpu.emu_start(0x24fa29, 0x24fa34, count=16)
        if cpu.reg_read(UC_ARM_REG_PC) != 0x24fa34:
            raise ValueError("mode writer did not reach its boundary")
        if bytes(cpu.mem_read(0x109739, 1)) != bytes([mode]):
            raise ValueError("mode writer changed source byte 1")
        if bytes(cpu.mem_read(0x109740, 1)) != bytes([mode ^ 0xff]):
            raise ValueError("mode writer changed source byte 2")
    return 256


def check_mode_parser(cpu, raw):
    allowed = (0, 1, 0x21, 0x41, 0x0f, 3, 0x0b, 0x13)
    if raw[0xaab70:0xaab78] != bytes(allowed):
        raise ValueError("mode element acceptance table differs")
    for value, present, length in (
            (value, present, length)
            for present, length in ((0, 1), (0, 0), (1, 1))
            for value in range(256)):
        cpu.mem_write(0x109738, bytes((present, 0xee)))
        cpu.mem_write(0x10b100, bytes([value]))
        cpu.reg_write(UC_ARM_REG_SP, 0x100f00)
        cpu.reg_write(UC_ARM_REG_R0, 0x63)
        cpu.reg_write(UC_ARM_REG_R1, 0x10b100)
        cpu.reg_write(UC_ARM_REG_R2, length)
        cpu.reg_write(UC_ARM_REG_LR, RETURN | 1)
        cpu.emu_start(0x23ca45, RETURN, count=256)
        if cpu.reg_read(UC_ARM_REG_PC) != RETURN:
            raise ValueError("mode parser did not return within bound")
        actual = bytes(cpu.mem_read(0x109738, 2))
        expected = (bytes((1, value))
                    if not present and length and value in allowed
                    else bytes((present, 0xee)))
        if actual != expected:
            raise ValueError(
                f"mode parser differs for {value:#x}, present={present}, length={length}")
    return 768


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


def check_optional_mode_parser(cpu):
    allowed = (0, 1, 0x21, 0x41, 0x0f, 3, 0x0b, 0x13)
    for present in (0, 1):
        for value in range(256):
            cpu.mem_write(0x109738, bytes((present, 0xee)))
            cpu.mem_write(0x10b100, bytes((0x63, value)))
            cpu.reg_write(UC_ARM_REG_SP, 0x100f00)
            cpu.reg_write(UC_ARM_REG_R0, 0x2e)
            cpu.reg_write(UC_ARM_REG_R1, 0x10b100)
            cpu.reg_write(UC_ARM_REG_R2, 2)
            cpu.reg_write(UC_ARM_REG_LR, RETURN | 1)
            cpu.emu_start(0x23d335, RETURN, count=1024)
            if cpu.reg_read(UC_ARM_REG_PC) != RETURN:
                raise ValueError("optional mode parser did not return within bound")
            expected = (bytes((1, value)) if not present and value in allowed
                        else bytes((present, 0xee)))
            if bytes(cpu.mem_read(0x109738, 2)) != expected:
                raise ValueError(
                    f"optional mode parser differs for {value:#x}, present={present}")
    return 512


def check_channel_description(cpu, raw):
    if raw[0xaab90:0xaab98] != bytes.fromhex("2f20408000010307"):
        raise ValueError("channel description mapping tables differ")
    for first in range(256):
        cpu.mem_write(0x109650, bytes([0xee]) * 12)
        cpu.mem_write(0x10b100, bytes((first, 0, 1)))
        cpu.reg_write(UC_ARM_REG_SP, 0x100f00)
        cpu.reg_write(UC_ARM_REG_R0, 0x64)
        cpu.reg_write(UC_ARM_REG_R1, 0x10b100)
        cpu.reg_write(UC_ARM_REG_R2, 3)
        cpu.reg_write(UC_ARM_REG_LR, RETURN | 1)
        cpu.emu_start(0x23ca45, RETURN, count=512)
        if cpu.reg_read(UC_ARM_REG_PC) != RETURN:
            raise ValueError("channel description did not return within bound")
        kind = first >> 3
        actual = bytes(cpu.mem_read(0x109650, 12))
        if 1 <= kind < 16:
            group = kind.bit_length() - 1
            channel = (kind & (0, 1, 3, 7)[group]) | (0x2f, 0x20, 0x40, 0x80)[group]
            if actual[:9] != bytes((channel, first & 7, 0, 0, 0, 0, 0, 1, 0)):
                raise ValueError(f"channel description differs for {first:#x}")
        elif actual != bytes([0xee]) * 12:
            raise ValueError(f"rejected channel description wrote fields for {first:#x}")
    return 256


def check_channel_parameters(cpu):
    for packed in range(65536):
        second, third = packed >> 8, packed & 255
        cpu.mem_write(0x109650, bytes([0xee]) * 12)
        cpu.mem_write(0x10b100, bytes((8, second, third)))
        cpu.reg_write(UC_ARM_REG_SP, 0x100f00)
        cpu.reg_write(UC_ARM_REG_R0, 0x64)
        cpu.reg_write(UC_ARM_REG_R1, 0x10b100)
        cpu.reg_write(UC_ARM_REG_R2, 3)
        cpu.reg_write(UC_ARM_REG_LR, RETURN | 1)
        cpu.emu_start(0x23ca45, RETURN, count=512)
        if cpu.reg_read(UC_ARM_REG_PC) != RETURN:
            raise ValueError("channel parameters did not return within bound")
        hopping = bool(second & 0x10)
        channel_number = ((second & 3) << 8) | third
        actual = bytes(cpu.mem_read(0x109650, 12))
        if hopping:
            expected = bytes((0x2f, 0, second >> 5, 1, 0,
                              ((second & 15) << 2) | (third >> 6),
                              0, 0, third & 63))
        elif 1 <= channel_number <= 124:
            expected = bytes((0x2f, 0, second >> 5, 0, 0, 0,
                              channel_number >> 8, channel_number & 255, 0))
        else:
            expected = bytes([0xee]) * 9
        if actual[:9] != expected:
            raise ValueError(f"channel parameters differ for {packed:#06x}")
    return 65536


def check_rr_receive_route(cpu):
    for message_id in (0x05ea, 0x05eb):
        cpu.mem_write(0x10b000, message_id.to_bytes(2, "big"))
        cpu.reg_write(UC_ARM_REG_R4, 0x10b000)
        cpu.emu_start(0x26caed, 0x26c98c, count=128)
        if cpu.reg_read(UC_ARM_REG_PC) != 0x26c98c:
            raise ValueError(f"RR receive route differs for {message_id:#x}")
        for protocol in range(256):
            cpu.mem_write(0x10b008, (0x10b100).to_bytes(4, "big"))
            cpu.mem_write(0x10b003, bytes([0x5a]))
            cpu.mem_write(0x10b104, bytes([protocol]))
            cpu.reg_write(UC_ARM_REG_R4, 0x10b000)
            target = 0x26c9c0 if protocol & 15 == 6 else 0x26c998
            cpu.emu_start(0x26c98d, target, count=16)
            if cpu.reg_read(UC_ARM_REG_PC) != target:
                raise ValueError(f"RR protocol selection differs for {protocol:#x}")
            if protocol & 15 == 6 and (
                    cpu.reg_read(UC_ARM_REG_R0), cpu.reg_read(UC_ARM_REG_R1)
                    ) != (0x10b100, 0x5a):
                raise ValueError("RR receive route changed parser arguments")
    return 512


def check_rr_payload_allocation(cpu):
    for kind in (0, 0x40, 0x50, 0x60, 0xa0, 0xff):
        for length in range(256):
            cpu.mem_write(0x10b000, bytes(12))
            cpu.mem_write(0x10b257, bytes([length ^ 0xff]))
            cpu.mem_write(0x10b264, bytes([kind]))
            cpu.mem_write(0x10b22b, bytes([length]))
            cpu.reg_write(UC_ARM_REG_R4, 0x10b000)
            cpu.reg_write(UC_ARM_REG_R5, 0x10b100)
            fixed = kind in (0x40, 0x50, 0x60, 0xa0)
            target = 0x237ee0 if fixed else 0x237ec6
            cpu.emu_start(0x237e99, target, count=64)
            if cpu.reg_read(UC_ARM_REG_PC) != target:
                raise ValueError("RR allocation branch did not reach its boundary")
            if cpu.reg_read(UC_ARM_REG_R0) != (26 if fixed else length + 4):
                raise ValueError("RR payload allocation size differs")
            if bytes(cpu.mem_read(0x10b003, 2)) != bytes((length ^ 0xff, 1)):
                raise ValueError("RR payload metadata differs")
    return 1536


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
    routes = check_request_routes(cpu)
    return {"cases": count, "c3_first_arguments": sorted(speech_inputs),
            "request_routes": routes,
            "request_field_cases": check_request_fields(cpu, raw),
            "mode_writer_cases": check_mode_writer(cpu),
            "mode_parser_cases": check_mode_parser(cpu, raw),
            "optional_mode_parser_cases": check_optional_mode_parser(cpu),
            "channel_description_cases": check_channel_description(cpu, raw),
            "channel_parameter_cases": check_channel_parameters(cpu),
            "rr_receive_route_cases": check_rr_receive_route(cpu),
            "rr_payload_allocation_cases": check_rr_payload_allocation(cpu),
            "ring_copy_start_positions": check_ring_copy(raw),
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
