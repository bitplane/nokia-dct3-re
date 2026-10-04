#!/usr/bin/env python3
"""NHM-3 checksum comparison and explicit initial-record provisioning fixture."""

import argparse
import hashlib
import json
from pathlib import Path
import re
import sys

try:
    from tools.nse5_pmm_journal import replay
except ModuleNotFoundError:
    from nse5_pmm_journal import replay


def checksum(cache):
    if len(cache) < 0x256:
        raise ValueError("NV image does not cover the checksum contract")
    return (sum(cache[0x120:0x254]) - sum(cache[0x154:0x156])) & 0xffff


def record_checksum(destination, data):
    if not 0 <= destination <= 0xffff:
        raise ValueError("record destination must fit a word")
    return ((destination >> 8) + (destination & 0xff) + sum(data)) & 0xff


def assess(image, trace=None):
    cache, records, stop = replay(image, 0xa28)
    first = records[0] if records else None
    if not first or first["destination"] != 0 or first["length"] != 0xa28:
        raise ValueError("expected NHM-3 complete initial cache record")
    base = image[first["source"]:first["source"] + first["length"]]
    if trace is not None:
        snapshots = re.findall(r"6250_nv_sum_shadow: bytes=([0-9a-f]+)", trace)
        if not snapshots or any(bytes.fromhex(x) != cache[0x120:0x256] for x in snapshots):
            raise ValueError("no matching runtime checksum-range shadow")
    return {
        "records": len(records), "stop_offset": f"{stop:04x}",
        "sector_inventory": [
            {"offset": f"{offset:04x}",
             "erased": all(value == 0xff for value in image[offset:offset + 0x2000]),
             "eeprom_signature": image[offset + 6:offset + 12] == b"EEPROM",
             "reader_state_word": image[offset + 0x18:offset + 0x1a].hex()}
            for offset in range(0, len(image) - 0x1fff, 0x2000)],
        "base_computed": f"{checksum(base):04x}",
        "base_stored": base[0x254:0x256].hex(),
        "replayed_computed": f"{checksum(cache):04x}",
        "replayed_stored": cache[0x254:0x256].hex(),
        "checksum_region_updates": [row for row in records[1:]
                                    if row["destination"] < 0x256 and
                                    row["destination"] + row["length"] > 0x120],
        "record_checksum_mismatches": [
            {"record": row["record"], "stored": image[row["record"] + 1],
             "computed": record_checksum(row["destination"],
                                         image[row["source"]:row["source"] + row["length"]])}
            for row in records if image[row["record"] + 1] != record_checksum(
                row["destination"], image[row["source"]:row["source"] + row["length"]])],
        "scope": "write-journal replay; NHM-3 reader does not enforce record checksum bytes",
    }


def initial_record_fixture(image):
    assess(image)
    _, records, _ = replay(image, 0xa28)
    first = records[0]
    end = first["source"] + first["length"]
    base = image[first["source"]:end]
    if checksum(base) != int.from_bytes(base[0x254:0x256], "big"):
        raise ValueError("initial record has no valid application checksum to preserve")
    result = bytearray(image)
    result[first["record"] + 1] = record_checksum(0, base)
    # Retain the initial payload exactly; discard this sector's later history.
    result[end:0x2000] = b"\xff" * (0x2000 - end)
    return bytes(result)


def main():
    parser = argparse.ArgumentParser(description=__doc__)
    parser.add_argument("pmm", type=Path)
    parser.add_argument("--trace", type=Path)
    parser.add_argument("--initial-record-fixture", type=Path,
                        help="write a separate derived fixture, never overwrite the acquired PMM")
    args = parser.parse_args()
    try:
        image = args.pmm.read_bytes()
        result = assess(image, args.trace.read_text() if args.trace else None)
        if args.initial_record_fixture:
            if args.initial_record_fixture.resolve() == args.pmm.resolve():
                raise ValueError("fixture output must not overwrite its source")
            fixture = initial_record_fixture(image)
            with args.initial_record_fixture.open("xb") as output:
                output.write(fixture)
            result["fixture"] = {"source_sha256": hashlib.sha256(image).hexdigest(),
                                 "fixture_sha256": hashlib.sha256(fixture).hexdigest(),
                                 "policy": "initial acquired payload only; record checksum recomputed"}
    except (OSError, ValueError) as exc:
        print(f"6250 PMM comparison failed: {exc}", file=sys.stderr)
        return 1
    print(json.dumps(result, indent=2))
    return 0


if __name__ == "__main__":
    raise SystemExit(main())
