#!/usr/bin/env python3
"""Verify the recovered NSE-3 v5.48 ROM3/ROM4 bootstrap boundary."""

from __future__ import annotations

import argparse
import hashlib
import json
from pathlib import Path

import capstone
from capstone.arm import ARM_OP_IMM, ARM_OP_MEM, ARM_REG_PC


FLASH_BASE = 0x200000
FLASH_SIZE = 0x100000
STREAM_WORDS = 63 * 512 + 510

VARIANTS = {
    "rom3": {
        "sha1": "5768841c9eb39c744f4fa04f0485e4f9ad4553b3",
        "sha256": "3ad47781485cb776910d30fa20d440a963eae90e847cfe24748b5c4ac2f8e6e3",
        "loader": 0x2833F4,
        "loader_call": 0x297CF6,
        "delay_helper": 0x2A4C48,
        "state": 0x10BA28,
        "state_literal_roots": [
            0x283226, 0x28331E, 0x28347E, 0x283626, 0x2836E4,
            0x2838C8, 0x2838E8, 0x283908, 0x28392A, 0x283A5A,
            0x283AF2, 0x297A04, 0x297B4E, 0x297C3C,
        ],
        "eeprom_security_directory": 0x2B8524,
        "eeprom_security_records": {
            0x0701: (0x0358, 0x0008),
            0x0702: (0x0360, 0x002C),
        },
        "internal_rom_service": 0x23751A,
        "internal_rom_service_call": 0x237532,
        "internal_rom_service_setter": 0x28CB36,
        "internal_rom_diagnostic": 0x23A050,
        "internal_rom_text": 0x23A2CC,
        "formatter": 0x28C9DA,
        "acceptance": 0x29799C,
        "stream_sha1": "73ddf5f79e421fdcfff7742e238fb24ea5f1fcfa",
        "identity": [b"V  5.48", b"08-09-99", b"40.3.617", b"14-Dec-98", b"NSE-3"],
    },
    "rom4": {
        "sha1": "3bcc5c93ec247c63490e134196aab98a4e60c184",
        "sha256": "2adca0d661af2d8e7bed3e04d2941b6db9572a1eb10b2b1ebc545e33fbdd7c7f",
        "loader": 0x285010,
        "loader_call": 0x299922,
        "delay_helper": 0x2A693C,
        "state": 0x10BA38,
        "state_literal_roots": [
            0x284E42, 0x284F3A, 0x28509A, 0x285242, 0x285300,
            0x2854E4, 0x285504, 0x285524, 0x285546, 0x285676,
            0x28570E, 0x299630, 0x29977A, 0x299868,
        ],
        "eeprom_security_directory": 0x2B9838,
        "eeprom_security_records": {
            0x0701: (0x0380, 0x0008),
            0x0702: (0x0388, 0x002C),
        },
        "internal_rom_service": 0x237A5A,
        "internal_rom_service_call": 0x237A72,
        "internal_rom_service_setter": 0x28E75E,
        "internal_rom_diagnostic": 0x23A5CA,
        "internal_rom_text": 0x23A84C,
        "formatter": 0x28E602,
        "acceptance": 0x2995C8,
        "stream_sha1": "94e447d8386e010326fdfb261e247d6c0ac4d97a",
        "identity": [b"V 05.48", b"03-09-99", b"14-Dec-98", b"NSE-3"],
    },
}


def decoder() -> capstone.Cs:
    result = capstone.Cs(
        capstone.CS_ARCH_ARM,
        capstone.CS_MODE_THUMB | capstone.CS_MODE_BIG_ENDIAN,
    )
    result.detail = True
    return result


def image_instructions(data: bytes) -> list[capstone.CsInsn]:
    disassembler = decoder()
    disassembler.skipdata = True
    return list(disassembler.disasm(data, FLASH_BASE))


def instruction(data: bytes, pc: int) -> capstone.CsInsn:
    offset = pc - FLASH_BASE
    decoded = list(decoder().disasm(data[offset : offset + 4], pc, count=1))
    if not decoded:
        raise ValueError(f"no Thumb instruction at {pc:#x}")
    return decoded[0]


