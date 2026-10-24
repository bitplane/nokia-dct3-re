"""Qualify own NSB-3 program retention under explicit NSE-1 mask compatibility."""
import argparse
import hashlib
from pathlib import Path
import re
import sys

if __package__ in (None, ''):
    sys.path.insert(0, str(Path(__file__).resolve().parents[1]))
from tools.nsb3_static_check import inspect


def check_upload(source, trace, window):
    if 'nsb3_program_extent: writes=811 highest=2b2a observer_writes=0' not in trace:
        raise ValueError('NSB-3 actual program upload extent differs')
    # Own catalogue-4 entry 1: descriptor 2bd53c, destination 25b4.
    offset = 0xbd53c + 12 + (0x2800 - 0x25b4) * 2
    expected = source[offset:offset + 811 * 2]
    if len(window) != 0x1000 or len(expected) != 1622 or window[:1622] != expected:
        raise ValueError('NSB-3 uploaded program tail was lost or differs from own source')


def check_native(trace):
    if not re.search(r'nsb3_native_state: pc=[0-9a-f]{4} illegal=0 idle=[01] observer_writes=0', trace):
        raise ValueError('missing legal native execution sample')
    if re.search(r'nsb3_native_state: .*illegal=1', trace):
        raise ValueError('native core reached an illegal instruction')
    summary = re.search(r'rom4_interface_summary: completion_strobes=(\d+) mailbox_writes=(\d+) slot_expiries=(\d+) frame_expiries=(\d+)', trace)
    if not summary or any(int(value) == 0 for value in summary.groups()):
        raise ValueError('missing native mailbox/slot/frame activity')


def main():
    parser = argparse.ArgumentParser(description=__doc__)
    parser.add_argument('image', type=Path)
    parser.add_argument('upload_run', type=Path)
    parser.add_argument('native_run', type=Path)
    parser.add_argument('--negative-upload', type=Path, help='pre-fix window with 800 mismatching words')
    args = parser.parse_args()
    try:
        from PIL import Image
        source = args.image.read_bytes()
        inspect(source[:0xd6d00])
        if hashlib.sha256(source).hexdigest() != '927f94aed4ece8e84ad2eda28614bcede1272e89a2d046b16192f88499da19c2':
            raise ValueError('requires own installer-derived PPM-A image')
        window = (args.upload_run / 'nsb3_program_2800.bin').read_bytes()
        check_upload(source, (args.upload_run / 'error.log').read_text(errors='replace'), window)
        check_native((args.native_run / 'error.log').read_text(errors='replace'))
        storage = args.native_run / 'nvram/nsb3r4t'
        if (storage / 'flash').read_bytes() != source:
            raise ValueError('native run altered own flash')
        if hashlib.sha256((storage / 'eeprom').read_bytes()).hexdigest() != '53584321c1f9e317e59cf67401fb5a28aa84fc4db32c67d264731825aa194d19':
            raise ValueError('native own EEPROM initialization differs')
        with Image.open(args.native_run / 'snap/nsb3_startup_5s.png') as frame:
            if frame.size != (84, 48) or hashlib.sha256(frame.convert('RGB').tobytes()).hexdigest() != '5fdab89cdada4b6ac821d32facb589dfb541e9856fe3b569e8da49646fc3d249':
                raise ValueError('full-scale-input native white-frame boundary differs')
        if args.negative_upload:
            before = args.negative_upload.read_bytes()
            if len(before) != len(window) or sum(before[i:i+2] != window[i:i+2] for i in range(0, 1622, 2)) != 800:
                raise ValueError('pre-fix program-overlay negative control differs')
    except (OSError, ValueError) as exc:
        parser.exit(1, f'NSB-3 native compatibility: {exc}\n')
    print('NSB-3 native compatibility PASS: own upload retained, legal execution; white frame, no UI/RF promotion')


if __name__ == '__main__':
    main()
