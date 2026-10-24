#!/usr/bin/env python3
"""Physical Phone-to-SIM move followed by SIM-to-Phone copy and cold readback."""
import argparse
import hashlib
import json
from pathlib import Path
import subprocess
import sys

from PIL import Image

if __package__ in (None, ''):
    sys.path.insert(0, str(Path(__file__).resolve().parents[1]))
from tools.run_noki6210_phone_memory_copy import (
    run as prepare_move, launch_phase, verify_bank_selection_digests, COPIED_SHA256)
from tools.run_noki6210_acceptance import check_frame, CONTACT_SHA256
from tools.sim_phonebook_check import validate_phonebook_storage


def verify_reverse_trace(trace):
    if 'ins=dc' in trace:
        raise ValueError('SIM-to-Phone copy wrote the source SIM')
    if '6210_phone_memory_reverse: action=erase_original' in trace:
        raise ValueError('reverse copy selected destructive source erasure')
    if 'header cla=a0 ins=b2 p1=01 p2=04 p3=20 selected=6f3a' not in trace:
        raise ValueError('reverse process did not read the SIM source record')
    cursor = 0
    for action in ('sim_to_phone', 'one_by_one', 'contact', 'keep_original', 'copied'):
        event = '6210_phone_memory_reverse: action=' + action
        position = trace.find(event, cursor)
        if position < 0:
            raise ValueError('missing ordered reverse-copy action: ' + action)
        cursor = position + len(event)
    if '6210_phone_memory_reverse: complete=1 firmware_state_forcing=0' not in trace[cursor:]:
        raise ValueError('reverse-copy physical input did not finish')


def verify_reverse_storage(source, copied, phone_cold, sim_cold, before_flash, after_flash):
    validate_phonebook_storage(source, b'A')
    if not source == copied == phone_cold == sim_cold:
        raise ValueError('reverse copy or cold readback changed the complete source SIM')
    if len(before_flash) != 0x400000 or len(after_flash) != len(before_flash):
        raise ValueError('missing complete reverse-copy handset flash')
    if before_flash == after_flash:
        raise ValueError('reverse copy did not change persistent Phone storage')


def assess(directory):
    seed = directory / 'move/sim_cold'
    card = lambda phase: (directory / phase / 'nvram/npe3hle/sim_card').read_bytes()
    verify_reverse_trace((directory / 'reverse/error.log').read_text(errors='replace'))
    verify_reverse_storage((seed / 'nvram/npe3hle/sim_card').read_bytes(),
                           card('reverse'), card('phone_cold'), card('sim_cold'),
                           (seed / 'nvram/npe3hle/flash').read_bytes(),
                           (directory / 'reverse/nvram/npe3hle/flash').read_bytes())
    selections = {}
    for bank in ('phone', 'sim'):
        phase = directory / (bank + '_cold')
        trace = (phase / 'error.log').read_text(errors='replace')
        if f'6210_phone_memory_{bank}_read: complete=1 firmware_state_forcing=0' not in trace or 'ins=dc' in trace:
            raise ValueError('cold reverse-copy bank input missing or SIM write observed')
        with Image.open(phase / 'snap' /
                        f'6210_phone_memory_{bank}_read_{bank}_selected.png') as frame:
            selections[bank] = hashlib.sha256(frame.convert('L').tobytes()).hexdigest()
        with Image.open(phase / 'snap' / f'6210_phone_memory_{bank}_read_contact.png') as frame:
            check_frame(frame, CONTACT_SHA256, 'reverse-copy cold ' + bank)
    verify_bank_selection_digests(selections)
    with Image.open(directory / 'reverse/snap/6210_phone_memory_reverse_copied.png') as frame:
        check_frame(frame, COPIED_SHA256, 'SIM-to-Phone Copied')
    (directory / 'result.json').write_text(json.dumps(dict(
        product='6210', research_hle=True, phone_to_sim_move=True,
        sim_to_phone_copy=True, both_banks_read_cold=True,
        reverse_sim_unchanged=True), indent=2) + '\n')


def run(directory):
    directory.mkdir(parents=True, exist_ok=False)
    prepare_move(directory / 'move', move=True)
    source = directory / 'move/sim_cold'
    reverse = launch_phase(directory, 'reverse', 'phone_memory_reverse_copy', 35, source)
    for bank in ('phone', 'sim'):
        launch_phase(directory, bank + '_cold', 'phone_memory_reverse_' + bank + '_read',
                     40, reverse)
    assess(directory)
    print('OK - physical Phone/SIM roundtrip, cold empty move source and both copied banks')


def main():
    parser = argparse.ArgumentParser(description=__doc__)
    parser.add_argument('directory', type=Path)
    args = parser.parse_args()
    try:
        run(args.directory.resolve())
    except (OSError, ValueError, RuntimeError, subprocess.SubprocessError) as error:
        parser.exit(1, f'FAIL - {error}\n')


if __name__ == '__main__':
    main()
