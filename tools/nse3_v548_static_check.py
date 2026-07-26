#!/usr/bin/env python3
"""Verify the recovered NSE-3 v5.48 ROM3/ROM4 bootstrap boundary."""

from __future__ import annotations

import argparse
import hashlib
import json
from pathlib import Path

import capstone
from capstone.arm import ARM_OP_MEM, ARM_REG_PC


FLASH_BASE = 0x200000
FLASH_SIZE = 0x100000
STREAM_WORDS = 63 * 512 + 510

VARIANTS = {
    "rom3": {
        "sha1": "5768841c9eb39c744f4fa04f0485e4f9ad4553b3",
        "sha256": "3ad47781485cb776910d30fa20d440a963eae90e847cfe24748b5c4ac2f8e6e3",
        "loader": 0x2833F4,
        "state": 0x10BA28,
        "stream_sha1": "73ddf5f79e421fdcfff7742e238fb24ea5f1fcfa",
        "identity": [b"V  5.48", b"08-09-99", b"40.3.617", b"14-Dec-98", b"NSE-3"],
    },
    "rom4": {
        "sha1": "3bcc5c93ec247c63490e134196aab98a4e60c184",
        "sha256": "2adca0d661af2d8e7bed3e04d2941b6db9572a1eb10b2b1ebc545e33fbdd7c7f",
        "loader": 0x285010,
        "state": 0x10BA38,
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
        "shared_cells": [0x10000, 0x10002, 0x10004, 0x10006],
        "pre_upload_cells": [0x10004, 0x10006],
        "final_publication_cells": [0x10000, 0x10002],
        "initial_sentinel": 0xFFFF,
        "transfer_blocks": 64,
        "stream_sha1": stream_sha1,
        "state": profile["state"],
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
