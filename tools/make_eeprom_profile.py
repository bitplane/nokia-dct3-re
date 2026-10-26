#!/usr/bin/env python3
"""Build the documented synthetic 3210 24C128 self-test profile."""

import argparse
from dataclasses import dataclass
from pathlib import Path
import struct


SIZE = 0x4000
FLASH_BASE = 0x200000
TUNE_SECURITY_CHECKSUM_OFFSET = 0x011C
TUNE_SECURITY_CHECKSUM_END = 0x0120
CONFIG_START = 0x0120
CONFIG_CHECKSUM_OFFSET = 0x0244
IDENTITY_OFFSET = 0x000C
SECURITY_CODE_OFFSET = 0x0110
SECURITY_STATE_OFFSET = 0x06C8
SECURITY_XOR = bytes((0x00, 0xFF, 0xFF, 0xFF))
SECURITY_CALLBACK_STATE = 0x11
DISPLAY_PROFILE_KEY = 0x0749
DISPLAY_PROFILE_LENGTH = 12
FALLBACK_RECORD_KEY = 0x0757
FALLBACK_RECORD_LENGTH = 400
# v6.00 0x29a802 and its v5.01 equivalent 0x296d6e construct these
# three reset profiles before committing descriptor 0x0749 variants 0..2.
# None marks bytes the constructor does not write; retain their erased state
# rather than presenting zero-filled scratch RAM as authored NV data.
# Descriptor 0x074c holds one four-byte record per built-in game (five
# variants in v6.00, three in v5.01): big-endian top score, level index 0..8 into the ROM speed table
# at 0x2d9738, and one byte the loader (0x29a0e2) does not consume. Loader and
# saver (0x299e5e) copy bytes 0..2 into the RAM records at 0x11040c. An erased
# level byte indexes past the nine-entry table to speed 0, which makes Snake
# advance every scheduler pass and leaves the level selector unable to move.
GAMES_RECORD_KEY = 0x074C
GAMES_RECORD_LENGTH = 4
GAMES_RECORD_VARIANTS = 5
GAMES_RECORD_DEFAULT = (0x00, 0x00, 0x00, None)
DISPLAY_PROFILE_DEFAULTS = (
    (0x00, 0x09, 0x01, 0x34, 0x01, 0x04, 0x01, 0x01, 0x00, None, None, None),
    (0x01, 0x08, 0x01, 0x34, 0x01, 0x04, None, 0x01, 0x00, None, None, None),
    (0x00, 0x09, 0x01, 0x34, 0x01, 0x04, 0x01, 0x01, 0x00, None, None, None),
)


def sum16(data: bytes) -> int:
    return sum(data) & 0xFFFF


def write_be32(image: bytearray, offset: int, value: int) -> None:
    image[offset:offset + 4] = value.to_bytes(4, "big")


def write_be16(image: bytearray, offset: int, value: int) -> None:
    image[offset:offset + 2] = value.to_bytes(2, "big")


def find_nv_descriptor(flash: bytes, key: int, length: int) -> int:
    matches = []
    for offset in range(0, len(flash) - 7, 4):
        # The profile builder consumes the original big-endian .fls image,
        # unlike the swap16 analysis image used by the static tools.
        if int.from_bytes(flash[offset:offset + 4], "big") != key:
            continue
        location_length = int.from_bytes(flash[offset + 4:offset + 8], "big")
        location = location_length >> 16
        if (location_length & 0xFFFF) == length and location + length <= SIZE:
            matches.append(location)
    if len(matches) != 1:
        raise ValueError(
            f"NV descriptor 0x{key:04x}/{length} has {len(matches)} usable matches")
    return matches[0]


@dataclass(frozen=True)
class FallbackLayout:
    constructor: int
    header_source: int
    body_source: int
    location: int


