"""Compare own full-scale rejection with one nominal external-input experiment."""
import argparse
import hashlib
import json
from pathlib import Path
import re
import sys

if __package__ in (None, ''):
    sys.path.insert(0, str(Path(__file__).resolve().parents[1]))
from tools.nsb3_static_check import inspect

ROW = re.compile(r'nsb3_acquisition: result=([0-9a-f]{8}) sample0=([0-9a-f]{4}) '
                 r'sample1=([0-9a-f]{4}) remaining=([0-9a-f]{2}) gain=3f800000 offset=00000000 observer_writes=0')


def check_traces(control, nominal):
    if ROW.findall(control) != [('00000000', '1a2c', '1a2c', '0a')] * 16:
        raise ValueError('NSB-3 full-scale rejection control differs')
    expected = [('00000001', '0e2c', '0e2c', f'{count:02x}') for count in range(10, 0, -1)]
    if ROW.findall(nominal) != expected or 'ccont_adc2_config: raw=22a external_input=1' not in nominal:
        raise ValueError('NSB-3 nominal acquisition does not drain ten samples organically')


def main():
    parser = argparse.ArgumentParser(description=__doc__)
    parser.add_argument('image', type=Path)
    parser.add_argument('control', type=Path)
    parser.add_argument('nominal', type=Path)
    args = parser.parse_args()
    try:
        source = args.image.read_bytes()
        inspect(source[:0xd6d00])
        check_traces((args.control / 'error.log').read_text(errors='replace'),
                     (args.nominal / 'error.log').read_text(errors='replace'))
        metadata = json.loads((args.nominal / 'input.json').read_text())
        if metadata.get('adc2_raw') != 554 or metadata.get('copied_storage_or_guest_state') is not False:
            raise ValueError('nominal fixture provenance differs')
        for run in (args.control, args.nominal):
            storage = run / 'nvram/nsb3r4t'
            if (storage / 'flash').read_bytes() != source:
                raise ValueError('acquisition experiment changed own flash')
            if hashlib.sha256((storage / 'eeprom').read_bytes()).hexdigest() != '53584321c1f9e317e59cf67401fb5a28aa84fc4db32c67d264731825aa194d19':
                raise ValueError('acquisition experiment changed own EEPROM result')
    except (OSError, ValueError) as exc:
        parser.exit(1, f'NSB-3 acquisition: {exc}\n')
    print('NSB-3 acquisition PASS: saturated rejection, ten nominal successes; presentation remains unresolved')


if __name__ == '__main__':
    main()
