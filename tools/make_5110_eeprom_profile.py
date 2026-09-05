#!/usr/bin/env python3
"""Provision the NSE-1 v5.30 factory-identity records in a 24C16 image."""

import argparse
from pathlib import Path


SIZE = 0x800
IDENTITY_OFFSET = 0x00C
FAID_RECORD_704 = 0x304
FAID_RECORD_705 = 0x334
FAID_RECORD_706 = 0x32C
FAID_RECORD_707 = 0x335
RECORD_CHECKSUM_OFFSET = 0x03E
TUNE_START = 0x040
TUNE_END = 0x11E
TUNE_CHECKSUM_OFFSET = 0x11E
TUNE_CORRECTION = (0x074, 0x075)
DSP_FAULT_LATCH = 0x29E
SECURITY_LEVEL = 0x2C7
FACTORY_KEY_FLASH_OFFSET = 0xAADB4
MSID = bytes.fromhex("8264b000eb8f457e168bd2d32a")
IMEI14 = "49054410901987"

ENCOD = bytes.fromhex("b173e65aab478e0d1a34680b")
DECOD = bytes.fromhex("d0162c58b071e2d55a67ce8d")
MSID_DEC_82 = bytes.fromhex("9f7aad34e77927734e410d26")
FLASH_ENC_82 = bytes.fromhex("4fb96c2702dd77dd06e3e2df")
LOCK_ENC_82 = bytes.fromhex("d1a25feb4ff02d7b0f4ce1c3")
IMEI_ENC_82 = bytes.fromhex("0baefc8cc7fc792ac194c237")
PERM = (
    (1, 4, 8, 9, 11), (5, 6, 10, 8, 9), (3, 1, 6, 10, 11),
    (0, 7, 8, 10, 11), (0, 1, 3, 5, 8), (2, 0, 1, 9, 10),
    (2, 3, 5, 7, 10), (0, 2, 3, 4, 11), (0, 4, 5, 7, 9),
    (1, 2, 4, 5, 6), (2, 6, 7, 9, 11), (3, 4, 6, 7, 8),
)


def sum16(data: bytes) -> int:
    return sum(data) & 0xFFFF


def _bit_reverse(value: int) -> int:
    return int(f"{value:08b}"[::-1], 2)


def _reverse_buffer(data: bytearray) -> None:
    data[:] = bytes(_bit_reverse(value) for value in reversed(data))


def _rotate_right32(data: bytearray, offset: int, count: int) -> None:
    value = int.from_bytes(data[offset:offset + 4], "big")
    count %= 32
    value = ((value >> count) | (value << (32 - count))) & 0xFFFFFFFF
    data[offset:offset + 4] = value.to_bytes(4, "big")


def _round_codec(source: bytes, table: bytes, schedule: bytes) -> bytes:
    data = bytearray(source)

    def mix(round_index: int) -> None:
        for index in range(12):
            data[index] ^= table[index]
        for index in (2, 3, 8, 9):
            data[index] ^= schedule[round_index]

        work = bytearray(12)
        seen = [False] * 12
        parity = 0
        for source_index, destinations in enumerate(PERM):
            value = data[source_index]
            parity ^= value
            for destination in destinations:
                work[destination] = (work[destination] ^ value) if seen[destination] else value
                seen[destination] = True
        for index in range(12):
            data[index] = work[index] ^ parity

    for round_index in range(11):
        mix(round_index)
        _rotate_right32(data, 0, 10)
        _rotate_right32(data, 8, 31)
        old = bytes(data)
        for index in range(12):
            data[index] = old[index] ^ (old[(index + 4) % 12] | (~old[(index + 8) % 12] & 0xFF))
        _rotate_right32(data, 8, 10)
        _rotate_right32(data, 0, 31)
    mix(11)
    _reverse_buffer(data)
    return bytes(data)


def decode_msid(msid: bytes) -> tuple[bytes, bytes, bytes]:
    if len(msid) != 13 or msid[0] != 0x82:
        raise ValueError("NSE-1 profile requires an algorithm-0x82 MSID")
    decoded = _round_codec(msid[1:], MSID_DEC_82, DECOD)
    return decoded[:4], decoded[4:8], decoded[8:]


def encode_record(plain: bytes, table: bytes, cobba: bytes) -> bytes:
    keyed = bytes(value ^ (cobba[index] if index < 4 else 0)
                  for index, value in enumerate(table))
    return _round_codec(plain, keyed, ENCOD)


