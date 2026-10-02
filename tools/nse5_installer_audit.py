#!/usr/bin/env python3
"""Read-only comparison of NSE-5 installer members and acquired images."""

import argparse
import hashlib
import json
from pathlib import Path
import zlib


MEMBERS = {"mcu": 0x834A7, "basic": 0x55DA10, "ppm_c": 0x2C9814}
INSTALLER_SHA256 = "d7f4430f75efbdb19f11eb9f4555f8fc8d66edc00dbbdcf52e6a6616ee8bfd50"


def member(source, offset, limit=16 * 1024 * 1024):
    decoder = zlib.decompressobj(31)
    data = decoder.decompress(source[offset:], limit + 1)
    if len(data) > limit or not decoder.eof:
        raise ValueError("gzip member is truncated or exceeds size bound")
    return data


def records(source):
    result = []
    offset = 0
    previous_end = None
    while offset < len(source):
        header = source[offset:offset + 9]
        if len(header) != 9:
            raise ValueError("truncated record header")
        address = int.from_bytes(header[1:4], "big")
        length = int.from_bytes(header[5:8], "big")
        if header[0] != 0x0B or not 0 < length <= 0x2000:
            raise ValueError("unsupported record type or length")
        end = offset + 9 + length
        if end > len(source):
            raise ValueError("truncated record payload")
        if previous_end is not None and address < previous_end:
            raise ValueError("overlapping or unordered records")
        result.append((address, source[offset + 9:end]))
        previous_end = address + length
        offset = end
    if not result:
        raise ValueError("empty record stream")
    return result


def compare(items, flash, pmm):
    rows = []
    for address, payload in items:
        reference = None
        if 0x200000 <= address and address + len(payload) <= 0x200000 + len(flash):
            reference = flash[address - 0x200000:address - 0x200000 + len(payload)]
        elif 0x5FA000 <= address and address + len(payload) <= 0x5FA000 + len(pmm):
            reference = pmm[address - 0x5FA000:address - 0x5FA000 + len(payload)]
        rows.append({"address": f"{address:06x}", "length": len(payload),
                     "sha1": hashlib.sha1(payload).hexdigest(),
                     "different_bytes": None if reference is None else
                     sum(a != b for a, b in zip(payload, reference))})
    return rows


def main():
    parser = argparse.ArgumentParser(description=__doc__)
    parser.add_argument("installer", type=Path)
    parser.add_argument("flash", type=Path)
    parser.add_argument("pmm", type=Path)
    parser.add_argument("--records", action="store_true", help="include every compared record")
    args = parser.parse_args()
    try:
        source, flash, pmm = (path.read_bytes() for path in
                             (args.installer, args.flash, args.pmm))
        if hashlib.sha256(source).hexdigest() != INSTALLER_SHA256:
            raise ValueError("installer hash differs from the mapped v5.01 package")
        if len(flash) != 0x390000 or len(pmm) != 0x6000:
            raise ValueError("expected a 0x390000 flash and 0x6000 PMM image")
        report = {"installer_sha256": hashlib.sha256(source).hexdigest(),
                  "members": {}}
        for name, offset in MEMBERS.items():
            items = records(member(source, offset))
            rows = compare(items, flash, pmm)
            report["members"][name] = {
                "offset": f"{offset:x}", "records": len(items),
                "gaps": sum(a + len(data) != b for (a, data), (b, _) in
                            zip(items, items[1:])),
                "equal_records": sum(row["different_bytes"] == 0 for row in rows),
                "comparison": rows if args.records else
                [row for row in rows if row["different_bytes"] != 0]}
    except (OSError, ValueError, zlib.error) as exc:
        parser.exit(1, f"NSE-5 installer audit failed: {exc}\n")
    print(json.dumps(report, indent=2))


if __name__ == "__main__":
    main()
