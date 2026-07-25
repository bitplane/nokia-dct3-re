#!/usr/bin/env python3
"""Recover DSP upload material from a passive verbose MAME trace."""

from __future__ import annotations

import argparse
import pathlib
import re
import struct


SHARED_WRITE = re.compile(
    r"dsp_shared_write: off=(?P<offset>[0-9a-f]+) "
    r"old=[0-9a-f]+ data=(?P<data>[0-9a-f]+) "
    r"pc=(?P<pc>[0-9a-f]+) t=(?P<time>[0-9.]+)",
    re.IGNORECASE,
)
PACKET_51 = re.compile(
    r"TX consume type=51 .*?data=(?P<data>[0-9a-f]+) t=",
    re.IGNORECASE,
)


def shared_snapshot(
    text: str, start: int, end: int, cutoff: float
) -> tuple[list[int], set[int]]:
    if start & 1 or end < start or end & 1:
        raise ValueError("shared range must use ordered even byte offsets")
    words = [0] * ((end - start) // 2 + 1)
    observed: set[int] = set()
    for match in SHARED_WRITE.finditer(text):
        if float(match.group("time")) > cutoff:
            continue
        offset = int(match.group("offset"), 16)
        if start <= offset <= end:
            index = (offset - start) // 2
            words[index] = int(match.group("data"), 16)
            observed.add(index)
    return words, observed


def segmented_image(text: str) -> tuple[int, list[int]]:
    image: dict[int, int] = {}
    for match in PACKET_51.finditer(text):
        payload = bytes.fromhex(match.group("data"))
        if len(payload) < 4 or len(payload) & 1:
            raise ValueError("malformed type-0x51 upload packet")
        address = int.from_bytes(payload[:2], "big")
        for offset in range(2, len(payload), 2):
            word_address = address + (offset - 2) // 2
            value = int.from_bytes(payload[offset:offset + 2], "big")
            previous = image.setdefault(word_address, value)
            if previous != value:
                raise ValueError(f"conflicting upload word at 0x{word_address:04x}")
    if not image:
        raise ValueError("no type-0x51 upload packets")
    first, last = min(image), max(image)
    if len(image) != last - first + 1:
        raise ValueError("non-contiguous type-0x51 upload")
    return first, [image[address] for address in range(first, last + 1)]


def encode_words(words: list[int], byteorder: str) -> bytes:
    prefix = ">" if byteorder == "big" else "<"
    return b"".join(struct.pack(prefix + "H", word) for word in words)


def main() -> None:
    parser = argparse.ArgumentParser()
    parser.add_argument("log", type=pathlib.Path)
    parser.add_argument("output", type=pathlib.Path)
    parser.add_argument("--kind", choices=("type51", "shared"), default="type51")
    parser.add_argument("--start", type=lambda value: int(value, 0))
    parser.add_argument("--end", type=lambda value: int(value, 0))
    parser.add_argument("--cutoff", type=float)
    parser.add_argument("--byteorder", choices=("big", "little"), default="big")
    args = parser.parse_args()
    text = args.log.read_text(errors="replace")

    if args.kind == "type51":
        base, words = segmented_image(text)
        detail = f"DSP word base=0x{base:04x}"
    else:
        if args.start is None or args.end is None or args.cutoff is None:
            parser.error("--kind shared requires --start, --end, and --cutoff")
        words, observed = shared_snapshot(text, args.start, args.end, args.cutoff)
        detail = (
            f"shared byte range=0x{args.start:03x}-0x{args.end:03x} "
            f"changed-writes={len(observed)}/{len(words)}"
        )

    args.output.parent.mkdir(parents=True, exist_ok=True)
    args.output.write_bytes(encode_words(words, args.byteorder))
    print(f"{args.output}: {detail} words={len(words)} byteorder={args.byteorder}")


if __name__ == "__main__":
    main()
