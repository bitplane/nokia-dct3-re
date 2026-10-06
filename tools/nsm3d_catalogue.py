"""Inventory acquired NSM-3D DSP uploads; do not infer fitted mask contents."""

import argparse
import hashlib
import json
from pathlib import Path
import struct

from tools.extract_nsm3_verifier import NSM3D_FLASH_SHA1


def catalogue(image, product="8250"):
    profiles = {
        "8250": (NSM3D_FLASH_SHA1, 0x109560, 0x12f040),
        "8890": ("a214a0d69760ecd8eeca0b9d82f95c94bdfe70ed", 0x107608, 0x134c78),
        "8210": ("c1a0fe95cedb89a92b19654208cc4855e1a4988e", 0x10cf50, 0x13579c),
        "8210-rom5": ("c1a0fe95cedb89a92b19654208cc4855e1a4988e", 0x10ced4, 0x135810),
    }
    if product not in profiles:
        raise ValueError("unsupported catalogue product")
    digest, initialization, expected_destination = profiles[product]
    if hashlib.sha1(image).hexdigest() != digest:
        raise ValueError(f"not the pinned {product} flash")
    size, destination = struct.unpack_from(">2I", image, initialization)
    if (size, destination) != (0x74, expected_destination):
        raise ValueError("unexpected catalogue initialization record")
    pointers = struct.unpack_from(">29I", image, initialization + 8)
    if pointers[-1] != 0 or any(not pointer for pointer in pointers[:-1]):
        raise ValueError("unexpected catalogue terminator")
    entries = []
    for selector, pointer in enumerate(pointers[:-1]):
        offset = pointer - 0x200000
        if offset < 0 or offset + 12 > len(image):
            raise ValueError("descriptor outside acquired flash")
        header = struct.unpack_from(">6H", image, offset)
        start = offset + 12
        end = start + header[2] * 2
        if end > len(image) or header[0] + header[2] > 0x10000:
            raise ValueError("descriptor payload or destination exceeds extent")
        entries.append({
            "selector": selector, "descriptor": pointer,
            "header": list(header), "payload_offset": start,
            "words": header[2], "sha1": hashlib.sha1(image[start:end]).hexdigest(),
            "declared_destination": [header[0], header[0] + header[2]],
        })
    return entries


def covering(entries, address):
    # This is only the descriptor's declared destination, not proof of a
    # program-space installation or absence of dynamically relocated code.
    return [entry["selector"] for entry in entries
            if entry["declared_destination"][0] <= address < entry["declared_destination"][1]]


def main():
    parser = argparse.ArgumentParser(description=__doc__)
    parser.add_argument("flash", type=Path)
    parser.add_argument("--product", choices=("8250", "8890", "8210", "8210-rom5"), default="8250")
    parser.add_argument("--address", type=lambda value: int(value, 0), action="append", default=[])
    args = parser.parse_args()
    try:
        entries = catalogue(args.flash.read_bytes(), args.product)
    except (OSError, ValueError, struct.error) as error:
        parser.exit(1, f"NSM-3D catalogue failed: {error}\n")
    print(json.dumps({"entries": entries, "coverage": {
        f"{address:04x}": covering(entries, address) for address in args.address},
        "scope": "All 28 initialized descriptors; declared destinations only. Relocation and fitted mask contents are not proved."}, indent=2))


if __name__ == "__main__":
    main()
