#!/usr/bin/env python3
"""Qualify the acquired NSB-7 v5.22 package without manufacturing PMM data."""
import argparse
import configparser
import hashlib
import io
import json
from pathlib import Path
import sys
import zipfile

if __package__ in (None, ''):
    sys.path.insert(0, str(Path(__file__).resolve().parents[1]))
from tools.extract_dct3_wintesla import decode_records

SHA256 = '5c28267a14e5d3e36ed4dc166987da42e07b04a83ff88499dc38ee4485df0040'
VERSION = b'V  5.22\n08-11-01\nNSB-7\n(c) NMP.'
MEMBERS = {'nsb7___5.22a', 'nsb7___5.220', 'nsb-7.ini', 'nsb7.pp', 'nsb7.cli'}


def inspect(source):
    if hashlib.sha256(source).hexdigest() != SHA256:
        raise ValueError('requires the acquired NSB-7 v5.22 installer')
    with zipfile.ZipFile(io.BytesIO(source)) as archive:
        names = archive.namelist()
        if len(names) != len(MEMBERS) or set(names) != MEMBERS:
            raise ValueError('package member inventory differs')
        config = configparser.ConfigParser(comment_prefixes=(';',))
        config.read_string(archive.read('nsb-7.ini').decode('ascii'))
        manifest = config['NSB-7_RESTOREFILES']
        if manifest['Market1'] != 'AMERICA_A':
            raise ValueError('restore market differs')
        section = config[manifest['Market1']]
        images, rows = [], []
        for key, address, size, digest in (
                ('ImageFile', 0x200000, 0x160000, '6f2c32bfdb5dce75019f85ac56271fbf05a216cf'),
                ('PpmFile', 0x360000, 0x70000, 'e09921c52e90888992792eceb57d2241f4eb091c')):
            name = section[key].lower()
            start, data = decode_records(archive.read(name), name)
            if start != address or len(data) != size or hashlib.sha1(data).hexdigest() != digest:
                raise ValueError(f'{key}: decoded span/content differs')
            header = data[0x1fc:] if key == 'ImageFile' else data[4:]
            if not header.startswith(VERSION) or (key == 'PpmFile' and not data.startswith(b'PPM\x00')):
                raise ValueError(f'{key}: own firmware identity differs')
            images.append(data)
            rows.append({'member': name, 'address': hex(start), 'bytes': len(data), 'sha1': digest})
        combined = b''.join(images)
        return combined, {'installer_sha256': SHA256, 'product': 'NSB-7',
                          'version': '5.22', 'ppm': 'A', 'phone_streams': rows,
                          'bytes': len(combined), 'sha1': hashlib.sha1(combined).hexdigest(),
                          'restore_erase_start': manifest['LowEraseLimit'],
                          'restore_erase_end': manifest['HighEraseLimit'],
                          'pmm_supplied': False, 'boot_claim': False}


def main():
    parser = argparse.ArgumentParser(description=__doc__)
    parser.add_argument('installer', type=Path)
    parser.add_argument('--output', type=Path)
    args = parser.parse_args()
    try:
        if args.output and args.output.resolve() == args.installer.resolve():
            raise ValueError('output must not overwrite the installer')
        data, report = inspect(args.installer.read_bytes())
        if args.output:
            args.output.parent.mkdir(parents=True, exist_ok=True)
            args.output.write_bytes(data)
    except (OSError, ValueError, KeyError, zipfile.BadZipFile, configparser.Error) as error:
        parser.exit(1, f'NSB-7 package: FAIL: {error}\n')
    print(json.dumps(report, indent=2))


if __name__ == '__main__':
    main()
