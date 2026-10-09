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
VERIFIER_STREAM_SHA1 = "1e9487dbc339646937dc9e5bb1bb6c3e2e759dac"
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


def verifier_stream(image):
    """Serialize the sparse flash halfwords, not a contiguous DSP program."""
    if len(image) != 0x200000:
        raise ValueError("wrong NSE-6 verifier image size")
    return b"".join(image[offset:offset + 2]
                    for offset in range(0x40, len(image), 32)) + b"\xff\xff" * 2


def integrity_arithmetic(block, excluded_word):
    """Arithmetic before the service callback, not an identity validator."""
    if len(block) not in (0xDE, 0x9E) or not 0 <= excluded_word <= 0xFFFF:
        raise ValueError("unsupported NSE-6 integrity arithmetic input")
    return (sum(block) - (excluded_word >> 8) - (excluded_word & 0xFF)) & 0xFFFF


def identity_sample(raw):
    """Observed identity-calibration branch; no electrical units implied."""
    if not isinstance(raw, int) or not 0 <= raw <= 0x3FF:
        raise ValueError("CCONT sample must be ten-bit")
    return raw * 1500 // 232


def read32(image, address):
    offset = address - BASE
    if offset < 0 or offset + 4 > len(image):
        raise ValueError("word outside NSE-6 image")
    return int.from_bytes(image[offset:offset + 4], "big")


