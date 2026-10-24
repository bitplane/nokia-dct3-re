"""Characterize completed SIM transfer with unresolved NSB-3 post-menu Wait."""
import argparse
from pathlib import Path
import re
import sys

if __package__ in (None, ''):
    sys.path.insert(0, str(Path(__file__).resolve().parents[1]))
from tools.nsb3_toolkit_check import check
from tools.nsk1_phonebook_roundtrip import frame_check


def check_transport(text):
    rows = re.findall(r'nsb3_toolkit_transfer: stage=return source=([0-9a-f]{8}) header=([0-9a-f]{8}) result=([0-9a-f]{2}) observer_writes=0', text)
    selected = [(header, result) for _, header, result in rows if header in ('a0c20000', 'd3070202')]
    if selected != [('a0c20000', '07'), ('d3070202', '07')]:
        raise ValueError('ENVELOPE header/body transfer completion differs')
    tx = re.findall(r'nsb3_toolkit_envelope_tx: pc=002a16a6 lr=002a1661 sp=([0-9a-f]{8}) stack=([0-9a-f,]+) observer_writes=0', text)
    if len(tx) != 1 or not 0x100000 <= int(tx[0][0], 16) < 0x110000:
        raise ValueError('missing own live SIMI caller context')
    words = tx[0][1].split(',')
    if len(words) != 16 or '00280869' not in words or '00280c3d' not in words:
        raise ValueError('generic transfer caller stack differs')


def main():
    parser = argparse.ArgumentParser(description=__doc__)
    parser.add_argument('source', type=Path)
    parser.add_argument('run', type=Path)
    args = parser.parse_args()
    try:
        check(args.source, args.run, menu_item=1, return_wait=True)
        check_transport((args.run / 'error.log').read_text())
        for name in ('end', 'return'):
            frame_check(args.run / f'snap/nsb3_toolkit_menu_{name}.png',
                        '675fd00adf99c80636183cf695a5291891abdca36ec93e007d01b82b8cf27ec9')
    except (OSError, ValueError, KeyError) as exc:
        parser.exit(1, f'NSB-3 ENVELOPE boundary: FAIL: {exc}\n')
    print('CHARACTERIZED: SIM transfer completed; Wait persists after physical End/Menu; UI settlement unproved')


if __name__ == '__main__':
    main()
