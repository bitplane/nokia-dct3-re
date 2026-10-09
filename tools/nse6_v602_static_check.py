#!/usr/bin/env python3
"""Verify acquired NSE-6 normalization/reset facts; no runtime provisioning."""

import argparse
import configparser
import hashlib
import io
import json
from pathlib import Path
import zipfile

from tools.extract_dct3_wintesla import decode_records

PACKAGE_SHA256 = "abc2fa6a0b1b0f5e33206c7ffb5ce17e81ab46e155733584e23aa3459b3349e9"
IMAGE_SHA1 = "e3b548816fa027da906be3daf049ce6332a9f257"
BASE = 0x200000


def normalize(package):
    if hashlib.sha256(package).hexdigest() != PACKAGE_SHA256:
        raise ValueError("not the acquired NSE-6 v6.02 package")
    with zipfile.ZipFile(io.BytesIO(package)) as archive:
        config = configparser.ConfigParser(interpolation=None)
        config.read_string(archive.read("nse-6.ini").decode("latin-1"))
        mcu_name = config["NSE-6_RESTOREFILES"]["ImageFile"]
        ppm_name = config["EURO_A"]["PpmFile"]
        mcu_base, mcu = decode_records(archive.read(mcu_name))
        ppm_base, ppm = decode_records(archive.read(ppm_name))
    if (mcu_base, len(mcu), ppm_base, len(ppm)) != (
            BASE, 0x170000, 0x370000, 0x90000):
        raise ValueError("NSE-6 record extents changed")
    return mcu + ppm


def read32(image, address):
    offset = address - BASE
    if offset < 0 or offset + 4 > len(image):
        raise ValueError("word outside NSE-6 image")
    return int.from_bytes(image[offset:offset + 4], "big")


def eeprom_descriptor(value):
    """Decode the fields used by NSE-6 routine 0x2dd100."""
    page_shift = ((value >> 3) & 7) - 1
    if not 0 <= value <= 255 or page_shift < 0:
        raise ValueError("unsupported EEPROM descriptor")
    return {"capacity_bytes": 1 << ((value & 7) + 9),
            "page_bytes": 1 << page_shift,
            "address_bytes": 2 if value & 0x40 else 1}