def thumb_bl_callers(image, target, extent):
    """Syntactic Thumb-1 BL census; data/indirect edges are not classified."""
    if extent < 4 or extent > len(image) or extent % 2:
        raise ValueError("invalid Thumb census extent")
    callers = []
    pairs = 0
    for offset in range(0, extent - 3, 2):
        first = int.from_bytes(image[offset:offset + 2], "big")
        second = int.from_bytes(image[offset + 2:offset + 4], "big")
        if first & 0xF800 != 0xF000 or second & 0xF800 != 0xF800:
            continue
        pairs += 1
        displacement = ((first & 0x7FF) << 12) | ((second & 0x7FF) << 1)
        if displacement & 0x400000:
            displacement -= 0x800000
        if BASE + offset + 4 + displacement == target:
            callers.append(BASE + offset)
    return pairs, callers


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
        (0x2B61E6, 0x10000), (0x2B6202, 0xFFFF),
        (0x2B6208, 0x1205C0), (0x2DD102, 0x200005),
        (0x2D3914, 0x1205CA), (0x2D3918, 0x0B06),
        (0x2409A6, 0x6209), (0x2DFDB2, 0x13FF1C),
        (0x2882DA, 0x20033), (0x288B82, 0x20033),
        (0x288D30, 0x20033), (0x288FE6, 0x20033),
        (0x2DE296, 0x20033), (0x2E0570, 0x20033),
        (0x288CC6, 0x13FFA8), (0x288D20, 0x12147D),
        (0x221C8C, 0x13FE18),
        (0x221E20, 0x9004),
        (0x21E09A, 0x11EB59), (0x21F836, 0x11EB5E),
        (0x293D7A, 0x157C), (0x293D8A, 0x157C),
        (0x2E0B0C, 0x3033D8), (0x2E0AF2, 0x121588),
        (0x293CCE, 0x13FE18), (0x293CE8, 1500),
        (0x2E09C4, 0x3033E0), (0x2E09C0, 0x121588),
        (0x2E0B66, 0x12158C),
        (0x2DD106, 0x121570), (0x2DE16E, 0x20033),
        (0x2DE18E, 0x20031), (0x2DE198, 0x2002F),
        (0x2E04B8, 0x3033D0), (0x2E04CC, 0x3033B4),
        (0x2E1146, 0x20000), (0x2E1184, 0x20000),
        (0x2E11A0, 0x20000), (0x2DFC9C, 0x20000),
        (0x2CA5D2, 0x20036), (0x2CA81A, 0x2003D),
        (0x2CA820, 0x20037), (0x2CA8A2, 0x20038),
        (0x2CA91E, 0x20000), (0x2CA988, 0x20037))
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
        (0x240B9A, "add", "r1, sp, #4"),
        (0x240B9E, "bl", "#0x2dd07e"),
        (0x240BB4, "cmp", "r1, r0"),
        (0x240BB6, "bne", "#0x240c1e"),
        (0x240C20, "strb", "r0, [r7, #0xc]"),
        (0x240C24, "movs", "r0, #0xbf"),
        (0x240C28, "strb", "r0, [r6, r5]"),
        (0x240B46, "bl", "#0x2be71e"),
        (0x240B4E, "movs", "r0, #0x12"),
        (0x240B50, "strb", "r0, [r7, #0x12]"),
        (0x2BE720, "bl", "#0x28d5ae"),
        (0x2BE724, "cmp", "r0, #1"),
        (0x28D5B8, "movs", "r0, #0"),
        (0x28D5BC, "movs", "r2, #0x40"),
        (0x28D5C4, "movs", "r1, #0x3c"),
        (0x28D5C6, "bl", "#0x2ce3d0"),
        (0x28D5CC, "ldr", "r1, [r5, #0x3c]"),
        (0x28D5CE, "cmp", "r0, r1"),
        (0x288D1C, "cmp", "r0, #6"),
        (0x288D28, "cmp", "r0, #0xf"),
        (0x288C7A, "movs", "r0, #1"),
        (0x288C80, "strb", "r0, [r2]"),
        (0x288D46, "movs", "r0, #0xe0"),
        (0x288D4C, "strb", "r0, [r5]"),
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
        (0x2E04C2, "movs", "r1, #0x19"),
        (0x2E1148, "movs", "r1, #0x29"),
        (0x2E114C, "lsrs", "r1, r1, #1"),
        (0x2E1150, "movs", "r1, #0x2c"),
        (0x2E1152, "strb", "r0, [r1, r2]"),
        (0x2E118E, "movs", "r1, #0x2b"),
        (0x2E1190, "strb", "r0, [r1, r2]"),
        (0x2E11A4, "movs", "r0, #0x21"),
        (0x2E11B2, "movs", "r0, #0x20"),
        (0x2E1204, "movs", "r0, #0x24"),
        (0x2E120A, "movs", "r5, #0x80"),
        (0x2E120E, "movs", "r0, #0x40"),
        (0x2E122C, "strb", "r5, [r0, r4]"),
        (0x2E122E, "movs", "r0, #0x54"),
        (0x2E1238, "movs", "r1, #0x2b"),
        (0x2E123A, "strb", "r3, [r1, r4]"),
        (0x2E1248, "cmp", "r2, #6"),
        (0x2E1256, "movs", "r0, #0x20"),
        (0x2DFC9E, "movs", "r2, #0x28"),
        (0x2DFCA0, "movs", "r1, #0x22"),
        (0x2DFCA2, "strb", "r1, [r2, r0]"),
        (0x2DFCA4, "movs", "r2, #0x2a"),
        (0x2DFCA6, "movs", "r1, #4"),
        (0x2DFCAA, "strb", "r1, [r2, r0]"),
        (0x2DFCAC, "movs", "r1, #0x29"),
        (0x2DFCB0, "lsrs", "r1, r1, #3"),
        (0x2DFCB2, "blo", "#0x2dfcac"),
        (0x2DFCB4, "movs", "r1, #0x2d"),
        (0x2DFCB6, "ldrb", "r0, [r1, r0]"),
        (0x2DFBF2, "movs", "r1, #0x28"),
        (0x2DFBF4, "movs", "r0, #0x22"),
        (0x2DFC46, "movs", "r0, #0x2a"),
        (0x2DFC48, "strb", "r4, [r0, r5]"),
        (0x2CA822, "ldrb", "r0, [r5, #5]"),
        (0x2CA828, "ldrb", "r0, [r5]"),
        (0x2CA834, "bne", "#0x2ca828"),
        (0x2CA920, "movs", "r1, #0x38"),
        (0x2CA922, "movs", "r0, #0xff"),
        (0x2CA924, "strb", "r0, [r1, r4]"),
        (0x2CA97C, "movs", "r0, #0x39"),
        (0x2CA97E, "movs", "r1, #0x32"),
        (0x2CA980, "strb", "r1, [r0, r4]"),
        (0x2CA98A, "ldrb", "r0, [r1, #5]"),
        (0x2CA996, "ldrb", "r2, [r1]"),
        (0x2CA9A4, "bne", "#0x2ca992"),
        (0x2DCA92, "movs", "r0, #1"),
        (0x2DCA94, "movs", "r1, #0x14"),
        (0x2DCA96, "bl", "#0x27b318"),
        (0x21FB94, "bl", "#0x294c90"),
        (0x21FBA0, "bl", "#0x2dca88"),
        (0x222546, "cmp", "r0, #0x1d"),
        (0x222554, "mov", "pc, r0"),
        (0x21E028, "adds", "r1, #0x9c"),
        (0x21E02E, "movs", "r0, #0x49"),
        (0x221CEC, "cmp", "r0, #0x49"),
        (0x221D0C, "cmp", "r2, r1"),
        (0x221E18, "beq", "#0x221de0"),
        (0x221E22, "bl", "#0x2dfc60"),
        (0x221E26, "cmp", "r0, #1"),
        (0x221E28, "bne", "#0x221da0"),
        (0x21F7B4, "cmp", "r0, #0x21"),
        (0x21F7B8, "b", "#0x21fb1e"),
        (0x21E094, "adds", "r1, #0xa1"),
        (0x293D9E, "bl", "#0x293cc0"),
        (0x293DA4, "beq", "#0x293dc6"),
        (0x293DAE, "subs", "r0, r0, #1"),
        (0x293CC6, "movs", "r0, #7"),
        (0x293D08, "movs", "r0, #8"),
        (0x293D74, "lsls", "r2, r2, #3"),
        (0x293D78, "blt", "#0x293d92"),
        (0x293D7E, "bgt", "#0x293d92"),
        (0x293D90, "movs", "r0, #1"),
        (0x2E0AEA, "cmp", "r5, #8"),
        (0x2E0AF8, "subs", "r0, #0xc"),
        (0x2E0AFA, "ldrh", "r4, [r0]"),
        (0x293CEC, "movs", "r1, #0xe8"),
        (0x2E09CC, "bl", "#0x2e5db8"),
        (0x2E09D4, "subs", "r1, #0xc"),
        (0x2E09D6, "strh", "r0, [r1]"),
        (0x2E0B62, "lsls", "r3, r0, #0xf"),
        (0x2E0B6A, "strh", "r3, [r1, r2]"),
        (0x293D00, "ldrh", "r1, [r6, #0x3a]"),
        (0x293D02, "subs", "r0, r0, r1"))
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
    controller_targets = [read32(image, address)
                          for address in range(0x288A84, 0x288AC8, 4)]
    if controller_targets != [
            0x288B0A, 0x288AF6, 0x288F7E, 0x288EB0, 0x2890A2,
            0x287F20, 0x2885C8, 0x2885D2, 0x28929A, 0x28923A,
            0x288D88, 0x2892DE, 0x28903A, 0x289084, 0x28905E,
            0x289110, 0x288C3A]:
        raise ValueError("own NSE-6 input-controller table changed")
    report_pairs, report_callers = thumb_bl_callers(image, 0x2DCA88, 0x170000)
    if report_pairs != 35774 or report_callers != [0x21FBA0]:
        raise ValueError("own NSE-6 direct report-14 census changed")
    if read32(image, 0x222558 + 0x1A * 4) != 0x221C8C:
        raise ValueError("own NSE-6 report dispatcher state 1a changed")
    if [read32(image, 0x222558 + state * 4) for state in (3, 4)] != [
            0x221DF0, 0x221D38]:
        raise ValueError("own NSE-6 report dispatcher continuation changed")
    if image[0x103534] != 0x70:
        raise ValueError("own NSE-6 CCONT status-command table changed")
    source_selectors = image[0x1033D8:0x1033E2]
    if source_selectors != bytes.fromhex("00040506070301020201"):
        raise ValueError("own NSE-6 source-selector table changed")
    stream = verifier_stream(image)
    stream_sha1 = hashlib.sha1(stream).hexdigest()
    if stream_sha1 != VERIFIER_STREAM_SHA1:
        raise ValueError("own NSE-6 verifier stream changed")
    return {"product": "NSE-6", "version": "6.02", "ppm": "A",
            "image_sha1": IMAGE_SHA1, "reset_literals": literals,
            "service_manual_sram_bytes": 0x40000,
            "service_manual_eeprom_bytes": 0x8000,
            "eeprom_descriptor": eeprom_descriptor(image[5]),
            "security_integrity": {"calculator": "0x240918",
                                   "stored_offset": "0x11e",
                                   "secondary_offset": "0x90",
                                   "mismatch_branch": "0x240c1e",
                                   "failure_status": "0x0c",
                                   "normal_flag_mask": "0x40",
                                   "valid_provisioning_proven": False},
            "config_integrity": {"validator": "0x28d5ae",
                                  "data_offset": 0, "data_bytes": 0x3C,
                                  "stored_offset": "0x3c", "stored_bytes": 4,
                                  "sum_bits": 16, "stored_byte_order": "big",
                                  "status_index": "0x12"},
            "input_controller": {"table": "0x288a84",
                                 "targets": [hex(target) for target in controller_targets],
                                 "readiness_byte": "0x13ffa8",
                                 "readiness_low_nibble": 6,
                                 "report_byte": "0x12147d",
                                 "report_low_nibble": 15,
                                 "missing_bit_report": "0x14",
                                 "unmask_instruction": "0x288d4c"},
            "report14": {"stub": "0x2dca88", "post_primitive": "0x27b318",
                         "direct_callers": [hex(address) for address in report_callers],
                         "scanned_bytes": 0x170000, "syntactic_bl_pairs": report_pairs,
                         "indirect_callers_closed": False,
                         "dispatcher": "0x222532", "states": 30,
                         "observed_state_1a_target": "0x221c8c"},
            "acquisition": {"reader": "0x293cc0", "source_reader": "0x2e0ada",
                            "selector_table": "0x3033d8",
                            "selectors": list(source_selectors),
                            "source7_selector": 2, "source8_cache": "0x12158c",
                            "cache_writer": "0x2e09d6",
                            "cache_selectors": [2, 1], "cache_sentinel": "0x8000",
                            "gain_address": "0x13fe4c", "offset_address": "0x13fe50",
                            "scale_numerator": 1500, "scale_divisor": 232,
                            "identity_accepted_raw_range": [279, 850],
                            "physical_units_proven": False},
            "simi": {"initializer": "0x2ca910", "receiver": "0x2ca986",
                     "tx_register": "0x36", "rx_register": "0x37",
                     "iir_register": "0x38", "control_register": "0x39",
                     "rx_count_register": "0x3c", "initial_control": "0x32",
                     "runtime_card_exchange_proven": False},
            "gensio": {"ccont_reader": "0x2dfc60", "ccont_writer": "0x2dfba8",
                       "ccont_write": "0x2a", "control": "0x28",
                       "lcd_data": "0x2b", "ccont_read": "0x2d",
                       "status": "0x29", "lcd_command": "0x2c",
                       "ccont_ready_bit": 2, "ccont_select_value": "0x22"},
            "display": {"initializer": "0x2e1194", "width": 84,
                        "height": 48, "command_register": "0x2c",
                        "data_register": "0x2b", "control_register": "0x28",
                        "control_value": "0x21", "status_register": "0x29",
                        "ready_bit": 0, "pup_reset_mask": "0x20",
                        "exact_controller_part_proven": False,
                        "runtime_frame_proven": False},
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
                             "stream_sha1": stream_sha1,
                             "stream_bytes": len(stream),
                             "stride_bytes": 32, "full_blocks": 127,
                             "words_per_block": 512, "last_words": 510,
                             "buffers": ["0x10200", "0x10600"],
                             "handshake": "0x100fe",
                             "final_wait": "0x2b6200",
                             "result_registers": ["0x10000", "0x10002"],
                             "result_context": ["0x1205ca", "0x1205cc"],
                             "resident_mask_proven": False},
            "runtime_acceptance": False}


def main():
    parser = argparse.ArgumentParser(description=__doc__)
    parser.add_argument("package", type=Path)
    args = parser.parse_args()
    print(json.dumps(check(normalize(args.package.read_bytes())), indent=2))


if __name__ == "__main__":
    main()
