"""Qualify the acquired NSB-1 v6.71 service package without donor storage."""

import argparse
import configparser
import hashlib
import io
import json
from pathlib import Path
import sys
from zipfile import BadZipFile, ZipFile

if __package__ in (None, ''):
    sys.path.insert(0, str(Path(__file__).resolve().parents[1]))
from tools.extract_dct3_wintesla import decode_records

SHA256 = '7c7167aba12342fa1ef3810c93bf518b0f8aeb79f67e9637e19d76b34e781128'
VERSION = b'V  6.71\n25-01-01\nNSB-1\n(c) NMP.'
MEMBERS = {'nsb1_3.pp', 'nsb1___6.71a', 'nsb1___6.71b',
           'nsb1___6.710', 'nsb1_671.doc', 'nsb-1.ini'}


def eeprom_geometry(descriptor):
    page_shift = ((descriptor >> 3) & 7) - 1
    if page_shift < 0:
        raise ValueError('EEPROM descriptor has an invalid page shift')
    return {'descriptor': hex(descriptor), 'address_bytes': 2 if descriptor & 0x40 else 1,
            'page_bytes': 1 << page_shift, 'capacity_bytes': 1 << ((descriptor & 7) + 9),
            'fitted_part_proved': False}


def inspect(source, ppm='A'):
    if ppm not in ('A', 'B'):
        raise ValueError('unsupported NSB-1 PPM selection')
    if hashlib.sha256(source).hexdigest() != SHA256:
        raise ValueError('requires the acquired 5190 NSB-1 v6.71 package')
    with ZipFile(io.BytesIO(source)) as archive:
        if len(archive.infolist()) != 6 or set(archive.namelist()) != MEMBERS:
            raise ValueError('unexpected archive member inventory')
        if any(member.file_size > 2 * 1024 * 1024 for member in archive.infolist()):
            raise ValueError('archive member exceeds expected bound')
        config = configparser.ConfigParser(interpolation=None)
        config.read_string(archive.read('nsb-1.ini').decode('ascii'))
        restore = config['NSB-1_RESTOREFILES']
        if (restore['LowEraseLimit'], restore['HighEraseLimit'], restore['ROM4ImageFile']) != (
                '200000', '2FFFFF', 'nsb1___6.710'):
            raise ValueError('unexpected NSB-1 restore map')
        if (config['USA']['ROM4PpmFile'], config['USA_T9']['ROM4PpmFile']) != (
                'nsb1___6.71a', 'nsb1___6.71b'):
            raise ValueError('unexpected NSB-1 market/PPM mapping')
        image = bytearray(b'\xff' * 0x100000)
        streams = []
        for name, start, length, kind in (
                ('nsb1___6.710', 0x200000, 0xd2d00, 'MCU'),
                ('nsb1___6.71a', 0x2e0000, 0x20000, 'A'),
                ('nsb1___6.71b', 0x2e0000, 0x20000, 'B')):
            raw = archive.read(name)
            address, data = decode_records(raw)
            if address != start or len(data) != length or data.count(VERSION) != 1:
                raise ValueError(f'unexpected {kind} source span or product/version')
            if kind != 'MCU' and (not data.startswith(b'PPM\0' + VERSION) or data[40:41] != kind.encode()):
                raise ValueError(f'unexpected {kind} PPM identity')
            if kind in ('MCU', ppm):
                image[start - 0x200000:start - 0x200000 + length] = data
            streams.append({'name': kind, 'address': hex(start), 'bytes': length,
                            'raw_sha1': hashlib.sha1(raw).hexdigest(),
                            'decoded_sha1': hashlib.sha1(data).hexdigest()})
    return bytes(image), {'archive_sha256': SHA256, 'streams': streams,
                         'ppm': ppm, 'normalized_bytes': len(image),
                         'firmware_eeprom_geometry': eeprom_geometry(image[5]),
                         'normalized_sha1': hashlib.sha1(image).hexdigest(),
                         'absent_erased_gap': ['0x2d2d00', '0x2dffff'],
                         'provisioning_supplied': False, 'boot_claim': False,
                         'resident_rom4_supplied': False}


def main():
    parser = argparse.ArgumentParser(description=__doc__)
    parser.add_argument('archive', type=Path)
    parser.add_argument('--ppm', choices=('A', 'B'), default='A')
    parser.add_argument('--output', type=Path)
    args = parser.parse_args()
    try:
        if args.output and args.output.resolve() == args.archive.resolve():
            raise ValueError('output must not overwrite the source archive')
        image, report = inspect(args.archive.read_bytes(), args.ppm)
        if args.output:
            args.output.parent.mkdir(parents=True, exist_ok=True)
            args.output.write_bytes(image)
    except (OSError, ValueError, BadZipFile, KeyError, configparser.Error) as error:
        parser.exit(1, f'NSB-1 package: FAIL: {error}\n')
    print(json.dumps(report, indent=2))


if __name__ == '__main__':
    main()