def check(image):
    if len(image) != 0x200000 or hashlib.sha1(image).hexdigest() != IMAGE_SHA1:
        raise ValueError("not the acquired NSE-6 v6.02 PPM A image")
    import capstone
    from capstone.arm import ARM_OP_MEM, ARM_REG_PC
    decoder = capstone.Cs(capstone.CS_ARCH_ARM,
                          capstone.CS_MODE_ARM | capstone.CS_MODE_BIG_ENDIAN)
    decoder.detail = True
    literals = {}
    expected_literals = (
        (0x200068, 0x125F30), (0x200090, 0x125F30),
        (0x2000B0, 0x125F30), (0x2000B8, 0x200180),
        (0x2000F4, 0x100020), (0x2000F6, 0x1216CC),
        (0x2B611A, 0x10000), (0x2B6120, 0xFFFF),
        (0x2B6144, 0x100F6), (0x2B6186, 0x100FE),
        (0x2B6188, 0x10200), (0x2B618C, 0x200040),
        (0x2B6202, 0xFFFF), (0x2DD102, 0x200005),
        (0x2DD106, 0x121570), (0x2DE16E, 0x20033),
        (0x2DE18E, 0x20031), (0x2DE198, 0x2002F),
        (0x2E04B8, 0x3033D0), (0x2E04CC, 0x3033B4))
    for address, expected in expected_literals:
        thumb = address >= 0x2000EC
        decoder.mode = ((capstone.CS_MODE_THUMB if thumb else capstone.CS_MODE_ARM)
                        | capstone.CS_MODE_BIG_ENDIAN)
        offset = address - BASE
        insn = next(decoder.disasm(image[offset:offset + 4], address))
        if (insn.mnemonic != "ldr" or len(insn.operands) != 2
                or insn.operands[1].type != ARM_OP_MEM
                or insn.operands[1].mem.base != ARM_REG_PC):
            raise ValueError("reset literal instruction changed")
        pc = ((address + 4) & ~3) if thumb else address + 8
        value = read32(image, pc + insn.operands[1].mem.disp)
        if value != expected:
            raise ValueError("reset literal value changed")
        literals[f"{address:#x}"] = f"{value:#x}"
    decoder.mode = capstone.CS_MODE_THUMB | capstone.CS_MODE_BIG_ENDIAN
    expected_instructions = (
        (0x2000EC, "bl", "#0x27afe8"),
        (0x2000F0, "bl", "#0x23190e"),
        (0x200128, "bl", "#0x2d330e"),
        (0x2D3338, "bl", "#0x2b6118"),
        (0x2B619A, "strh", "r6, [r0]"),
        (0x2B61A0, "adds", "r6, #0x20"),
        (0x2B61C4, "cmp", "r3, #0x7f"),
        (0x2B6206, "beq", "#0x2b6200"),
        (0x2DE3DC, "movs", "r4, #0x80"),
        (0x2DE3DE, "lsls", "r3, r4, #0xa"),
        (0x2DE3E0, "adds", "r3, #0x20"),
        (0x2DE3E2, "movs", "r0, #1"),
        (0x2DE3E4, "movs", "r2, #4"),
        (0x2DE3EA, "strb", "r5, [r3]"),
        (0x2DE3F0, "strb", "r1, [r3, #4]"),
        (0x2DE428, "strb", "r5, [r3]"),
        (0x2DE450, "strb", "r5, [r3]"),
        (0x2DD10E, "adds", "r2, #9"),
        (0x2DD112, "str", "r3, [r0, #4]"),
        (0x2DD116, "lsrs", "r3, r1, #3"),
        (0x2DD11C, "subs", "r3, #1"),
        (0x2DD120, "strb", "r2, [r0, #1]"),
        (0x2DD122, "lsrs", "r1, r1, #6"),
        (0x2DD128, "strb", "r1, [r0]"),
        (0x2DCF3A, "lsrs", "r0, r7, #8"),
        (0x2DCF40, "bl", "#0x2de3d4"),
        (0x2DCF4C, "bl", "#0x2de3d4"),
        (0x2DE484, "ldrb", "r1, [r3]"),
        (0x2DE486, "eors", "r0, r1"),
        (0x2DE488, "lsls", "r7, r0, #0x1f"),
        (0x2DE4A0, "movs", "r6, #1"),
        (0x2DE4A2, "movs", "r7, #4"),
        (0x2DE4B2, "strb", "r2, [r3, #4]"),
        (0x2DE4F6, "ldrb", "r1, [r3]"),
        (0x2DE4F8, "tst", "r1, r6"),
        (0x2DE1A6, "ldrb", "r0, [r6, #1]"),
        (0x2DE1C8, "movs", "r4, #1"),
        (0x2DE1F0, "ldrb", "r0, [r6, #2]"),
        (0x2DE212, "lsls", "r0, r4, #2"),
        (0x2DE214, "adds", "r0, r4, r0"),
        (0x2DE216, "adds", "r0, r1, r0"),
        (0x2DE22A, "cmp", "r1, #5"),
        (0x2DE236, "cmp", "r4, #5"),
        (0x2DE246, "subs", "r0, #0x80"),
        (0x2E049C, "bl", "#0x2de164"),
        (0x2E04C2, "movs", "r1, #0x19"))
    for address, mnemonic, operands in expected_instructions:
        offset = address - BASE
        insn = next(decoder.disasm(image[offset:offset + 4], address))
        if (insn.mnemonic, insn.op_str) != (mnemonic, operands):
            raise ValueError(f"instruction contract changed at {address:#x}")
    normal = image[0x1033B4:0x1033B4 + 25]
    special = image[0x1033D0:0x1033D0 + 5]
    if (normal.hex() != "3e3e3e3e3e11190102030e170405060f18070809101a0c0a0b"
            or special.hex() != "3e3e3e3e0d"):
        raise ValueError("own NSE-6 keypad tables changed")
    return {"product": "NSE-6", "version": "6.02", "ppm": "A",
            "image_sha1": IMAGE_SHA1, "reset_literals": literals,
            "service_manual_sram_bytes": 0x40000,
            "service_manual_eeprom_bytes": 0x8000,
            "eeprom_descriptor": eeprom_descriptor(image[5]),
            "keypad": {"scanner": "0x2de164", "decoder": "0x2e049a",
                       "row_register": "0x31", "column_register": "0x30",
                       "direction_register": "0x2f", "mask_register": "0x33",
                       "columns": 5, "normal_table": normal.hex(),
                       "special_table": special.hex(), "power_column_mask": 16,
                       "runtime_input_proven": False},
            "eeprom_tx": {"entry": "0x2de3d4", "pup_data": "0x20020",
                          "pup_direction": "0x20024", "sda_bit": 0,
                          "scl_bit": 2},
            "dsp_verifier": {"entry": "0x2b6118", "source": "0x200040",
                             "stride_bytes": 32, "full_blocks": 127,
                             "words_per_block": 512, "last_words": 510,
                             "buffers": ["0x10200", "0x10600"],
                             "handshake": "0x100fe",
                             "final_wait": "0x2b6200",
                             "resident_mask_proven": False},
            "runtime_acceptance": False}


def main():
    parser = argparse.ArgumentParser(description=__doc__)
    parser.add_argument("package", type=Path)
    args = parser.parse_args()
    print(json.dumps(check(normalize(args.package.read_bytes())), indent=2))


if __name__ == "__main__":
    main()
