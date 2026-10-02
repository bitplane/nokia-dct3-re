#!/usr/bin/env python3
"""Read-only replay of the observed NSE-5 PMM startup write journal."""

import argparse
import hashlib
import json
from pathlib import Path
import re


def replay(image, cache_size=0x898):
    if len(image) < 0x2000 or image[6:12] != b"EEPROM":
        raise ValueError("expected a complete NSE-5 EEPROM sector")
    if int.from_bytes(image[0x18:0x1A], "big") != 1:
        raise ValueError("sector is not accepted by the startup reader")
    cache = bytearray(cache_size)
    writes = []
    cursor = 0x20
    while cursor <= 0x1FFF:
        start = cursor
        header = int.from_bytes(image[cursor:cursor + 2], "big")
        if header == 0xFFFF or header & 0x200:
            return bytes(cache), writes, cursor
        cursor += 2
        length = header >> 10
        if not length:
            length = int.from_bytes(image[cursor:cursor + 2], "big")
            cursor += 2
        # The acquired sector has no deletion records. Do not guess their
        # cache effects from the observed write-only path.
        if header & 0x100:
            raise ValueError("deletion record requires a separate contract")
        destination = int.from_bytes(image[cursor:cursor + 2], "big")
        cursor += 2
        if not length or cursor + length > 0x2000 or destination + length > cache_size:
            raise ValueError("journal write exceeds the observed sector/cache")
        cache[destination:destination + length] = image[cursor:cursor + length]
        writes.append({"record": start, "source": cursor,
                       "destination": destination, "length": length})
        cursor += (length + 1) & ~1
    raise ValueError("journal has no in-sector termination")


def check_trace(cache, text):
    snapshots = re.findall(r"nse5_compat_storage_cache_snapshot: bytes=([0-9a-f]+)", text)
    if not snapshots:
        raise ValueError("no loader-completion cache snapshot")
    for value in snapshots:
        if bytes.fromhex(value) != cache:
            raise ValueError("loader-completion cache differs from independent replay")
    return len(snapshots)


def main():
    parser = argparse.ArgumentParser(description=__doc__)
    parser.add_argument("pmm", type=Path)
    parser.add_argument("--trace", type=Path, help="verify loader-completion snapshots")
    args = parser.parse_args()
    try:
        image = args.pmm.read_bytes()
        cache, writes, end = replay(image)
        snapshots = check_trace(cache, args.trace.read_text()) if args.trace else None
    except (OSError, ValueError) as exc:
        parser.exit(1, f"NSE-5 PMM journal failed: {exc}\n")
    print(json.dumps({"pmm_sha256": hashlib.sha256(image).hexdigest(),
                      "writes": len(writes), "stop_offset": f"{end:x}",
                      "matching_snapshots": snapshots,
                      "cache_sha256": hashlib.sha256(cache).hexdigest(),
                      "command_region_matches_first_record": cache[:0x38] == image[0x26:0x5E],
                      "later_command_region_writes": [row for row in writes[1:]
                                                       if row["destination"] < 0x38]}, indent=2))


if __name__ == "__main__":
    main()
