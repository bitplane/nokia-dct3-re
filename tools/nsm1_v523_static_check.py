#!/usr/bin/env python3
"""Bound the acquired NSM-1 reset and DSP verifier without inventing replies."""

from __future__ import annotations

import argparse
import hashlib
import io
import json
from pathlib import Path
import zipfile

import capstone
from capstone.arm import ARM_OP_MEM, ARM_REG_PC


BASE = 0x200000
SIZE = 0x200000
SHA1 = "ea877d2ec204d0771b77fba414575407ccbc99ad"
PACKAGE_SHA256 = "69b882a55bbf8ed5fb81f7e52bd950ec631d619a0e141084d766a98860627904"
STREAM_WORDS = 127 * 512 + 510
SPANS = (
    (0x200040, 0x2000EC, "417277417929323fd0b5c97b34413f832015aa1d7938ea6152b7f523b9e4ea16"),
    (0x2A479E, 0x2A48AA, "07324fa54680e19536956abe4c25ec8be177d7fc6e6392f3ca4b34b16338d577"),
)


def read32(image: bytes, address: int) -> int:
    offset = address - BASE
    if offset < 0 or offset + 4 > len(image):
        raise ValueError(f"literal outside image: {address:#x}")
    return int.from_bytes(image[offset:offset + 4], "big")


def instruction(image: bytes, address: int, *, thumb: bool = True):
    mode = capstone.CS_MODE_THUMB if thumb else capstone.CS_MODE_ARM
    decoder = capstone.Cs(capstone.CS_ARCH_ARM, mode | capstone.CS_MODE_BIG_ENDIAN)
    decoder.detail = True
    offset = address - BASE
    decoded = list(decoder.disasm(image[offset:offset + 4], address, count=1))
    if not decoded:
        raise ValueError(f"no instruction at {address:#x}")
    return decoded[0]


def literal(image: bytes, address: int, *, thumb: bool = True) -> int:
    insn = instruction(image, address, thumb=thumb)
    if insn.mnemonic != "ldr" or len(insn.operands) != 2:
        raise ValueError(f"not a literal load at {address:#x}")
    operand = insn.operands[1]
    if operand.type != ARM_OP_MEM or operand.mem.base != ARM_REG_PC:
        raise ValueError(f"not a PC-relative load at {address:#x}")
    pc = ((address + 4) & ~3) if thumb else address + 8
    return read32(image, pc + operand.mem.disp)


def extract_stream(image: bytes) -> bytes:
    if len(image) != SIZE:
        raise ValueError("NSM-1 verifier requires its complete 2 MiB flash input")
    return b"".join(image[0x40 + index * 0x20:0x42 + index * 0x20]
                    for index in range(STREAM_WORDS)) + b"\xff" * 4


def normalize_package(package: bytes) -> bytes:
    # The self-extracting wrapper is read as ZIP data, never executed.
    try:
        from tools.extract_dct3_wintesla import decode_records
    except ModuleNotFoundError:
        from extract_dct3_wintesla import decode_records
    if hashlib.sha256(package).hexdigest() != PACKAGE_SHA256:
        raise ValueError("not the acquired NSM-1 v5.23 package")
    with zipfile.ZipFile(io.BytesIO(package)) as archive:
        mcu_base, mcu = decode_records(archive.read("nsm1ny_5.230"))
        ppm_base, ppm = decode_records(archive.read("nsm1ny_5.23c"))
    if (mcu_base, len(mcu), ppm_base, len(ppm)) != (BASE, 0xE0F00, 0x360000, 0xA0000):
        raise ValueError("NSM-1 package record extents changed")
    return mcu + b"\xff" * (ppm_base - mcu_base - len(mcu)) + ppm


