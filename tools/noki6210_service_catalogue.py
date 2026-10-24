"""Extract the own NPE-3 service function table; slots are not wire commands."""
import argparse
import hashlib
import json
from pathlib import Path
import struct


def extract(image):
    if hashlib.sha1(image).hexdigest() != '3d9ea319503e78ec69b60d72cda23e461e118ea9':
        raise ValueError('own NPE-3 MCU hash mismatch')
    base = 0x200000
    table = struct.unpack_from('>I', image, 0x4a013c - base)[0]
    if table != 0x255da0:
        raise ValueError('service table base mismatch')
    values = struct.unpack_from('>103I', image, table - base)
    if values[0] != 0x4b0477:
        raise ValueError('observed version getter is not slot0')
    return {'table': f'{table:08x}', 'dispatcher': '0049fda0',
            'extent': 103, 'non_null': sum(bool(value) for value in values),
            'scope': 'Function slots only; wire-command mapping and read/write effects unclassified.',
            'classified_slots': {
                '0': 'Own version-string pointer getter; runtime observed.',
                '1': 'Peripheral masked byte WRITE at4dbfea; never a read-only probe.',
                '2': 'Peripheral masked byte READ at4dc060; MMIO read side effects not excluded.',
                '4': 'DSP reset/initialization at426d14; clears2048 shared halfwords and writes control state.',
                '13': '24-bit address filter at503206: permits <=1fffff or >=a00000; rejects MCU flash window.',
                '14': '24-bit destination plus byte-count memory WRITE at5031c4; not a read-only probe.',
                '15': 'Wrapper4ca9ba calls503258/3fec30 to copy bounded MCU cache; wrapper effects not fully closed.',
                '16': 'Wrapper4ca906 calls503228/3fe944; paired update path, not qualified read-only.',
                '24': 'DSP mailbox/control publisher at426952; writes shared state, not a read-only probe.'},
            'slots': [{'slot': slot, 'pointer': f'{value:08x}'} for slot, value in enumerate(values)]}


def main():
    parser = argparse.ArgumentParser(description=__doc__)
    parser.add_argument('rom', type=Path)
    args = parser.parse_args()
    try:
        print(json.dumps(extract(args.rom.read_bytes()), indent=2))
    except (OSError, ValueError, struct.error) as error:
        parser.exit(1, f'NPE-3 service catalogue FAIL: {error}\n')


if __name__ == '__main__':
    main()