def verify_instruction(data: bytes, pc: int, mnemonic: str, op_str: str) -> None:
    decoded = instruction(data, pc)
    actual = (decoded.mnemonic, decoded.op_str)
    expected = (mnemonic, op_str)
    if actual != expected:
        raise ValueError(f"Thumb anchor {pc:#x}: expected {expected}, got {actual}")


def literal(data: bytes, pc: int) -> int:
    decoded = instruction(data, pc)
    if (
        decoded.mnemonic != "ldr"
        or len(decoded.operands) != 2
        or decoded.operands[1].type != ARM_OP_MEM
        or decoded.operands[1].mem.base != ARM_REG_PC
    ):
        raise ValueError(f"expected PC-relative literal load at {pc:#x}")
    address = ((pc + 4) & ~3) + decoded.operands[1].mem.disp
    offset = address - FLASH_BASE
    return int.from_bytes(data[offset : offset + 4], "big")


def verify_literal(data: bytes, pc: int, expected: int) -> None:
    actual = literal(data, pc)
    if actual != expected:
        raise ValueError(
            f"Thumb literal {pc:#x}: expected {expected:#x}, got {actual:#x}"
        )


def immediate_target(decoded: capstone.CsInsn) -> int | None:
    if (
        decoded.mnemonic not in ("bl", "blx")
        or len(decoded.operands) != 1
        or decoded.operands[0].type != ARM_OP_IMM
    ):
        return None
    return decoded.operands[0].imm


def literal_value(data: bytes, decoded: capstone.CsInsn) -> int | None:
    if (
        decoded.mnemonic != "ldr"
        or len(decoded.operands) != 2
        or decoded.operands[1].type != ARM_OP_MEM
        or decoded.operands[1].mem.base != ARM_REG_PC
    ):
        return None
    address = (
        ((decoded.address + 4) & ~3) + decoded.operands[1].mem.disp
    )
    offset = address - FLASH_BASE
    if offset < 0 or offset + 4 > len(data):
        return None
    return int.from_bytes(data[offset : offset + 4], "big")


def extract_bootstrap_stream(data: bytes) -> bytes:
    result = bytearray()
    for index in range(STREAM_WORDS):
        offset = 0x40 + index * 0x20
        result.extend(data[offset : offset + 2])
    result.extend(b"\xff\xff\xff\xff")
    return bytes(result)


