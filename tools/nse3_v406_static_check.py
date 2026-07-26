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
EEPROM_ANCHORS = {
    # Two-byte word-address framing in the common serial-memory transaction.
    0x29CDD2: ("lsrs", "r0, r7, #8"),
    0x29CDE0: ("lsls", "r0, r7, #0x18"),
    0x29CDE2: ("lsrs", "r0, r0, #0x18"),
    # Low-level byte sender: GenIO signal 0x20, SDA mask 1, SCL mask 4.
    0x29E8C4: ("movs", "r4, #0x80"),
    0x29E8C8: ("adds", "r3, #0x20"),
    0x29E8CA: ("movs", "r0, #1"),
    0x29E8CC: ("movs", "r2, #4"),
    0x29E8D4: ("ldrb", "r1, [r3, #4]"),
    0x29E90E: ("orrs", "r5, r2"),
    0x29E936: ("bics", "r5, r2"),
}
SIMI_ANCHORS = {
    # Clock gate and initialization over the standard MAD2 SIMI window.
    0x290510: ("movs", "r0, #0x20"),
    0x290512: ("ldrb", "r1, [r4, #0xd]"),
    0x29051C: ("movs", "r1, #0x3d"),
    0x29051E: ("movs", "r0, #0x18"),
    0x29052A: ("movs", "r1, #0x3e"),
    0x29052C: ("movs", "r0, #0x1a"),
    0x290538: ("movs", "r0, #0x39"),
    0x29053A: ("movs", "r1, #0x32"),
    # Activation, TX FIFO staging/commit, RX drain, and IIR acknowledge.
    0x2900FA: ("movs", "r0, #0x80"),
    0x2901B0: ("strb", "r3, [r1, #5]"),
    0x2901B4: ("strb", "r3, [r1, #8]"),
    0x2901C2: ("strb", "r3, [r1]"),
    0x2901D0: ("strb", "r5, [r1, #8]"),
    0x2903DE: ("ldrb", "r0, [r5, #5]"),
    0x2903E4: ("ldrb", "r0, [r5]"),
    0x290462: ("ldrb", "r0, [r1]"),
    0x29046A: ("strb", "r0, [r1]"),
}
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


def decode_thumb_anchors(data: bytes, anchors: dict[int, tuple[str, str]]) -> None:
    decoder = capstone.Cs(capstone.CS_ARCH_ARM, capstone.CS_MODE_THUMB)
    physical = swap16(data)
    for pc, expected in anchors.items():
        offset = pc - FLASH_BASE
        decoded = list(decoder.disasm(physical[offset : offset + 4], pc, count=1))
        actual = (decoded[0].mnemonic, decoded[0].op_str) if decoded else None
        if actual != expected:
            raise ValueError(f"Thumb anchor {pc:#x}: expected {expected}, got {actual}")


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


def verify_eeprom_boundary(data: bytes) -> dict:
    decode_thumb_anchors(data, EEPROM_ANCHORS)
    return {
        "transaction": 0x29CD88,
        "byte_sender": 0x29E8BC,
        "genio_signal": 0x20020,
        "genio_direction": 0x20024,
        "sda_bit": 0,
        "scl_bit": 2,
        "word_address_bytes": 2,
        "device_geometry_source": "NSE-3 parts list: 8 KiB serial EEPROM",
    }


def verify_simi_boundary(data: bytes) -> dict:
    decode_thumb_anchors(data, SIMI_ANCHORS)
    return {
        "driver_extent": {"start": 0x28FF84, "end": 0x2905F4},
        "clock_gate": {"register": 0x2000D, "mask": 0x20},
        "register_window": {"start": 0x20036, "end": 0x2003F},
        "control": 0x20039,
        "activation_mask": 0x80,
        "tx_data": 0x20036,
        "rx_data": 0x20037,
        "interrupt_identification": 0x20038,
        "rx_count": 0x2003C,
        "rx_fifo_control": 0x2003D,
        "tx_fifo_control": 0x2003E,
        "tx_count": 0x2003F,
        "synthetic_card_profile": "not_established",
    }


def verify(data: bytes) -> dict:
    return {
        "identity": verify_identity(data),
        "reset_boundary": verify_reset_boundary(data),
        "keypad_boundary": verify_keypad_boundary(data),
        "eeprom_boundary": verify_eeprom_boundary(data),
        "simi_boundary": verify_simi_boundary(data),
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
