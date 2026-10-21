"""Physical NSB-7 FDN On/cold-On/Off/cold-Off with exact SIM transitions."""
import argparse
from pathlib import Path
import re
import sys

if __package__ in (None, ''):
    sys.path.insert(0, str(Path(__file__).resolve().parents[1]))
from tools.nsb7_pin_check import check_trace as check_pin, check_frame
from tools.nsb7_security_check import check_trace as check_phone
from tools.nsb7_storage_check import inspect
from tools.sim_fdn_profile import expected_validity_card
from tools.sim_fdn_protocol_check import check_phase

PREFIX = [('Left Softkey / Menu', 0x19), ('Keypad 4', 4), ('Keypad 6', 6),
          ('Keypad 4', 4), ('Left Softkey / Menu', 0x19)]
ON_HASH = '3b6c0ba6a6e82be7ca88e6f668727991cfe76e4964be8f4e20f0dbfd83a381c7'
OFF_HASH = '4410aaf3dda54376a10f76949ce1e79e797e26744b085c992d17a6d5df986087'


def check_trace(text, phase):
    check_pin(text.split('nsb7_menu_input:', 1)[0], True)
    check_phone(text)
    check_phase(text, phase, 'nsb7_sim_tx')
    keys = list(PREFIX)
    if phase % 2 == 0:
        keys += [('Left Softkey / Menu', 0x19),
                 ('Scroll Up', 0x17) if phase == 0 else ('Scroll Down', 0x18),
                 ('Left Softkey / Menu', 0x19)]
        keys += [('Keypad '+str(digit), digit) for digit in (5, 6, 7, 8)]
        keys += [('Left Softkey / Menu', 0x19)]
    rows = list(re.finditer(r'nsb7_menu_input: key=(.*?) state=([01]) ', text))
    if [(row[1], row[2]) for row in rows] != [(key, edge) for key, _ in keys for edge in ('1', '0')]:
        raise ValueError('FDN physical contact sequence differs')
    for index, (_, code) in enumerate(keys):
        if f'nsb7_key_decode: key={code:02x} observer_writes=0' not in text[rows[2*index].end():rows[2*index+1].start()]:
            raise ValueError('FDN physical contact did not decode')


def check_card(before, after, phase, previous=None):
    if previous is not None and before != previous:
        raise ValueError('FDN cold input differs from predecessor')
    if after != expected_validity_card(before, phase):
        raise ValueError('FDN changed card bytes beyond expected validity')


def main():
    parser = argparse.ArgumentParser(description=__doc__)
    for name in ('on', 'cold_on', 'off', 'cold_off'):
        parser.add_argument(name, type=Path)
    args = parser.parse_args()
    try:
        previous = None
        for phase, run in enumerate((args.on, args.cold_on, args.off, args.cold_off)):
            check_trace((run/'error.log').read_text(), phase)
            storage = run/'nvram/nsb7hle'
            inspect((storage/'flash').read_bytes())
            card = (storage/'sim_card').read_bytes()
            check_card((run/'initial_sim_card.bin').read_bytes(), card, phase, previous)
            check_frame(run/f'snap/nsb7_menu_{35 if phase % 2 else 51}.png',
                        ON_HASH if phase < 2 else OFF_HASH)
            previous = card
    except (OSError, ValueError) as error:
        parser.exit(1, f'NSB-7 FDN: FAIL: {error}\n')
    print('PASS: physical FDN On/cold-On/Off/cold-Off and exact whole-card transitions')


if __name__ == '__main__':
    main()
