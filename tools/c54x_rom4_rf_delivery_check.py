#!/usr/bin/env python3
"""Check the observed native ROM4 scan-response delivery, not RF acquisition."""
import argparse
from pathlib import Path
import re
import sys


EVENT = re.compile(r"rom4_rf_(\w+): (.*)")
FIELDS = re.compile(r"(\w+)=([^ ]+)")


def check(text):
    events = []
    for line in text.splitlines():
        match = EVENT.search(line)
        if match:
            fields = dict(FIELDS.findall(match[2]))
            if "t" in fields:
                events.append((match[1], fields))

    def after(start, kind, predicate):
        for index in range(start, len(events)):
            name, fields = events[index]
            if name == kind and predicate(fields):
                return index, fields
        raise ValueError(f"missing ordered {kind} observation")

    def matches(**expected):
        return lambda fields: all(fields.get(key) == value for key, value in expected.items())

    index, publication = after(0, "native_publication", matches(type="008a", bytes="2"))
    index, queued = after(index + 1, "native_publish_exit", matches(address="3807", type="008a", bytes="2"))
    producer = int(queued["producer"], 16)
    consumer = int(queued["consumer"], 16)
    if not 0x80 <= producer < 0xe4 or not 0x80 <= consumer < 0xe4:
        raise ValueError("invalid receive-ring cursors")
    next_word = lambda cursor: 0x80 if cursor == 0xe3 else cursor + 1
    if next_word(next_word(consumer)) != producer:
        raise ValueError("response is not the next two-word ring packet")
    times = [float(publication["t"]), float(queued["t"])]
    for cursor in (next_word(consumer), producer):
        index, consumed = after(index + 1, "mcu_ring_write",
            matches(address="000101c8", value=f"{cursor:08x}", mask="0000ffff"))
        times.append(float(consumed["t"]))
    index, header = after(index + 1, "status_header_write",
        matches(pc="0026be48", value="13900000", mask="ffff0000"))
    object_address = header["address"]
    times.append(float(header["t"]))
    index, posted = after(index + 1, "post_fetch",
        matches(address="00254c84", pc="00254c84", r0="0000000a",
                r1=object_address, lr="0026be5d"))
    times.append(float(posted["t"]))
    _, received = after(index + 1, "task10_lifecycle",
        matches(pc="00211a9a", r0=object_address, status="1390"))
    times.append(float(received["t"]))
    if any(later < earlier for earlier, later in zip(times, times[1:])):
        raise ValueError("delivery timestamps regress")
    return object_address, publication["words"]


def main():
    parser = argparse.ArgumentParser(description=__doc__)
    parser.add_argument("log", type=Path)
    args = parser.parse_args()
    try:
        object_address, body = check(args.log.read_text())
    except (OSError, ValueError, KeyError) as error:
        print(f"ROM4 RF delivery: FAIL: {error}", file=sys.stderr)
        return 1
    print(f"ROM4 RF delivery: PASS object={object_address} native_body={body} acquisition_claim=0")
    return 0


if __name__ == "__main__":
    sys.exit(main())
