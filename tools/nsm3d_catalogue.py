"""Inventory acquired NSM-3D DSP uploads; do not infer fitted mask contents."""

import argparse
import hashlib
import json
from pathlib import Path
import struct

from tools.extract_nsm3_verifier import NSM3D_FLASH_SHA1


def catalogue(image):
    if hashlib.sha1(image).hexdigest() != NSM3D_FLASH_SHA1:
        raise ValueError("not the pinned 8250 flash")
    size, destination = struct.unpack_from(">2I", image, 0x109560)
    if (size, destination) != (0x74, 0x12f040):
        raise ValueError("unexpected catalogue initialization record")
    pointers = struct.unpack_from(">29I", image, 0x109568)
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
    parser.add_argument("--address", type=lambda value: int(value, 0), action="append", default=[])
    args = parser.parse_args()
    try:
        entries = catalogue(args.flash.read_bytes())
    except (OSError, ValueError, struct.error) as error:
        parser.exit(1, f"NSM-3D catalogue failed: {error}\n")
    print(json.dumps({"entries": entries, "coverage": {
        f"{address:04x}": covering(entries, address) for address in args.address},
        "scope": "All 28 initialized descriptors; declared destinations only. Relocation and fitted mask contents are not proved."}, indent=2))


if __name__ == "__main__":
    main()
