"""Bound the acquired NAM-1 v5.11 installer contents without executing it."""
import argparse
import hashlib
import json
from pathlib import Path
import sys
import zlib

if __package__ in (None, ''):
    sys.path.insert(0, str(Path(__file__).resolve().parents[1]))
from tools.nse5_installer_audit import records

INSTALLER_SHA256 = 'cfdd4bd690776a0bfe37ad99e0b4fc1517543777c3c17e6eaeea5d44e1fce7ef'


def inspect(source, flash):
    rows = []
    offset = 0
    while (offset := source.find(b'\x1f\x8b\x08', offset)) >= 0:
        decoder = zlib.decompressobj(31)
        try:
            payload = decoder.decompress(source[offset:], 16 * 1024 * 1024 + 1)
        except zlib.error:
            offset += 1
            continue
        if not decoder.eof or len(payload) > 16 * 1024 * 1024:
            raise ValueError(f'incomplete or oversized gzip candidate at {offset:x}')
        row = dict(offset=f'{offset:x}', bytes=len(payload),
                   sha256=hashlib.sha256(payload).hexdigest())
        try:
            items = records(payload)
        except ValueError:
            row['wintesla_records'] = False
        else:
            row.update(wintesla_records=True, records=len(items),
                       start=f'{items[0][0]:06x}', end=f'{items[-1][0] + len(items[-1][1]) - 1:06x}')
            outside, mismatches = 0, 0
            for address, data in items:
                index = address - 0x200000
                if index < 0 or index + len(data) > len(flash):
                    outside += len(data)
                else:
                    mismatches += sum(a != b for a, b in zip(data, flash[index:index + len(data)]))
            row.update(outside_image_bytes=outside, different_existing_bytes=mismatches)
        rows.append(row)
        offset += len(source[offset:]) - len(decoder.unused_data)
    return dict(installer_sha256=hashlib.sha256(source).hexdigest(),
                gzip_members=len(rows), members=rows, product_state_proved=False)


def main():
    parser = argparse.ArgumentParser(description=__doc__)
    parser.add_argument('installer', type=Path)
    parser.add_argument('flash', type=Path)
    args = parser.parse_args()
    try:
        source, flash = args.installer.read_bytes(), args.flash.read_bytes()
        if hashlib.sha256(source).hexdigest() != INSTALLER_SHA256 or len(flash) != 0x350000:
            raise ValueError('expected acquired NAM-1 v5.11 installer and 0x350000-byte image')
        report = inspect(source, flash)
        streams = [row for row in report['members'] if row['wintesla_records']]
        if report['gzip_members'] != 30 or len(streams) != 7:
            raise ValueError('installer member coverage differs')
        matching = [(row['offset'], row['start'], row['end']) for row in streams
                    if row['different_existing_bytes'] == row['outside_image_bytes'] == 0]
        if matching != [('2e016', '200000', '48ffff'), ('342061', '490000', '54ffff')]:
            raise ValueError('own MCU/PPM-E comparison differs')
        if any(row['outside_image_bytes'] for row in streams):
            raise ValueError('unexpected record data outside existing image; inspect before admission')
    except (OSError, ValueError) as error:
        parser.exit(1, f'NAM-1 installer audit FAIL: {error}\n')
    print(json.dumps(report, indent=2))


if __name__ == '__main__':
    main()
