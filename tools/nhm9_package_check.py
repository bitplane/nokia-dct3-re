#!/usr/bin/env python3
"""Qualify and normalize the acquired NHM-9 v5.30 installer without provisioning."""

import argparse
import hashlib
import json
from pathlib import Path
import sys
import zlib

if __package__ in (None, ''):
    sys.path.insert(0, str(Path(__file__).resolve().parents[1]))
from tools.extract_dct3_wintesla import decode_records


SHA256 = 'e9686c1c5171157b4308a0bdf96008ac040e866e6f5c31ca1ed993bac4e34acd'
FIRST_MEMBER = 0x7710
VERSION = b'V 05.30\n15-03-03\nNHM-9\n(c) NMP.'


def members(source, offset, limit=16 * 1024 * 1024):
    """Walk complete adjacent gzip members, rejecting truncation and trailing data."""
    while offset < len(source):
        decoder = zlib.decompressobj(31)
        payload = decoder.decompress(source[offset:], limit + 1)
        if len(payload) > limit or not decoder.eof:
            raise ValueError('gzip member truncated or exceeds bound')
        consumed = len(source) - offset - len(decoder.unused_data)
        if consumed <= 0:
            raise ValueError('gzip member made no progress')
        yield offset, payload
        offset += consumed


def inspect(source):
    if hashlib.sha256(source).hexdigest() != SHA256:
        raise ValueError('requires the acquired NHM-9 v5.30 installer')
    streams = list(members(source, FIRST_MEMBER))
    images, report = {}, []
    for offset, payload in streams:
        if payload[:1] != b'\x0b':
            continue
        address, data = decode_records(payload)
        if address == 0x200000 and len(data) == 0x270000 and data[0x1fc:0x1fc + len(VERSION)] == VERSION:
            name = 'mcu'
        elif address == 0x470000 and len(data) == 0xc0000 and data.startswith(b'PPM\x00' + VERSION):
            name = data[40:41].decode('ascii')
            if name not in 'ABCDEF':
                raise ValueError('unknown PPM identity')
        else:
            raise ValueError(f'unclassified phone record stream at {offset:#x}')
        if name in images:
            raise ValueError(f'duplicate phone stream {name}')
        images[name] = data
        report.append({'name': name, 'gzip_offset': hex(offset), 'address': hex(address),
                       'bytes': len(data), 'records': len(data) // 0x2000,
                       'record_sha1': hashlib.sha1(payload).hexdigest(),
                       'sha1': hashlib.sha1(data).hexdigest()})
    if len(streams) != 29 or set(images) != {'mcu', *'ABCDEF'}:
        raise ValueError('installer stream inventory differs')
    return images, {'installer_sha256': SHA256, 'gzip_members': len(streams),
                    'phone_streams': report, 'pmm_supplied': False,
                    'boot_claim': False, 'executable_end': '0x530000'}


def main():
    parser = argparse.ArgumentParser(description=__doc__)
    parser.add_argument('installer', type=Path)
    parser.add_argument('--ppm', choices=list('ABCDEF'), default='A')
    parser.add_argument('--output', type=Path, help='write raw MCU plus selected PPM, with no fabricated tail')
    args = parser.parse_args()
    try:
        if args.output and args.output.resolve() == args.installer.resolve():
            raise ValueError('output must not overwrite the source installer')
        images, report = inspect(args.installer.read_bytes())
        combined = images['mcu'] + images[args.ppm]
        report['normalized'] = {'ppm': args.ppm, 'bytes': len(combined),
                                'sha1': hashlib.sha1(combined).hexdigest()}
        if args.output:
            args.output.parent.mkdir(parents=True, exist_ok=True)
            args.output.write_bytes(combined)
    except (OSError, ValueError, zlib.error) as error:
        parser.exit(1, f'NHM-9 package: FAIL: {error}\n')
    print(json.dumps(report, indent=2))


if __name__ == '__main__':
    main()
