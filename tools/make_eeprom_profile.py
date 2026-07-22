#!/usr/bin/env python3
"""Build the documented synthetic 3210 24C128 self-test profile."""

import argparse
from pathlib import Path


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
# v6.00 0x29a802 and its v5.01 equivalent 0x296d6e construct these
# three reset profiles before committing descriptor 0x0749 variants 0..2.
# None marks bytes the constructor does not write; retain their erased state
# rather than presenting zero-filled scratch RAM as authored NV data.
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


def validate_checksums(image: bytes) -> None:
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
    code_bcd = bytes((int(security_code[0:2], 16),
                      int(security_code[2:4], 16),
                      int(security_code[4], 16) << 4))
    image[IDENTITY_OFFSET:IDENTITY_OFFSET + 8] = identity_bcd
    image[SECURITY_CODE_OFFSET:SECURITY_CODE_OFFSET + 3] = code_bcd

    packed_code = bytes((5, code_bcd[0], code_bcd[1], code_bcd[2]))
    encrypted = bytes(packed_code[index] ^ ord(identity[11 + index]) ^ SECURITY_XOR[index]
                      for index in range(4))
    identity_sum = sum16(identity.encode("ascii") + bytes((SECURITY_CALLBACK_STATE,)))
    image[SECURITY_STATE_OFFSET:SECURITY_STATE_OFFSET + 8] = (
        encrypted + bytes(2) + identity_sum.to_bytes(2, "big"))


def build_profile(flash: bytes, provisioned_identity: str | None = None) -> bytearray:
    image = bytearray([0xFF]) * SIZE

    # Firmware fallback record for NV descriptor 0x0757, variant zero.
    for index in range(0x190):
        source = 0x2D7FDC + index if index < 9 else 0x2D7C08 + index - 9
        raw = (source - FLASH_BASE) ^ 1
        if raw >= len(flash):
            raise ValueError(f"flash is too short for source address 0x{source:08x}")
        image[0x0DB0 + index] = flash[raw]

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
    for variant, record in enumerate(DISPLAY_PROFILE_DEFAULTS):
        record_offset = display_profile + variant * DISPLAY_PROFILE_LENGTH
        for field, value in enumerate(record):
            if value is not None:
                image[record_offset + field] = value

    if provisioned_identity is not None:
        provision_security_identity(image, provisioned_identity)

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
    args = parser.parse_args()

    profile = build_profile(args.flash.read_bytes(), args.provisioned_imei_prefix)
    args.output.parent.mkdir(parents=True, exist_ok=True)
    args.output.write_bytes(profile)
    print(f"wrote {args.output} ({len(profile)} bytes)")


if __name__ == "__main__":
    main()
