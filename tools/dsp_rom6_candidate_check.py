"""Audit the private captured ROM6 reference; optionally prepare explicit regions.

Program captures and the data alias are distinct inputs. Prepared data-region
zeros are a declared RAM initialization policy, not a hardware reset snapshot.
"""
import argparse
import hashlib
import json
from pathlib import Path
import re
import sys

PINNED = {
    'acquisition/program.be16': 'ccb400659ee915c9975685d5e644c385c6bdb783410e96063e27667559782af7',
    'acquisition/maskrom.be16': 'fcb9d987110c3288b93b448f2ad5d639399793afff662aa76c20f826de3ed15f',
    'acquisition/drom.txt': '0e64f8b15ccc46dd17eb72995cd3079432af7afe69e284237d7f962f534d8e1b',
    'sweeps/low-words.txt': 'f88d3fe8c108c540235fa6d86356e9b9da00fe7e7ead4a952d8f4ed3e78963f1',
    'sweeps/block-words.txt': '738d3c52a6161ae2629008abfe2e31aa0bcafb233ac63c923cf53cdbadc1d1e8',
    'sweeps/subprobe-words.txt': 'debc557a678538c1180f63b9363c97655cf50b6f71bf580eb311cc5b069a789d',
    'working/rom6_program.be16': 'b450413a3b29a83f691533584f5c923f475e0a720c85acc1ec50097c63eee4cf',
    'acquisition/page1.be16': 'ad6fc9dc87779a22fb23fd02e9789dfc23170d12a88c6bbd25538e292d8eff48',
}
# Single words re-read from hardware after the page-1 capture (A[16]=1).
PAGE1_SPOT_WORDS = {0x8000: 0x10f8, 0xa50d: 0xea00, 0xa776: 0xed00, 0xaee5: 0xf074, 0xbffe: 0x7c2a}
SWEEP = re.compile(r'([0-9A-Fa-f]{4}) ([0-9A-Fa-f]{4}) (ok|ok-confirmed3x)')
ALIAS = re.compile(r'\s*([0-9A-Fa-f]{4}):\s*([0-9A-Fa-f]{4})\s*')


def digest(data):
    return hashlib.sha256(data).hexdigest()


def program_page_audit(program):
    """Enumerate raw far-call patterns, not reachable instruction boundaries."""
    if len(program) != 0x20000:
        raise ValueError('expected exactly one program page')
    words = [int.from_bytes(program[at:at+2], 'big')
             for at in range(0, len(program), 2)]
    candidates = []
    for address, opcode in enumerate(words[:-1]):
        if (opcode & 0xfc80) != 0xf880 or not opcode & 0x7f:
            continue
        candidates.append(dict(site=f'{address:04x}', opcode=f'{opcode:04x}',
            target=f'{((opcode & 0x7f) << 16) | words[address+1]:06x}',
            call=bool(opcode & 0x100), delayed=bool(opcode & 0x200)))
    return dict(supplied_program_pages=[0], higher_program_pages_supplied=False,
        scanned_word_pairs=len(words)-1,
        scope='raw adjacent-word patterns; includes data and operands; not a control-flow or absence proof',
        nonzero_page_candidates=candidates)


def prepare_page1(page1):
    """Program page 1 (A[16]=1) with its DARAM overlay window zeroed.

    With OVLY set, 0080..1fff of every page reads on-chip DARAM, so the
    capture's words there are RAM contents, not ROM. Zeros there are padding,
    as in page 0."""
    if len(page1) != 0x20000:
        raise ValueError('expected exactly one page-1 program page')
    for address, value in PAGE1_SPOT_WORDS.items():
        if int.from_bytes(page1[2*address:2*address+2], 'big') != value:
            raise ValueError(f'page-1 spot word differs at 1:{address:04x}')
    prepared = bytearray(page1)
    prepared[2*0x80:2*0x2000] = bytes(2*(0x2000 - 0x80))
    return bytes(prepared)


def parse_rows(data, expression, first, last):
    rows = {}
    for number, line in enumerate(data.decode('ascii').splitlines(), 1):
        match = expression.fullmatch(line)
        if not match:
            raise ValueError(f'invalid capture row {number}')
        address, value = (int(field, 16) for field in match.groups()[:2])
        if not first <= address <= last or address in rows:
            raise ValueError(f'out-of-range or duplicate capture address {address:04x}')
        rows[address] = value
    return rows