def _imei_record(imei14: str, cobba: bytes) -> bytes:
    packed = bytes((int(imei14[index]) << 4) | int(imei14[index + 1])
                   for index in range(0, 14, 2))
    plain = bytearray.fromhex("792900000000000000ff0000")
    plain[2:9] = packed
    return encode_record(plain, IMEI_ENC_82, cobba)


def _simlock_pad(imei_record: bytes) -> bytes:
    work = bytearray(imei_record * 2)
    for index in range(12):
        product = work[index * 2] * work[index * 2 + 1]
        work[index * 2] = product & 0xFF
        work[index * 2 + 1] = product >> 8
    return bytes(_bit_reverse((~work[23 - index]) & 0xFF) for index in range(12))


def provision_low_records(image: bytearray) -> None:
    flash_crc, cobba, identity_hash = decode_msid(MSID)
    imei_record = _imei_record(IMEI14, cobba)
    image[0x00:0x0C] = imei_record

    flash_plain = bytearray(flash_crc + cobba + identity_hash)
    mask = bytes.fromhex("17ca6089")
    for index in range(4):
        flash_plain[index] ^= mask[index]
        flash_plain[4 + index] = flash_plain[index] ^ 0xFF
    flash_plain[8:12] = bytes(4)
    image[0x14:0x20] = encode_record(flash_plain, FLASH_ENC_82, cobba)

    pad = _simlock_pad(imei_record)
    part1 = bytes.fromhex("ffffffffff0f0000007854c2")
    part2 = bytes.fromhex("0000000000000000087c54c2")
    encoded1 = encode_record(part1, LOCK_ENC_82, cobba)
    encoded2 = encode_record(part2, LOCK_ENC_82, cobba)
    image[0x20:0x2C] = bytes(a ^ b for a, b in zip(encoded1, pad))
    image[0x2C:0x38] = bytes(a ^ b for a, b in zip(encoded2, pad))


def build_profile(eeprom: bytes, flash: bytes) -> bytearray:
    if len(eeprom) != SIZE:
        raise ValueError(f"NSE-1 EEPROM must be {SIZE} bytes")
    if len(flash) < FACTORY_KEY_FLASH_OFFSET + 8:
        raise ValueError("NSE-1 flash is too short for the factory key")

    image = bytearray(eeprom)
    provision_low_records(image)
    identity = image[IDENTITY_OFFSET:IDENTITY_OFFSET + 8]
    # Selector 3 at 0x258798 expands the serial portion of this specific
    # factory identity to the eight bytes consumed by derive routine 0x25f130.
    serial = b"9019871\0"
    key = flash[FACTORY_KEY_FLASH_OFFSET:FACTORY_KEY_FLASH_OFFSET + 8]
    derived = bytes(a ^ b ^ c for a, b, c in zip(identity, serial, key))

    image[FAID_RECORD_704:FAID_RECORD_704 + 8] = derived
    image[FAID_RECORD_706:FAID_RECORD_706 + 8] = derived
    image[FAID_RECORD_705] = 1
    image[FAID_RECORD_707] = 1

    image[DSP_FAULT_LATCH] = 0
    image[SECURITY_LEVEL] = 0xFF

    tune_sum = (sum(image[TUNE_START:TUNE_END])
                - image[TUNE_CORRECTION[0]]
                - image[TUNE_CORRECTION[1]]) & 0xFFFF
    image[TUNE_CHECKSUM_OFFSET:TUNE_CHECKSUM_OFFSET + 2] = tune_sum.to_bytes(2, "big")

    record_sum = sum16(image[:RECORD_CHECKSUM_OFFSET])
    image[RECORD_CHECKSUM_OFFSET:RECORD_CHECKSUM_OFFSET + 2] = record_sum.to_bytes(2, "big")
    return image


def main() -> None:
    parser = argparse.ArgumentParser()
    parser.add_argument("--eeprom", type=Path, required=True)
    parser.add_argument("--flash", type=Path, required=True)
    parser.add_argument("--output", type=Path, required=True)
    args = parser.parse_args()

    profile = build_profile(args.eeprom.read_bytes(), args.flash.read_bytes())
    args.output.parent.mkdir(parents=True, exist_ok=True)
    args.output.write_bytes(profile)
    print(f"wrote {args.output} ({len(profile)} bytes)")


if __name__ == "__main__":
    main()
