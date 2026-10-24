"""Qualify two integrity-only controls and the own NSB-3 empty-socket frame."""
import argparse
import hashlib
import json
from pathlib import Path
import sys

if __package__ in (None, ''):
    sys.path.insert(0, str(Path(__file__).resolve().parents[1]))
from tools.nsb3_static_check import inspect
from tools.nsb3_nominal_input import checksum_control


def check_traces(calibration_only, complete):
    comparison = 'nsb3_checksum_compare: stored=db24 computed=db24 secondary=ffff observer_writes=0'
    if comparison not in calibration_only or comparison not in complete:
        raise ValueError('calibration integrity control did not pass the own comparison')
    if 'nsb3_selftest_error: index=12 code=12 pointer=0010fcb2 observer_writes=0' not in calibration_only:
        raise ValueError('missing security-checksum negative control')
    if 'nsb3_selftest_error:' in complete or 'nsb3_selftest_final: flags=40 ' not in complete:
        raise ValueError('own complete integrity control still fails self-test')


def main():
    parser = argparse.ArgumentParser(description=__doc__)
    parser.add_argument('image', type=Path)
    parser.add_argument('source_run', type=Path)
    parser.add_argument('calibration_only', type=Path)
    parser.add_argument('complete', type=Path)
    args = parser.parse_args()
    try:
        from PIL import Image
        image = args.image.read_bytes()
        inspect(image[:0xd6d00])
        original = (args.source_run / 'nvram/nsb3r4t/eeprom').read_bytes()
        if hashlib.sha256(original).hexdigest() != '53584321c1f9e317e59cf67401fb5a28aa84fc4db32c67d264731825aa194d19':
            raise ValueError('requires own qualified cold EEPROM')
        check_traces((args.calibration_only / 'error.log').read_text(errors='replace'),
                     (args.complete / 'error.log').read_text(errors='replace'))
        for run, security in ((args.calibration_only, False), (args.complete, True)):
            expected = checksum_control(original, security)
            if (run / 'initial_eeprom.bin').read_bytes() != expected:
                raise ValueError('fixture changed bytes outside its decoded integrity fields')
            if (run / 'nvram/nsb3r4t/eeprom').read_bytes() != expected:
                raise ValueError('cold run changed integrity-control EEPROM')
            if (run / 'nvram/nsb3r4t/flash').read_bytes() != image:
                raise ValueError('cold control changed own executable')
            metadata = json.loads((run / 'input.json').read_text())
            if metadata.get('security_checksum_included') is not security:
                raise ValueError('integrity fixture provenance differs')
        with Image.open(args.complete / 'snap/nsb3r4t/0000.png') as frame:
            if frame.size != (84, 48) or hashlib.sha256(frame.convert('RGB').tobytes()).hexdigest() != 'a879c658a532b6526d3343287b42848f8a16bf10b7215086c1e215db45ee67cc':
                raise ValueError('own Insert SIM card frame differs')
    except (OSError, ValueError) as exc:
        parser.exit(1, f'NSB-3 self-test: {exc}\n')
    print('NSB-3 self-test PASS: six integrity bytes only; own Insert SIM card, no fitted-mask/RF promotion')


if __name__ == '__main__':
    main()