def find_fallback_layout(flash: bytes) -> FallbackLayout:
    """Locate the reviewed 9+391-byte variant-zero constructor, not a ROM name.

    Thumb halfwords and pool words are big-endian in the original .fls.
    LDRB payload bytes use the separate flash byte-lane transform below.
    Unrecognised or ambiguous constructor families are deliberately refused.
    """
    pattern = (
        (0x1c20, 0xffff), (0x4900, 0xff00), (0x2209, 0xffff),
        (0xf000, 0xf800), (0xf800, 0xf800), (0x4900, 0xff00),
        (0x2009, 0xffff), (0x1900, 0xffff), (0x22ff, 0xffff),
        (0x3288, 0xffff), (0xf000, 0xf800), (0xf800, 0xf800),
        (0x2000, 0xffff), (0x9000, 0xffff), (0x1c20, 0xffff),
        (0x4900, 0xff00), (0x4a00, 0xff00), (0x1c2b, 0xffff),
        (0xf000, 0xf800), (0xf800, 0xf800),
    )

    def literal(offset: int, instruction: int) -> int:
        pool = ((offset + 4) & ~3) + (instruction & 255) * 4
        if pool + 4 > len(flash):
            raise ValueError('fallback constructor literal is outside flash')
        return int.from_bytes(flash[pool:pool + 4], 'big')

    matches = []
    anchor = flash.find(bytes.fromhex('2209'))
    while anchor >= 0:
        offset = anchor - 4
        if offset >= 0x56 and not offset & 1 and offset + 40 <= len(flash):
            words = struct.unpack_from('>20H', flash, offset)
            if all(word & mask == value for word, (value, mask) in zip(words, pattern)):
                selector = struct.unpack_from('>4H', flash, offset - 0x3e)
                branch = selector[3]
                displacement = (branch & 255) - (256 if branch & 128 else 0)
                target = offset - 0x38 + 4 + displacement * 2
                prologue = struct.unpack_from('>4H', flash, offset - 0x56)
                if (selector[:3] == (0x2500, 0x1c28, 0x2800) and
                        branch & 0xff00 == 0xd000 and target == offset and
                        prologue == (0xb530, 0xb081, 0x20ff, 0x3091) and
                        literal(offset + 30, words[15]) == FALLBACK_RECORD_KEY):
                    header = literal(offset + 2, words[1])
                    body = literal(offset + 10, words[5])
                    for source, count in ((header, 9), (body, 391)):
                        if (source < FLASH_BASE or
                                any(((source - FLASH_BASE + i) ^ 1) >= len(flash)
                                    for i in range(count))):
                            raise ValueError('fallback payload source is outside flash')
                    matches.append(FallbackLayout(FLASH_BASE + offset - 0x56,
                        header, body, find_nv_descriptor(
                            flash, FALLBACK_RECORD_KEY, FALLBACK_RECORD_LENGTH)))
        anchor = flash.find(bytes.fromhex('2209'), anchor + 1)
    if len(matches) != 1:
        raise ValueError(f'NV fallback constructor has {len(matches)} usable matches')
    return matches[0]


def fallback_record(flash: bytes, layout: FallbackLayout) -> bytes:
    if any(source < FLASH_BASE or
           any(((source - FLASH_BASE + i) ^ 1) >= len(flash) for i in range(count))
           for source, count in ((layout.header_source, 9), (layout.body_source, 391))):
        raise ValueError('fallback payload source is outside flash')
    return bytes(flash[(source - FLASH_BASE + i) ^ 1]
                 for source, count in ((layout.header_source, 9), (layout.body_source, 391))
                 for i in range(count))


def validate_checksums(image: bytes) -> None:
    if len(image) != SIZE:
        raise ValueError('EEPROM profile must contain exactly 16384 bytes')
    tune_security_sum = sum16(image[:TUNE_SECURITY_CHECKSUM_END - 2])
    stored_tune_security = int.from_bytes(
        image[TUNE_SECURITY_CHECKSUM_OFFSET:TUNE_SECURITY_CHECKSUM_END], "big")
    if tune_security_sum != stored_tune_security:
        raise ValueError(
            f"tune/security checksum mismatch: 0x{tune_security_sum:04x} != "
            f"0x{stored_tune_security:08x}")

    config_sum = (sum(image[CONFIG_START:CONFIG_CHECKSUM_OFFSET])
                  - image[0x0154] - image[0x0155]) & 0xFFFF
    stored_config = int.from_bytes(image[CONFIG_CHECKSUM_OFFSET:CONFIG_CHECKSUM_OFFSET + 2], "big")
    if config_sum != stored_config:
        raise ValueError(f"config checksum mismatch: 0x{config_sum:04x} != 0x{stored_config:04x}")