def require_coverage(rows, expected):
    if set(rows) != expected:
        raise ValueError('capture address coverage is incomplete or excessive')


def merge_captures(destination, rows):
    for address, value in rows.items():
        if 0x80 <= address < 0x2000:
            continue
        if address in destination and destination[address] != value:
            raise ValueError(f'contradictory program captures at {address:04x}')
        destination[address] = value


def reconstruct(base, upper, low, blocks, subprobe, alias):
    if len(base) != 0x20000 or len(upper) != 0xa000:
        raise ValueError('incorrect program address-space or upper-image size')
    if any(base[:0x16000]) or base[0x16000:] != upper:
        raise ValueError('acquisition base does not isolate the captured upper program')
    require_coverage(low, set(range(0x8000, 0xb000)))
    require_coverage(subprobe, set(range(0x3800, 0x3c00)))
    require_coverage(alias, set(range(0xb000, 0x10000)))
    captures = {}
    for rows in (low, blocks, subprobe):
        merge_captures(captures, rows)
    require_coverage(captures, set(range(0x80)) | set(range(0x2000, 0xb000)))
    program = bytearray(base)
    for address, value in captures.items():
        program[2*address:2*address+2] = value.to_bytes(2, 'big')
    data = bytearray(0x20000)
    for address, value in alias.items():
        data[2*address:2*address+2] = value.to_bytes(2, 'big')
    if upper == data[0x16000:]:
        raise ValueError('program and data views unexpectedly identical')
    return bytes(program), bytes(data)


def audit(root, pins=PINNED):
    inputs = {}
    for name, expected in pins.items():
        path = root / name
        if not 0 < path.stat().st_size <= 1024*1024:
            raise ValueError(f'invalid bounded input size: {name}')
        inputs[name] = path.read_bytes()
        if digest(inputs[name]) != expected:
            raise ValueError(f'capture hash mismatch: {name}')
    program, data = reconstruct(inputs['acquisition/program.be16'], inputs['acquisition/maskrom.be16'],
        parse_rows(inputs['sweeps/low-words.txt'], SWEEP, 0x8000, 0xafff),
        parse_rows(inputs['sweeps/block-words.txt'], SWEEP, 0, 0x7fff),
        parse_rows(inputs['sweeps/subprobe-words.txt'], SWEEP, 0x3800, 0x3bff),
        parse_rows(inputs['acquisition/drom.txt'], ALIAS, 0xb000, 0xffff))
    if program != inputs['working/rom6_program.be16']:
        raise ValueError('program reconstruction differs from promoted image')
    page1 = prepare_page1(inputs['acquisition/page1.be16'])
    report = dict(program_sha256=digest(program), program_page1_sha256=digest(page1),
        data_region_sha256=digest(data),
        program_bytes=len(program), low_capture_words=12288, data_alias_words=20480,
        program_padding='0080..1fff: zeros, not immutable ROM',
        program_page1='A[16]=1 capture; 0080..1fff zeroed (DARAM overlay window); higher pages not captured',
        data_initialization='outside B000..FFFF: zeros, not a captured reset state',
        program_page_coverage=program_page_audit(program),
        fitted_mask_compatibility=False, execution_validated=False)
    return program, page1, data, report


def main():
    parser = argparse.ArgumentParser(description=__doc__)
    parser.add_argument('root', type=Path)
    parser.add_argument('--prepare', type=Path, help='write derived research regions after all checks pass')
    args = parser.parse_args()
    try:
        program, page1, data, report = audit(args.root)
        if args.prepare:
            args.prepare.mkdir(parents=True, exist_ok=True)
            (args.prepare/'nsm3_rom6_program.be16').write_bytes(program)
            (args.prepare/'nsm3_rom6_program_page1.be16').write_bytes(page1)
            (args.prepare/'nsm3_rom6_data.be16').write_bytes(data)
            (args.prepare/'rom6-input-audit.json').write_text(json.dumps(report, indent=2)+'\n')
    except (OSError, UnicodeError, ValueError) as error:
        print(f'ROM6 candidate rejected: {error}', file=sys.stderr)
        return 1
    print(json.dumps(report, indent=2))
    return 0


if __name__ == '__main__':
    sys.exit(main())
