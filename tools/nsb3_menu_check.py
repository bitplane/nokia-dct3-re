"""Check the own-EEPROM retry fixture and physical NSB-3 menu control."""
import argparse
import hashlib
import json
from pathlib import Path

FRAME = '52f6ae5577f82efd58e386e4606c4ce713a16b2c67b243ba8d0a708a0dd117be'


def check_fixture(metadata, seed):
    if not (metadata.get('fresh_phone_retry_initialized') and metadata.get('insert_sim')
            and metadata.get('security_checksum_included')):
        raise ValueError('requires explicit retry, integrity and inserted-card fixture')
    if set(metadata['changed_offsets']) != {'0x11c', '0x11d', '0x11e', '0x11f', '0x1fe', '0x1ff', '0x4ac'}:
        raise ValueError('fixture edits differ from seven qualified bytes')
    if len(seed) != 8192 or seed[0x4ac] != 0:
        raise ValueError('fresh retry seed differs')
    if hashlib.sha256(seed).hexdigest() != metadata['initial_eeprom_sha256']:
        raise ValueError('seed hash differs')


def check_trace(text):
    for token in ('nsb3_phonecode_matched_branch: retry=00 observer_writes=0',
                  'nsb3_phonecode_menu: down=1 physical_only=1',
                  'nsb3_phonecode_menu: down=0 physical_only=1'):
        if token not in text:
            raise ValueError('missing ' + token)
    for digit in '12345':
        if f'name=Keypad {digit} down=1' not in text or f'name=Keypad {digit} down=0' not in text:
            raise ValueError('incomplete physical credential input')


def main():
    from PIL import Image
    parser = argparse.ArgumentParser(description=__doc__)
    parser.add_argument('run', type=Path)
    args = parser.parse_args()
    try:
        check_fixture(json.loads((args.run / 'input.json').read_text()),
                      (args.run / 'initial_eeprom.bin').read_bytes())
        check_trace((args.run / 'error.log').read_text())
        with Image.open(args.run / 'snap/nsb3_phonecode_menu.png') as frame:
            if frame.size != (84, 48) or hashlib.sha256(frame.convert('RGB').tobytes()).hexdigest() != FRAME:
                raise ValueError('reviewed Messages menu frame differs')
    except (OSError, ValueError, KeyError) as exc:
        parser.exit(1, f'NSB-3 menu: FAIL: {exc}\n')
    print('PASS: explicit own retry fixture, matched physical code and reviewed Messages menu')


if __name__ == '__main__':
    main()
