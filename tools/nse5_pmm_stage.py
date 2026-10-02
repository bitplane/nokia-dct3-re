#!/usr/bin/env python3
"""Read-only reproduction of NSE-5 MCU packet preparation at 0x3ae364."""

import argparse
import json
from pathlib import Path


def stage_block(storage: bytes, block: bytes) -> bytes:
    if len(storage) != 12 or len(block) != 24:
        raise ValueError("packet preparation requires 12 storage bytes and 24 block bytes")
    products = b"".join(
        (storage[i] * storage[i + 1]).to_bytes(2, "little")
        for i in range(0, 12, 2)
    ) * 2
    # Firmware reverses byte order and complements each bit-reversed byte.
    mask = bytes(int(f"{byte:08b}"[::-1], 2) ^ 0xff for byte in products[::-1])
    return bytes(byte ^ key for byte, key in zip(block, mask))


def inspect_pmm(image: bytes) -> dict[str, object]:
    if len(image) < 0x5e or image[6:12] != b"EEPROM":
        raise ValueError("expected acquired NSE-5 EEPROM flash record header")
    storage, block = image[0x26:0x32], image[0x46:0x5e]
    return {"storage": storage.hex(), "block": block.hex(),
            "staged": stage_block(storage, block).hex(),
            "startup_requests": {
                "14": image[0x3a:0x46].hex(),
                "15": (image[0x26:0x32] + image[0x32:0x3a]).hex(),
                "16": stage_block(storage, block).hex(),
            }}


def main() -> None:
    parser = argparse.ArgumentParser(description=__doc__)
    parser.add_argument("pmm", type=Path)
    args = parser.parse_args()
    try:
        result = inspect_pmm(args.pmm.read_bytes())
    except (OSError, ValueError) as error:
        parser.exit(1, f"NSE-5 packet preparation: {error}\n")
    print(json.dumps(result, indent=2))


if __name__ == "__main__":
    main()