def imei_check_digit(first_fourteen: str) -> str:
    if len(first_fourteen) != 14 or not first_fourteen.isdigit():
        raise ValueError("IMEI identity must contain exactly fourteen digits")
    total = 0
    for index, digit in enumerate(map(int, first_fourteen)):
        value = digit * 2 if index & 1 else digit
        total += value // 10 + value % 10
    return str((-total) % 10)


def provision_security_identity(image: bytearray, first_fourteen: str,
                                security_code: str = "12345") -> None:
    """Create the records consumed by 0x29bb68/0x2ae61a.

    EEPROM 0x000c holds the first fourteen IMEI digits as high-nibble-first
    BCD. Firmware 0x265244 calculates digit fifteen. EEPROM 0x0110 similarly
    stores the five-digit phone security code. Initializer 0x292350 derives the
    eight-byte record at 0x06c8 through 0x2ae4e8 and 0x2ae598.
    """
    if len(security_code) != 5 or not security_code.isdigit():
        raise ValueError("security code must contain exactly five digits")

    identity = first_fourteen + imei_check_digit(first_fourteen)
    identity_bcd = bytes((int(first_fourteen[index]) << 4) | int(first_fourteen[index + 1])
                         for index in range(0, 14, 2)) + bytes(1)
    image[IDENTITY_OFFSET:IDENTITY_OFFSET + 8] = identity_bcd
    provision_security_state(image, identity, security_code)


def provision_security_state(image: bytearray, identity: str,
                             security_code: str = "12345") -> None:
    """Encode the code and verifier record for a formatted 15-character identity."""
    if len(identity) != 15:
        raise ValueError("formatted identity must contain exactly fifteen characters")
    if len(security_code) != 5 or not security_code.isdigit():
        raise ValueError("security code must contain exactly five digits")
    code_bcd = bytes((int(security_code[0:2], 16),
                      int(security_code[2:4], 16),
                      int(security_code[4], 16) << 4))
    image[SECURITY_CODE_OFFSET:SECURITY_CODE_OFFSET + 3] = code_bcd

    packed_code = bytes((5, code_bcd[0], code_bcd[1], code_bcd[2]))
    encrypted = bytes(packed_code[index] ^ ord(identity[11 + index]) ^ SECURITY_XOR[index]
                      for index in range(4))
    identity_sum = sum16(identity.encode("ascii") + bytes((SECURITY_CALLBACK_STATE,)))
    image[SECURITY_STATE_OFFSET:SECURITY_STATE_OFFSET + 8] = (
        encrypted + bytes(2) + identity_sum.to_bytes(2, "big"))


