#!/usr/bin/env python3
"""Verify the reproducible NSE-3 v4.06 reset and direct-MMIO boundary.

This checker is deliberately firmware-specific.  It proves properties of the
identified normalized image; it does not assign semantics to MAD2 registers or
claim compatibility with a particular internal ROM revision.
"""

from __future__ import annotations

import argparse
import hashlib
import json
import struct
from pathlib import Path

import capstone

try:
    from tools.mad2_static_census import analyze_image
except ModuleNotFoundError:  # Direct execution from tools/.
    from mad2_static_census import analyze_image


FLASH_BASE = 0x200000
FLASH_SIZE = 0x100000
SRAM_BASE = 0x100000
SRAM_SIZE = 0x10000
EXPECTED_SHA1 = "5025a6ac3b4a13714211fde903f27f92cbb7c9b6"
VECTOR_SOURCE = 0x200180
KEYMAP_ADDRESS = 0x2BE8BC
KEYMAP = bytes.fromhex(
    "3e 3e 3e 3e 3e "
    "11 19 01 02 03 "
    "0e 17 04 05 06 "
    "0f 18 07 08 09 "
    "10 1a 0c 0a 0b"
)
SPECIAL_KEYMAP_ADDRESS = 0x2BE8D8
SPECIAL_KEYMAP = bytes.fromhex("3e 3e 3e 3e 0d")
EXPECTED_CENSUS = {
    "literal_seeds": 225,
    "resolved_accesses": 548,
    "offsets": [
        0x00, 0x01, 0x02, 0x03, 0x04, 0x06, 0x08, 0x09,
        0x0A, 0x0B, 0x0C, 0x0D, 0x0E, 0x0F, 0x10, 0x11,
        0x12, 0x15, 0x16, 0x18, 0x19, 0x1A, 0x1B, 0x1C,
        0x1E, 0x20, 0x22, 0x24, 0x28, 0x29, 0x33, 0x36,
        0x37, 0x38, 0x39, 0x3A, 0x3B, 0x3C, 0x3D, 0x3E,
        0x3F,
    ],
}

ENTRY_ANCHORS = {
    0x200040: ("mov", "r1, #0x200000"),
    0x200044: ("ldr", "r0, [r1]"),
    0x200058: ("mov", "r1, #0x40000"),
    0x20005C: ("str", "r0, [r1]"),
    0x2000B8: ("ldr", "r0, [pc, #0xa8]"),
    0x2000BC: ("mov", "r1, #0"),
    0x2000C0: ("ldm", "r0, {r2, r3, r4, r5, r6, r7, r8, sb}"),
    0x2000C4: ("stm", "r1, {r2, r3, r4, r5, r6, r7, r8, sb}"),
    0x2000E4: ("add", "r0, pc, #1"),
    0x2000E8: ("bx", "r0"),
}


def swap16(data: bytes) -> bytes:
    if len(data) % 2:
        raise ValueError("swap16 input must contain an even number of bytes")
    result = bytearray(data)
    result[0::2], result[1::2] = data[1::2], data[0::2]
    return bytes(result)


def verify_identity(data: bytes) -> dict:
    digest = hashlib.sha1(data).hexdigest()
    if len(data) != FLASH_SIZE:
        raise ValueError(f"expected {FLASH_SIZE:#x}-byte flash, got {len(data):#x}")
    if digest != EXPECTED_SHA1:
        raise ValueError(f"expected NSE-3 v4.06 SHA-1 {EXPECTED_SHA1}, got {digest}")
    return {"size": len(data), "sha1": digest}


