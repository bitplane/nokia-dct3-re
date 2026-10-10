#!/usr/bin/env python3
"""Check NSE-6 uploaded-code visibility and sampled stack/ring integrity, not boot parity."""

import argparse
from pathlib import Path
import re
import sys


UPLOAD = re.compile(r"nse6_native_context_upload: address=([0-9a-f]+) value=([0-9a-f]+) mask=ffff .*? t=([0-9.]+)")
TARGET = re.compile(r"nse6_native_context_target: address=2946 pmst=([0-9a-f]+) program=([0-9a-f,]+) data=([0-9a-f,]+) t=([0-9.]+)")
SNAPSHOT = re.compile(r"nse6_native_snapshot: pc=[0-9a-f]+ sp=([0-9a-f]+) .*? t=([0-9.]+)")
DEPTH = re.compile(r"nse6_native_stack_depth: .*? sp=([0-9a-f]+) ")
INDEX = re.compile(r"nse6_native_rx_index_write: address=([0-9a-f]+) value=([0-9a-f]+) .*? t=([0-9.]+)")
MEMORY_READBACK = re.compile(r"nse6_native_memory_readback: address=([0-9a-f]+) matched=(\d+) words=(\d+) consumed=([0-9.]+) t=([0-9.]+)")


def check_memory_uploads(text):
    """Check the four measured fragments; this does not establish their use."""
    records = MEMORY_READBACK.findall(text)
    if len(records) < 4:
        raise ValueError("missing native memory-upload readbacks")
    for record, address in zip(records[:4], (0x2286, 0x22ad, 0x22d4, 0x22fb)):
        actual, matched, words, consumed, observed = record
        if int(actual, 16) != address or int(words) != 39 or int(matched) != 39:
            raise ValueError("native memory-upload span or readback mismatch")
        if float(observed) <= float(consumed):
            raise ValueError("memory readback does not follow consumption")
    return 156


def check(text):
    if "unimplemented C54x opcode" in text:
        raise ValueError("native execution stopped at an unsupported opcode")
    uploads = {}
    for address, value, timestamp in UPLOAD.findall(text):
        uploads.setdefault(int(address, 16), (int(value, 16), float(timestamp)))
    addresses = range(0x2946, 0x295e)
    if any(address not in uploads for address in addresses):
        raise ValueError("missing contiguous context upload observations")
    targets = TARGET.findall(text)
    if not targets:
        raise ValueError("missing executed context-target readback")
    pmst, program, data, timestamp = targets[0]
    expected = [uploads[address][0] for address in addresses]
    if not any(expected) or not int(pmst, 16) & 0x20:
        raise ValueError("missing nonzero upload or enabled program overlay")
    if max(uploads[address][1] for address in addresses) >= float(timestamp):
        raise ValueError("readback does not follow completed upload")
    for words in (program, data):
        if [int(word, 16) for word in words.split(",")] != expected:
            raise ValueError("program/data readback differs from written upload")
    snapshots = [(int(sp, 16), float(t)) for sp, t in SNAPSHOT.findall(text)]
    if not snapshots or max(t for _, t in snapshots) < 28:
        raise ValueError("need stack observations through 28 seconds")
    if any(sp <= 0x1ca5 for sp, _ in snapshots):
        raise ValueError("sampled stack overlaps executable context entry")
    depths = [int(sp, 16) for sp in DEPTH.findall(text)]
    if not depths or min(depths) <= 0x1ca5:
        raise ValueError("missing stack-depth trace or stack entered executable context")
    indices = INDEX.findall(text)
    if len(indices) < 4:
        raise ValueError("need at least four native ring-index publications")
    if any(int(address, 16) not in (0x08e4, 0x08e5)
           or not 0x80 <= int(value, 16) <= 0xe4 for address, value, _ in indices):
        raise ValueError("observed ring index outside its decoded span")
    return {"words": len(expected), "snapshots": len(snapshots), "indices": len(indices)}


def main():
    parser = argparse.ArgumentParser(description=__doc__)
    parser.add_argument("log", type=Path)
    parser.add_argument("--memory-uploads", action="store_true",
                        help="also require the measured operational data uploads")
    args = parser.parse_args()
    try:
        text = args.log.read_text(errors="replace")
        result = check(text)
        if args.memory_uploads:
            check_memory_uploads(text)
    except (OSError, ValueError) as error:
        print(f"NSE-6 native overlay: FAIL {error}", file=sys.stderr)
        return 1
    print(f"NSE-6 native overlay: PASS words={result['words']} snapshots={result['snapshots']} indices={result['indices']}")
    if args.memory_uploads:
        print("NSE-6 native memory uploads: PASS fragments=4 words=156")
    return 0


if __name__ == "__main__":
    sys.exit(main())