def verify_variant(path: Path, name: str) -> dict:
    profile = VARIANTS[name]
    data = path.read_bytes()
    if len(data) != FLASH_SIZE:
        raise ValueError(f"{path}: expected {FLASH_SIZE:#x} bytes, got {len(data):#x}")
    sha1 = hashlib.sha1(data).hexdigest()
    sha256 = hashlib.sha256(data).hexdigest()
    if sha1 != profile["sha1"] or sha256 != profile["sha256"]:
        raise ValueError(f"{path}: unexpected {name} image identity")
    for marker in profile["identity"]:
        if marker not in data:
            raise ValueError(f"{path}: missing embedded identity {marker!r}")

    # The ROM3 and ROM4 images carry different external-EEPROM record
    # directories.  Each descriptor is {u32 id, u16 offset, u16 length}.
    # Keep these product-revision offsets distinct; a valid ROM4 template is
    # not evidence for ROM3's security/configuration layout.
    directory_offset = profile["eeprom_security_directory"] - FLASH_BASE
    expected_directory = b"".join(
        record.to_bytes(4, "big")
        + offset.to_bytes(2, "big")
        + length.to_bytes(2, "big")
        for record, (offset, length)
        in profile["eeprom_security_records"].items()
    )
    actual_directory = data[
        directory_offset : directory_offset + len(expected_directory)
    ]
    if actual_directory != expected_directory:
        raise ValueError(
            f"{path}: EEPROM security record directory changed: expected "
            f"{expected_directory.hex()}, got {actual_directory.hex()}"
        )

    base = profile["loader"]
    # v5.48 initializes four shared bootstrap cells.  The pre-upload exchange
    # uses +4/+6; this is distinct from the final +0/+2 publication.
    anchors = {
        0x00: ("push", "{r4, r5, r6, r7, lr}"),
        0x0E: ("strh", "r7, [r4]"),
        0x16: ("strh", "r1, [r4, #2]"),
        0x18: ("strh", "r1, [r4, #4]"),
        0x1C: ("strh", "r0, [r4, #6]"),
        0x92: ("ldrh", "r0, [r4, #4]"),
        0xC0: ("ldrh", "r1, [r4, #4]"),
        0xC6: ("ldrh", "r0, [r4, #6]"),
        0x11C: ("cmp", "r0, #0x3f"),
        0x134: ("strh", "r0, [r1]"),
        0x138: ("strh", "r0, [r1]"),
        0x152: ("ldrh", "r1, [r4, #2]"),
        0x15A: ("ldrh", "r0, [r4, #2]"),
        0x15E: ("strh", "r0, [r1, #0xe]"),
        0x160: ("ldrh", "r0, [r4]"),
        0x162: ("strh", "r0, [r1, #0xc]"),
        0x18A: ("pop", "{r4, r5, r6, r7, pc}"),
    }
    for delta, expected in anchors.items():
        verify_instruction(data, base + delta, *expected)

    literals = {
        0x0A: 0x10000,
        0x10: 0xFFFF,
        0x3E: 0x100F6,
        0x8E: 0x100FE,
        0x94: 0xFFFF,
        0xB4: 0xFFFF,
        0xD4: 0x10200,
        0xD8: 0x200040,
        0x154: 0xFFFF,
    }
    for delta, expected in literals.items():
        verify_literal(data, base + delta, expected)

    # State storage is separate between ROM3 and ROM4 even though the loader
    # algorithm is otherwise homologous.
    verify_literal(data, base + 0x8A, profile["state"])

    # The pre-upload publication is bounded and recoverable independently
    # from its real value.  Firmware waits for shared +4 to leave 0xffff,
    # permits only a finite retry loop, stores the observed value at state+8,
    # and enters the transfer only when shared +4 and +6 agree and the wait
    # did not time out.
    pre_upload_storage = profile["state"] + 0x08
    pre_upload_anchors = {
        0x92: ("ldrh", "r0, [r4, #4]"),
        0x96: ("cmp", "r0, r1"),
        0x98: ("bne", f"#{base + 0xc0:#x}"),
        0x9E: ("movs", "r0, #0xa"),
        0xA0: ("bl", f"#{profile['delay_helper']:#x}"),
        0xA6: ("cmp", "r0, #0x14"),
        0xB2: ("ldrh", "r1, [r4, #4]"),
        0xB6: ("cmp", "r1, r0"),
        0xC0: ("ldrh", "r1, [r4, #4]"),
        0xC4: ("strh", "r1, [r0, #8]"),
        0xC6: ("ldrh", "r0, [r4, #6]"),
        0xC8: ("cmp", "r1, r0"),
        0xCA: ("bne", f"#{base + 0x16c:#x}"),
        0xD0: ("cmp", "r0, #0"),
        0xD2: ("beq", f"#{base + 0x16c:#x}"),
    }
    for delta, expected in pre_upload_anchors.items():
        verify_instruction(data, base + delta, *expected)
    verify_literal(data, base + 0x94, 0xFFFF)
    verify_literal(data, base + 0xB4, 0xFFFF)

    internal_rom_service = profile["internal_rom_service"]
    verify_literal(data, internal_rom_service, pre_upload_storage)
    service_anchors = {
        0x02: ("ldrh", "r0, [r0]"),
        0x04: ("cmp", "r0, #0xa"),
        0x0A: ("adds", "r0, #0x30"),
        0x0C: ("strb", "r0, [r1, #0xc]"),
        0x12: ("strb", "r1, [r0, #0xd]"),
        0x14: ("movs", "r0, #9"),
    }
    for delta, expected in service_anchors.items():
        verify_instruction(data, internal_rom_service + delta, *expected)
    verify_instruction(
        data,
        profile["internal_rom_service_call"],
        "bl",
        f"#{profile['internal_rom_service_setter']:#x}",
    )

    internal_rom_diagnostic = profile["internal_rom_diagnostic"]
    verify_literal(data, internal_rom_diagnostic, pre_upload_storage)
    diagnostic_anchors = {
        0x02: ("ldrh", "r0, [r0]"),
        0x04: ("adds", "r0, #0x30"),
        0x06: ("strb", "r0, [r7, #8]"),
    }
    for delta, expected in diagnostic_anchors.items():
        verify_instruction(data, internal_rom_diagnostic + delta, *expected)
    internal_rom_text = b"DSP ROM  \x00"
    text_offset = profile["internal_rom_text"] - FLASH_BASE
    if data[text_offset : text_offset + len(internal_rom_text)] != internal_rom_text:
        raise ValueError(f"{path}: DSP ROM diagnostic text changed")

    # Both v5.48 variants later consume the first captured halfword at
    # state+0x0c.  One path renders its nibbles as a three-character Bxx
    # result, and another accepts only 0x0b06.  This is an exact external-MCU
    # constraint; the independently reported physical COBBA B07 demonstrates
    # that it must not be promoted to a fitted-silicon revision semantic.
    first_result = profile["state"] + 0x0C
    second_result = profile["state"] + 0x0E
    formatter = profile["formatter"]
    formatter_anchors = {
        0x02: ("ldrh", "r0, [r1]"),
        0x04: ("lsrs", "r0, r0, #8"),
        0x0A: ("adds", "r0, #0x37"),
        0x0C: ("strb", "r0, [r4]"),
        0x0E: ("ldrh", "r0, [r1]"),
        0x10: ("lsrs", "r0, r0, #4"),
        0x16: ("adds", "r0, #0x30"),
        0x18: ("strb", "r0, [r4, #1]"),
        0x1A: ("ldrh", "r0, [r1]"),
        0x20: ("adds", "r0, #0x30"),
    }
    verify_literal(data, formatter, first_result)
    for delta, expected in formatter_anchors.items():
        verify_instruction(data, formatter + delta, *expected)

    acceptance = profile["acceptance"]
    acceptance_anchors = {
        0x02: ("ldrh", "r1, [r0]"),
        0x06: ("cmp", "r1, r0"),
    }
    verify_literal(data, acceptance, first_result)
    verify_literal(data, acceptance + 0x04, 0x0B06)
    for delta, expected in acceptance_anchors.items():
        verify_instruction(data, acceptance + delta, *expected)

    verify_instruction(
        data, profile["loader_call"], "bl", f"#{profile['loader']:#x}"
    )
    instructions = image_instructions(data)
    direct_callers = [
        decoded.address
        for decoded in instructions
        if immediate_target(decoded) == profile["loader"]
    ]
    if direct_callers != [profile["loader_call"]]:
        raise ValueError(
            f"{path}: expected sole loader caller {profile['loader_call']:#x}, "
            f"got {[hex(address) for address in direct_callers]}"
        )
    result_literal_references = {
        address: [
            decoded.address
            for decoded in instructions
            if literal_value(data, decoded) == address
        ]
        for address in (pre_upload_storage, first_result, second_result)
    }
    state_literal_roots = [
        decoded.address
        for decoded in instructions
        if literal_value(data, decoded) == profile["state"]
    ]
    if state_literal_roots != profile["state_literal_roots"]:
        raise ValueError(
            f"{path}: bootstrap state literal roots changed: expected "
            f"{[hex(address) for address in profile['state_literal_roots']]}, "
            f"got {[hex(address) for address in state_literal_roots]}"
        )
    expected_pre_upload_references = [
        internal_rom_service,
        internal_rom_diagnostic,
    ]
    if (
        result_literal_references[pre_upload_storage]
        != expected_pre_upload_references
    ):
        raise ValueError(
            f"{path}: pre-upload result direct references changed: expected "
            f"{[hex(address) for address in expected_pre_upload_references]}, "
            f"got {[hex(address) for address in result_literal_references[pre_upload_storage]]}"
        )
    expected_first_references = [formatter, acceptance]
    if result_literal_references[first_result] != expected_first_references:
        raise ValueError(
            f"{path}: first-result direct references changed: expected "
            f"{[hex(address) for address in expected_first_references]}, got "
            f"{[hex(address) for address in result_literal_references[first_result]]}"
        )
    if result_literal_references[second_result]:
        raise ValueError(
            f"{path}: second-result direct literal references changed: "
            f"{[hex(address) for address in result_literal_references[second_result]]}"
        )

    stream = extract_bootstrap_stream(data)
    stream_sha1 = hashlib.sha1(stream).hexdigest()
    if stream_sha1 != profile["stream_sha1"]:
        raise ValueError(f"{path}: unexpected staged bootstrap stream")

    return {
        "variant": name,
        "size": len(data),
        "sha1": sha1,
        "sha256": sha256,
        "loader": base,
        "loader_direct_callers": direct_callers,
        "shared_cells": [0x10000, 0x10002, 0x10004, 0x10006],
        "pre_upload_cells": [0x10004, 0x10006],
        "pre_upload_exchange": {
            "initial_value": 0xFFFF,
            "wait_cell": 0x10004,
            "agreement_cell": 0x10006,
            "transfer_requires_equality": True,
            "retry_delay_raw": 10,
            "retry_counter_limit": 20,
            "timeout_is_fail_closed": True,
            "captured_storage": pre_upload_storage,
            "captured_direct_literal_references":
                result_literal_references[pre_upload_storage],
            "service_selector": 0x09,
            "service_encoding": "single ASCII digit for values below 10",
            "diagnostic_label": "DSP ROM",
            "dsp_publication_value": "not_static",
        },
        "final_publication_cells": [0x10000, 0x10002],
        "initial_sentinel": 0xFFFF,
        "transfer_blocks": 64,
        "stream_sha1": stream_sha1,
        "state": profile["state"],
        "state_literal_roots": state_literal_roots,
        "eeprom_security_directory": profile["eeprom_security_directory"],
        "eeprom_security_records": {
            f"{record:#06x}": {
                "offset": offset,
                "length": length,
            }
            for record, (offset, length)
            in profile["eeprom_security_records"].items()
        },
        "final_result_capture": {
            "first_storage": first_result,
            "first_direct_literal_references":
                result_literal_references[first_result],
            "first_required_value": 0x0B06,
            "first_render": "B06",
            "physical_cobba_revision_semantic_proven": False,
            "second_storage": second_result,
            "second_direct_literal_references":
                result_literal_references[second_result],
            # The exact-image whole-state census has fourteen literal roots.
            # Inspection of every root-owning routine finds no state pointer
            # passed to another routine and no read at state+0x0e.  The sole
            # access is the loader's anchored STRH capture above.
            "second_state_root_reads": [],
            "second_pointer_escapes": [],
            "second_mcu_access": "capture_write_only",
            "second_value": "unknown",
        },
    }


def main() -> None:
    parser = argparse.ArgumentParser()
    parser.add_argument("--rom3", type=Path, required=True)
    parser.add_argument("--rom4", type=Path, required=True)
    parser.add_argument("--json", type=Path)
    args = parser.parse_args()

    result = {
        "rom3": verify_variant(args.rom3, "rom3"),
        "rom4": verify_variant(args.rom4, "rom4"),
        "rom4_loader_delta": VARIANTS["rom4"]["loader"] - VARIANTS["rom3"]["loader"],
        "profiles_are_interchangeable": False,
    }
    encoded = json.dumps(result, indent=2, sort_keys=True) + "\n"
    if args.json:
        args.json.parent.mkdir(parents=True, exist_ok=True)
        args.json.write_text(encoded)
    print(encoded, end="")


if __name__ == "__main__":
    main()