def verify(image: bytes) -> dict:
    if len(image) != SIZE or hashlib.sha1(image).hexdigest() != SHA1:
        raise ValueError("not the normalized NSM-1 v5.23 PPM C image")
    for begin, end, expected in SPANS:
        if hashlib.sha256(image[begin - BASE:end - BASE]).hexdigest() != expected:
            raise ValueError(f"instruction span changed at {begin:#x}")
    anchors = {
        0x2A47A4: ("strh", "r4, [r6]"),
        0x2A47A8: ("strh", "r5, [r6, #2]"),
        0x2A4818: ("movs", "r2, #1"),
        0x2A481A: ("lsls", "r2, r2, #9"),
        0x2A4826: ("adds", "r6, #0x20"),
        0x2A484A: ("cmp", "r3, #0x7f"),
        0x2A484E: ("movs", "r6, #0xff"),
        0x2A4850: ("adds", "r6, #0xff"),
        0x2A4866: ("strh", "r5, [r0]"),
        0x2A486A: ("strh", "r5, [r0]"),
        0x2A4886: ("ldrh", "r1, [r2, #2]"),
        0x2A488C: ("beq", "#0x2a4886"),
        0x2A4892: ("strh", "r1, [r0, #0xc]"),
        0x2A4896: ("strh", "r1, [r0, #0xa]"),
    }
    for address, expected in anchors.items():
        insn = instruction(image, address)
        if (insn.mnemonic, insn.op_str) != expected:
            raise ValueError(f"instruction mismatch at {address:#x}")
    expected_literals = {
        0x2A47A0: 0x10000,
        0x2A47A6: 0xFFFF,
        0x2A480C: 0x100FE,
        0x2A480E: 0x10200,
        0x2A4812: 0x200040,
        0x2A486C: 0x10000,
        0x2A4888: 0xFFFF,
    }
    for address, expected in expected_literals.items():
        if literal(image, address) != expected:
            raise ValueError(f"literal mismatch at {address:#x}")
    stream = extract_stream(image)
    stream_sha1 = hashlib.sha1(stream).hexdigest()
    if stream_sha1 != "0fac5ba57a3504f8cb4490a97ab961f468218121":
        raise ValueError("sparse stream digest mismatch")
    return {
        "product": "NSM-1", "firmware": "5.23 PPM C", "flash_sha1": SHA1,
        "reset_entry": 0x200040,
        "reset_stack_literals": [literal(image, pc, thumb=False)
                                 for pc in (0x200068, 0x200090, 0x2000B0)],
        "loader": 0x2A479E,
        "coverage": {"pinned_instruction_bytes": sum(b - a for a, b, _ in SPANS),
                     "decoded_anchors": len(anchors), "pool_loads": len(expected_literals)},
        "stream": {"full_blocks": 127, "block_words": 512, "last_source_words": 510,
                   "stride": 0x20, "first_source": 0x200040,
                   "last_source": BASE + 0x40 + (STREAM_WORDS - 1) * 0x20,
                   "bytes": len(stream), "sha1": stream_sha1,
                   "role": "sparse_external_flash_verification_candidate",
                   "contiguous_dsp_code_image": False},
        "final_publication": {"wait_address": 0x10002, "sentinel": 0xFFFF,
                              "capture_state": literal(image, 0x2A488E),
                              "first_field_offset": 0xA, "second_field_offset": 0xC,
                              "dsp_verdict_algorithm": "unknown",
                              "matching_resident_dsp_proven": False},
        "boot_promoted": False,
    }


def main() -> None:
    parser = argparse.ArgumentParser(description=__doc__)
    parser.add_argument("image", type=Path)
    parser.add_argument("--package", type=Path, help="normalize this pinned ZIP package before checking")
    parser.add_argument("--output", type=Path)
    args = parser.parse_args()
    try:
        image = normalize_package(args.package.read_bytes()) if args.package else args.image.read_bytes()
        result = json.dumps(verify(image), indent=2) + "\n"
        if args.package:
            args.image.parent.mkdir(parents=True, exist_ok=True)
            args.image.write_bytes(image)
    except (OSError, ValueError) as exc:
        parser.exit(1, f"NSM-1 static contract: {exc}\n")
    if args.output:
        args.output.parent.mkdir(parents=True, exist_ok=True)
        args.output.write_text(result, encoding="utf-8")
    print(result, end="")


if __name__ == "__main__":
    main()
