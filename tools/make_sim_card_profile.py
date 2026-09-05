#!/usr/bin/env python3
"""Create deterministic persistent storage for the laboratory GSM SIM."""

import argparse
from pathlib import Path


ADN_SIZE = 50 * 32
LOCI_SIZE = 11
KC_SIZE = 9
BCCH_SIZE = 16
SMS_RECORD_SIZE = 176
SMS_RECORDS = 10
SMSP_RECORD_SIZE = 44
SMSP_RECORDS = 2

CHV1 = b"1234\xff\xff\xff\xff"
CHV2 = b"5678\xff\xff\xff\xff"
PUK1 = b"12345678"
PUK2 = b"87654321"


def make_profile(pin_enabled: bool) -> bytes:
    image = bytearray()
    image.extend(b"\xff" * ADN_SIZE)

    loci = bytearray(b"\xff" * LOCI_SIZE)
    loci[-1] = 0x01
    image.extend(loci)

    kc = bytearray(b"\xff" * KC_SIZE)
    kc[-1] = 0x07
    image.extend(kc)
    image.extend(b"\xff" * BCCH_SIZE)

    for _ in range(SMS_RECORDS):
        image.extend(b"\x00" + b"\xff" * (SMS_RECORD_SIZE - 1))

    smsp = bytearray(b"\xff" * (SMSP_RECORD_SIZE * SMSP_RECORDS))
    smsp[16] = 0xFD
    smsp[29:36] = bytes((0x06, 0x91, 0x21, 0x43, 0x65, 0x87, 0x09))
    image.extend(smsp)

    image.extend(CHV1 + CHV2 + PUK1 + PUK2)
    image.extend(bytes((3, 3, 10, 10, int(pin_enabled))))
    image.extend(b"\x00\x00\x00")  # EF_ACM
    return bytes(image)


def main() -> int:
    parser = argparse.ArgumentParser()
    parser.add_argument("--output", required=True, type=Path)
    parser.add_argument("--pin-enabled", action="store_true")
    args = parser.parse_args()

    data = make_profile(args.pin_enabled)
    args.output.parent.mkdir(parents=True, exist_ok=True)
    args.output.write_bytes(data)
    print(f"wrote {args.output} ({len(data)} bytes, PIN {'enabled' if args.pin_enabled else 'disabled'})")
    return 0


if __name__ == "__main__":
    raise SystemExit(main())
