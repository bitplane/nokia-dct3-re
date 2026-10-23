"""Check the pinned TI 4.2.0 operand probe; not a DSP execution oracle."""
import argparse
import hashlib
import json
from pathlib import Path
import re


SOURCE_SHA256 = 'b041552799eefc7ec42f4a9a5684afac3364f13e047a7dd5a99d9c939987100d'
ASSEMBLER_SHA256 = '02fc7aae931091e62b1f39ca7913d48b9a6699ed1a8e1dae0fdaf8e0ebe0e91b'
DRIVER_SHA256 = '452a93dee4321d58388164858e8e2a006dcbfa8cb120eb651d47bbe36c892a62'
WORDS = (0x1092, 0x109a, 0x10ea, 1, 0x809a, 0x689a, 0xff,
         0x699a, 1, 0x6a9a, 1, 0x6b9a, 1)
WARNINGS = (3, 6, 7, 8, 9)


def check(listing):
    if not listing.startswith('TMS320C54x Assembler PC v4.2.0 '):
        raise ValueError('unreviewed assembler version')
    words = []
    for line in listing.splitlines():
        match = re.fullmatch(r'\s+(?:\d+\s+)?([0-9a-fA-F]{6})\s+([0-9a-fA-F]{4})(?:\s+.*)?', line)
        if match:
            address, word = (int(value, 16) for value in match.groups())
            if address != len(words):
                raise ValueError('noncontiguous or duplicate listing addresses')
            words.append(word)
    if tuple(words) != WORDS:
        raise ValueError('operand control encodings differ')
    warnings = re.findall(
        r'^ "operand_probe\.asm", WARNING! at line (\d+): \[W0000\] \*\+ARn addressing is for write-only\s*$',
        listing, re.MULTILINE)
    if tuple(map(int, warnings)) != WARNINGS or listing.count('WARNING!') != len(WARNINGS):
        raise ValueError('write-only diagnostics differ')
    if 'No Assembly Errors, 5 Assembly Warnings' not in listing or 'ERROR!' in listing:
        raise ValueError('missing successful assembly with the expected warnings')
    return dict(instructions=8, words=len(words), warning_lines=list(WARNINGS),
                ordinary_store_and_long_read_unflagged=True,
                short_rmw_flagged=True, dsp_execution_validated=False,
                silicon_trap_validated=False)


def pinned(path, expected, name):
    if hashlib.sha256(path.read_bytes()).hexdigest() != expected:
        raise ValueError(f'{name} does not match the reviewed input')


def main():
    parser = argparse.ArgumentParser(description=__doc__)
    parser.add_argument('listing', type=Path)
    parser.add_argument('--source', required=True, type=Path)
    parser.add_argument('--assembler', required=True, type=Path)
    parser.add_argument('--driver', required=True, type=Path)
    args = parser.parse_args()
    try:
        pinned(args.source, SOURCE_SHA256, 'probe source')
        pinned(args.assembler, ASSEMBLER_SHA256, 'assembler')
        pinned(args.driver, DRIVER_SHA256, 'compiler driver')
        result = check(args.listing.read_text())
    except (OSError, ValueError) as error:
        parser.exit(1, f'C54x vendor operand probe FAIL: {error}\n')
    print(json.dumps(result, sort_keys=True))


if __name__ == '__main__':
    main()
