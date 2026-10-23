#!/usr/bin/env python3
"""List candidate C54x PORTR/PORTW sites in a big-endian word image."""

from __future__ import annotations

import argparse
import json
import re
from collections import defaultdict
from pathlib import Path


def census(image: bytes) -> dict[tuple[str, int], list[int]]:
    if len(image) % 2:
        raise ValueError("C54x image must contain complete 16-bit words")
    words = [int.from_bytes(image[i:i + 2], "big") for i in range(0, len(image), 2)]
    sites: dict[tuple[str, int], list[int]] = defaultdict(list)
    for address, opcode in enumerate(words):
        family = opcode & 0xff00
        if family not in (0x7400, 0x7500):
            continue
        mode = opcode & 0xff
        # A port operand alone is not a complete long-offset instruction.
        if mode >= 0xe0 and address + 2 >= len(words):
            continue
        # Absolute Smem (F8..FF) puts the address before the port; long-offset
        # forms (E0..F7) put the port before the address extension instead.
        port_index = address + (2 if mode >= 0xf8 else 1)
        if port_index < len(words) and words[port_index] < 0x100:
            direction = "R" if family == 0x7400 else "W"
            sites[(direction, words[port_index])].append(address)
    return dict(sites)


def direct_callers(image: bytes, target: int) -> list[tuple[int, bool]]:
    """Candidate CALL/CALLD sites; raw words need runtime confirmation."""
    if len(image) % 2:
        raise ValueError("C54x image must contain complete 16-bit words")
    words = [int.from_bytes(image[i:i + 2], "big") for i in range(0, len(image), 2)]
    return [(address, opcode == 0xf274)
            for address, opcode in enumerate(words[:-1])
            if opcode in (0xf074, 0xf274) and words[address + 1] == target]


def classify_candidates(image: bytes, disassemblies: list[str], ports=None):
    """Classify raw candidates against byte-matched linear disassembly.

    Linear decode coverage is not a CFG, execution, or physical absence proof.
    Overlapping exports that disagree remain conflicts, not majority votes.
    """
    candidates=census(image)
    intervals=[]
    for source in disassemblies:
        rows=[]
        for line in source.splitlines():
            match=re.match(r'^00:([0-9a-fA-F]{4}):\s+(.*)$',line)
            if match:
                rows.append((int(match[1],16),match[2]))
        for (start,body),(end,_) in zip(rows,rows[1:]):
            length=end-start
            if not 1<=length<=3:
                continue
            tokens=body.split()
            if len(tokens)<=length or not all(re.fullmatch(r'[0-9a-fA-F]{4}',word) for word in tokens[:length]):
                continue
            encoded=bytes.fromhex(''.join(tokens[:length]))
            valid=image[start*2:end*2]==encoded
            intervals.append((start,end,tokens[length].upper(),valid))
    result=[]
    for (direction,port),addresses in sorted(candidates.items()):
        if ports is not None and port not in ports:
            continue
        for address in addresses:
            opinions=set()
            for start,end,mnemonic,valid in intervals:
                if start<=address<end:
                    if not valid:
                        opinions.add('stale_disassembly')
                    elif address>start:
                        opinions.add('operand_word')
                    elif mnemonic==('PORTR' if direction=='R' else 'PORTW'):
                        opinions.add('linear_io')
                    else:
                        opinions.add('non_io_decode')
            status=next(iter(opinions)) if len(opinions)==1 else 'conflicting' if opinions else 'uncovered'
            result.append(dict(direction=direction,port=f'{port:02x}',address=f'{address:04x}',
                               classification=status,evidence=sorted(opinions)))
    return result


def caller_contexts(image: bytes, target: int, radius: int):
    """Raw surrounding words, not an instruction-boundary or argument proof."""
    if radius < 0:
        raise ValueError("context radius must be nonnegative")
    sites = direct_callers(image, target)
    words = [int.from_bytes(image[i:i + 2], "big") for i in range(0, len(image), 2)]
    return [(address, delayed, max(0, address - radius),
             words[max(0, address - radius):min(len(words), address + 2 + radius)])
            for address, delayed in sites]


def main() -> None:
    parser = argparse.ArgumentParser(description=__doc__)
    parser.add_argument("image", type=Path)
    parser.add_argument("--port", type=lambda value: int(value, 0))
    parser.add_argument("--call-target", type=lambda value: int(value, 0))
    parser.add_argument("--disassembly", type=Path, action='append', default=[],
                        help="classify candidates using byte-matched MAME linear exports; repeatable")
    parser.add_argument("--context", type=int, default=0,
                        help="raw words before/after each candidate call; not recovered arguments")
    args = parser.parse_args()
    image = args.image.read_bytes()
    if args.disassembly:
        rows=classify_candidates(image,[path.read_text() for path in args.disassembly],
                                 None if args.port is None else {args.port})
        print(json.dumps(dict(candidates=rows,coverage=dict(
            (status,sum(row['classification']==status for row in rows))
            for status in sorted({row['classification'] for row in rows})),
            runtime_reachability_proved=False),indent=2))
        return
    if args.call_target is not None:
        if args.context < 0:
            parser.error("context radius must be nonnegative")
        for address, delayed, base, words in caller_contexts(image, args.call_target, args.context):
            print(f"{address:04x} {'CALLD' if delayed else 'CALL'} {args.call_target:04x}")
            if args.context:
                print(f"  words@{base:04x}: " + " ".join(f"{word:04x}" for word in words))
        return
    for (direction, port), addresses in sorted(census(image).items()):
        if args.port is None or args.port == port:
            print(f"{direction} {port:02x} {len(addresses):3d} " +
                  " ".join(f"{address:04x}" for address in addresses))


if __name__ == "__main__":
    main()
