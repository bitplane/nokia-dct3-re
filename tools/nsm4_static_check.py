"""Recover NSM-4 v5.18 startup and declared DSP uploads, not fitted silicon."""
import argparse
import hashlib
import json
from pathlib import Path
import struct
import sys

if __package__ in (None, ''):
    sys.path.insert(0, str(Path(__file__).resolve().parents[1]))

MCU_SHA256 = 'bf80e6961d37e071c73b0ec5eba448889b4134fa080dfd7f4a6dd04f3a69aac2'
TABLE = 0x3e8e60


def read_catalogue(image):
    offset = TABLE - 0x200000
    if len(image) < offset + 124:
        raise ValueError('NSM-4 catalogue outside image')
    size, destination = struct.unpack_from('>II', image, offset)
    pointers = struct.unpack_from('>29I', image, offset + 8)
    if (size, destination) != (0x74, 0x13a9a8) or pointers[-1] != 0:
        raise ValueError('NSM-4 catalogue initializer differs')
    entries = []
    for selector, pointer in enumerate(pointers[:-1]):
        start = pointer - 0x200000
        if start < 0 or start + 12 > len(image):
            raise ValueError('NSM-4 descriptor outside image')
        header = struct.unpack_from('>6H', image, start)
        end = start + 12 + header[2] * 2
        if end > len(image) or header[0] + header[2] > 0x10000:
            raise ValueError('NSM-4 declared payload exceeds bounds')
        payload = image[start + 12:end]
        entries.append({'selector': selector, 'descriptor': hex(pointer),
                        'header': list(header), 'sha256': hashlib.sha256(payload).hexdigest(),
                        'declared_destination': [header[0], header[0] + header[2]]})
    return entries


def inspect(image):
    if hashlib.sha256(image).hexdigest() != MCU_SHA256:
        raise ValueError('requires own normalized NSM-4 v5.18 MCU')
    entries = read_catalogue(image)
    bootstrap = entries[27]
    if (bootstrap['descriptor'] != '0x4019d8'
            or bootstrap['header'] != [0xf00, 0, 246, 0xf00, 220, 0]
            or bootstrap['sha256'] != '320c8b9c163e13491e0b867b24c2966e10bc51ad96beb2329e6d43eb32d8a9e2'):
        raise ValueError('NSM-4 bootstrap differs')
    return {'mcu_sha256': MCU_SHA256, 'catalogue': hex(TABLE),
            'catalogue_destination': '0x13a9a8', 'entries': entries,
            'reset_arm_entry': '0x200040', 'thumb_entry': '0x200148',
            'startup_calls': ['0x2ea064', '0x23f900', '0x3ab530'],
            'bootstrap': {'identity_data_0803': 6, 'resident_call': '0x861f',
                          'fitted_mask_compatibility_proved': False},
            'scope': 'Static MCU declarations only; no upload execution, peripheral wiring, PMM or resident mask established'}


def main():
    parser = argparse.ArgumentParser(description=__doc__)
    parser.add_argument('mcu', type=Path)
    args = parser.parse_args()
    try:
        report = inspect(args.mcu.read_bytes())
    except (OSError, ValueError) as exc:
        parser.exit(1, f'NSM-4 static check: {exc}\n')
    print(json.dumps(report, indent=2))


if __name__ == '__main__':
    main()
