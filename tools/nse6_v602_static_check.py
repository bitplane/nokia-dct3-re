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


def check(image):
    if len(image) != 0x200000 or hashlib.sha1(image).hexdigest() != IMAGE_SHA1:
        raise ValueError("not the acquired NSE-6 v6.02 PPM A image")
    import capstone
    from capstone.arm import ARM_OP_MEM, ARM_REG_PC
    decoder = capstone.Cs(capstone.CS_ARCH_ARM,
                          capstone.CS_MODE_ARM | capstone.CS_MODE_BIG_ENDIAN)
    decoder.detail = True
    literals = {}
    for address, expected in ((0x200068, 0x125F30), (0x200090, 0x125F30),
                              (0x2000B0, 0x125F30), (0x2000B8, 0x200180)):
        offset = address - BASE
        insn = next(decoder.disasm(image[offset:offset + 4], address))
        if (insn.mnemonic != "ldr" or len(insn.operands) != 2
                or insn.operands[1].type != ARM_OP_MEM
                or insn.operands[1].mem.base != ARM_REG_PC):
            raise ValueError("reset literal instruction changed")
        value = read32(image, address + 8 + insn.operands[1].mem.disp)
        if value != expected:
            raise ValueError("reset literal value changed")
        literals[f"{address:#x}"] = f"{value:#x}"
    return {"product": "NSE-6", "version": "6.02", "ppm": "A",
            "image_sha1": IMAGE_SHA1, "reset_literals": literals,
            "service_manual_sram_bytes": 0x40000,
            "service_manual_eeprom_bytes": 0x8000,
            "runtime_acceptance": False}


def main():
    parser = argparse.ArgumentParser(description=__doc__)
    parser.add_argument("package", type=Path)
    args = parser.parse_args()
    print(json.dumps(check(normalize(args.package.read_bytes())), indent=2))


if __name__ == "__main__":
    main()