def build_profile(flash: bytes, provisioned_identity: str | None = None,
                  erased_identity_security_code: str | None = None) -> bytearray:
    image = bytearray([0xFF]) * SIZE

    # The constructor and descriptor independently own source and destination.
    # Their addresses move between the reviewed v6.00 and v5.01 images.
    fallback = find_fallback_layout(flash)
    image[fallback.location:fallback.location + FALLBACK_RECORD_LENGTH] = fallback_record(flash, fallback)

    patches = {
        # Firmware default written by the service-session provisioning path at
        # 0x236a78. Startup reloads this four-byte availability record from
        # EEPROM offset 0x0150 through 0x29bb98.
        0x0150: 0x30,
        0x0151: 0x00,
        0x0152: 0x80,
        0x0153: 0x90,
        0x0170: 0x01,
        0x0171: 0x00,
        0x048C: 0x0A,
        0x048D: 0x00,
        0x048E: 0x0A,
        0x048F: 0x80,
        0x0394: 0x0A,
        0x0395: 0x00,
        0x0396: 0x0A,
        0x0397: 0x80,
        0x0398: 0x09,
        0x0399: 0x00,
        0x039A: 0x00,
        0x039B: 0x00,
    }
    for address, value in patches.items():
        image[address] = value

    # Descriptor 0x0749 owns three display-profile records. Both supported ROMs
    # contain equivalent reset constructors for the fields below. Provision
    # those ROM-authored defaults, while retaining erased bytes where the
    # constructor itself makes no assignment. Firmware consumes the records
    # through normal I2C/NV loading; no firmware RAM or LCD state is overridden.
    display_profile = find_nv_descriptor(
        flash, DISPLAY_PROFILE_KEY, DISPLAY_PROFILE_LENGTH)
    if display_profile + len(DISPLAY_PROFILE_DEFAULTS) * DISPLAY_PROFILE_LENGTH > SIZE:
        raise ValueError('display profile variants extend outside EEPROM')
    for variant, record in enumerate(DISPLAY_PROFILE_DEFAULTS):
        record_offset = display_profile + variant * DISPLAY_PROFILE_LENGTH
        for field, value in enumerate(record):
            if value is not None:
                image[record_offset + field] = value

    # Descriptor 0x074c: per-game top score and level. The ROM carries no reset
    # constructor for these records, so provision a zero top score and level
    # index 0 (the slowest speed) and leave the unconsumed byte erased.
    games_record = find_nv_descriptor(flash, GAMES_RECORD_KEY, GAMES_RECORD_LENGTH)
    # The adjacent 0x0757/400 record starts at 0x0db0 in v6.00, but
    # 0x0da8 in v5.01. Their loaders independently confirm five and three
    # iterations respectively; never write game defaults into that record.
    games_end = find_nv_descriptor(flash, 0x0757, 400)
    games_bytes = games_end - games_record
    if (games_bytes <= 0 or games_bytes % GAMES_RECORD_LENGTH or
            games_bytes > GAMES_RECORD_VARIANTS * GAMES_RECORD_LENGTH):
        raise ValueError("unsupported games NV record extent")
    for variant in range(games_bytes // GAMES_RECORD_LENGTH):
        record_offset = games_record + variant * GAMES_RECORD_LENGTH
        for field, value in enumerate(GAMES_RECORD_DEFAULT):
            if value is not None:
                image[record_offset + field] = value

    if provisioned_identity is not None and erased_identity_security_code is not None:
        raise ValueError("choose either a provisioned identity or erased-identity security code")
    if provisioned_identity is not None:
        provision_security_identity(image, provisioned_identity)
    elif erased_identity_security_code is not None:
        # The unprovisioned ROM formats its erased identity as fifteen question
        # marks. Supplying the matching verifier state lets a physical 12345
        # entry leave that editor without inventing an IMEI.
        provision_security_state(
            image, "?" * 15, erased_identity_security_code)

    # Firmware 0x264c56 reads 0x120 bytes, sums the first 0x11e bytes, and
    # compares the result with the big-endian 32-bit word at 0x011c. The two
    # bytes of overlap are zero, so writing the 16-bit sum as a 32-bit value
    # leaves the summed data unchanged.
    image[TUNE_SECURITY_CHECKSUM_OFFSET:TUNE_SECURITY_CHECKSUM_END] = bytes(4)
    write_be32(image, TUNE_SECURITY_CHECKSUM_OFFSET,
               sum16(image[:TUNE_SECURITY_CHECKSUM_END - 2]))

    # The contact/config block checksum excludes the two correction bytes at
    # 0x0154..0x0155 even though they reside inside the summed range.
    contact_sum = (sum(image[CONFIG_START:CONFIG_CHECKSUM_OFFSET])
                   - image[0x0154] - image[0x0155]) & 0xFFFF
    write_be16(image, CONFIG_CHECKSUM_OFFSET, contact_sum)

    for start, end in ((0x02E0, 0x02EC), (0x0310, 0x0314), (0x0330, 0x0338)):
        image[start:end] = bytes(end - start)

    validate_checksums(image)
    return image


def main() -> None:
    parser = argparse.ArgumentParser()
    parser.add_argument("--flash", type=Path, required=True)
    parser.add_argument("--output", type=Path, required=True)
    parser.add_argument("--provisioned-imei-prefix", metavar="DIGITS",
                        help="provision a synthetic 14-digit IMEI prefix and matching security record")
    parser.add_argument("--erased-identity-security-code", metavar="DIGITS",
                        help="provision a code/verifier while leaving the IMEI erased")
    args = parser.parse_args()

    profile = build_profile(
        args.flash.read_bytes(),
        args.provisioned_imei_prefix,
        args.erased_identity_security_code)
    args.output.parent.mkdir(parents=True, exist_ok=True)
    args.output.write_bytes(profile)
    print(f"wrote {args.output} ({len(profile)} bytes)")


if __name__ == "__main__":
    main()