def verify_reset_boundary(data: bytes) -> dict:
    if len(data) < 0x180:
        raise ValueError("image is too short for the NSE-3 reset boundary")
    decoder = capstone.Cs(
        capstone.CS_ARCH_ARM, capstone.CS_MODE_ARM | capstone.CS_MODE_BIG_ENDIAN
    )
    decoded = {
        insn.address: (insn.mnemonic, insn.op_str)
        for insn in decoder.disasm(data[0x40:0xEC], FLASH_BASE + 0x40)
    }
    for pc, expected in ENTRY_ANCHORS.items():
        if decoded.get(pc) != expected:
            raise ValueError(
                f"reset anchor {pc:#x}: expected {expected}, got {decoded.get(pc)}"
            )

    literals = {
        "vector_source": struct.unpack_from(">I", data, 0x168)[0],
        "irq_fiq_stack": struct.unpack_from(">I", data, 0x16C)[0],
        "supervisor_stack": struct.unpack_from(">I", data, 0x170)[0],
        "abort_stack": struct.unpack_from(">I", data, 0x174)[0],
        "system_stack": struct.unpack_from(">I", data, 0x178)[0],
    }
    if literals["vector_source"] != VECTOR_SOURCE:
        raise ValueError(
            f"expected vector source {VECTOR_SOURCE:#x}, "
            f"got {literals['vector_source']:#x}"
        )
    for name, address in literals.items():
        if name != "vector_source" and not SRAM_BASE <= address < SRAM_BASE + SRAM_SIZE:
            raise ValueError(f"{name} {address:#x} lies outside 64 KiB NSE-3 SRAM")
    return {
        "entry": FLASH_BASE + 0x40,
        "vector_destination": 0,
        "thumb_transition": FLASH_BASE + 0xE4,
        "literals": literals,
    }


def verify_mad2_census(data: bytes) -> dict:
    accesses, coverage = analyze_image(swap16(data))
    offsets = sorted({access["offset"] for access in accesses})
    actual = {
        "literal_seeds": coverage["literal_seeds"],
        "resolved_accesses": coverage["resolved_accesses"],
        "offsets": offsets,
    }
    if actual != EXPECTED_CENSUS:
        raise ValueError(f"MAD2 census changed: expected {EXPECTED_CENSUS}, got {actual}")
    actual["maximum_offset"] = offsets[-1]
    return actual


def verify_keypad_boundary(data: bytes) -> dict:
    offset = KEYMAP_ADDRESS - FLASH_BASE
    special_offset = SPECIAL_KEYMAP_ADDRESS - FLASH_BASE
    if data[offset : offset + len(KEYMAP)] != KEYMAP:
        raise ValueError(f"normal 5x5 keypad table changed at {KEYMAP_ADDRESS:#x}")
    if data[special_offset : special_offset + len(SPECIAL_KEYMAP)] != SPECIAL_KEYMAP:
        raise ValueError(f"special keypad table changed at {SPECIAL_KEYMAP_ADDRESS:#x}")
    return {
        "normal_table": KEYMAP_ADDRESS,
        "indexing": "drive_row_times_5_plus_sense_column",
        "volume_down": {"drive_row": 1, "sense_column": 0, "keycode": 0x11},
        "volume_up": {"drive_row": 4, "sense_column": 0, "keycode": 0x10},
        "special_table": SPECIAL_KEYMAP_ADDRESS,
        "power_keycode": 0x0D,
    }


def verify(data: bytes) -> dict:
    return {
        "identity": verify_identity(data),
        "reset_boundary": verify_reset_boundary(data),
        "keypad_boundary": verify_keypad_boundary(data),
        "mad2_direct_access_census": verify_mad2_census(data),
        "claims": {
            "rom3_compatibility": "candidate_not_proven",
            "register_semantics": "not_assigned",
            "boot_promotion": False,
        },
    }


def main() -> None:
    parser = argparse.ArgumentParser()
    parser.add_argument("flash", type=Path)
    parser.add_argument("--json", type=Path)
    args = parser.parse_args()
    result = verify(args.flash.read_bytes())
    if args.json:
        args.json.parent.mkdir(parents=True, exist_ok=True)
        args.json.write_text(json.dumps(result, indent=2) + "\n")
    print(
        "verified NSE-3 v4.06 static boundary: "
        f"entry={result['reset_boundary']['entry']:#x}, "
        f"MAD2-sites={result['mad2_direct_access_census']['resolved_accesses']}, "
        "boot-promotion=no"
    )


if __name__ == "__main__":
    main()
