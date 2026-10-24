#!/usr/bin/env python3
"""Physical NPE-3 Phone-to-SIM copy/move and independent cold bank readback."""
import argparse
import hashlib
import json
import os
from pathlib import Path
import shutil
import subprocess
import sys

from PIL import Image

if __package__ in (None, ''):
    sys.path.insert(0, str(Path(__file__).resolve().parents[1]))
from tools.run_dct3_phone_memory import run as prepare_contact, SELECTED_SHA256, EMPTY_SHA256
from tools.run_noki6210_acceptance import build_command, check_frame, CONTACT_SHA256
from tools.noki6210_staged_check import verify
from tools.run_sip_handset_gate import verify_handset_health
from tools.sim_phonebook_check import validate_phonebook, validate_phonebook_storage

COPIED_SHA256 = '12fe4de7e19b94296dbac66f7a783d371881f3277f3f24461ed7d7222107ac25'
MOVED_SHA256 = 'c4c59289b4ef41151b7dda5bb69f99ba6db71ceda29497f50fc327d65a692101'
SIM_SELECTED_SHA256 = '960aacb8e69d1fd3666ab1ea3a475410fd0b6651531f20553250c0d08cb84c5c'


def verify_bank_selection_digests(actual):
    if actual != {'phone': SELECTED_SHA256, 'sim': SIM_SELECTED_SHA256}:
        raise ValueError('cold readback did not explicitly select both distinct memory banks')


def verify_copy_storage(before, copied, phone_cold, sim_cold):
    validate_phonebook_storage(copied, b'A')
    if not before or len(before) != len(copied):
        raise ValueError('missing or truncated complete predecessor SIM')
    if before[:1600] != b'\xff' * 1600:
        raise ValueError('copy destination must start with an empty ADN bank')
    if before[32:] != copied[32:]:
        raise ValueError('copy changed unrelated complete-card contents')
    if copied != phone_cold or copied != sim_cold:
        raise ValueError('cold bank readback changed copied SIM contents')


def verify_copy_trace(trace, move=False):
    cursor = 0
    events = (
            '6210_phone_memory_copy: action=keep_original',
            *(('6210_phone_memory_copy: action=erase_original',) if move else ()),
            '6210_phone_memory_copy: action=copied',
            'header cla=a0 ins=dc p1=01 p2=04 p3=20 selected=6f3a',
            'body ins=dc length=32 selected=6f3a',
            'SIM status ins=dc sw=9000',
            '6210_phone_memory_copy: complete=1 firmware_state_forcing=0')
    if not move and '6210_phone_memory_copy: action=erase_original' in trace:
        raise ValueError('copy fixture selected destructive move semantics')
    for event in events:
        position = trace.find(event, cursor)
        if position < 0:
            raise ValueError('missing ordered physical copy event: ' + event)
        cursor = position + len(event)


def launch_phase(directory, phase, script, seconds, previous):
    root = Path(__file__).resolve().parents[1]
    target = directory / phase
    target.mkdir()
    shutil.copytree(previous / 'nvram', target / 'nvram')
    initial = {str(p.relative_to(target / 'nvram')):
               hashlib.sha256(p.read_bytes()).hexdigest()
               for p in sorted((target / 'nvram').rglob('*')) if p.is_file()}
    command = build_command(root, None, 'npe3hle', script, seconds)
    script_path = Path(command[command.index('-autoboot_script') + 1])
    launch = dict(command=command, initial_storage=initial,
                  predecessor=str(previous), research_hle=True,
                  script_sha256=hashlib.sha256(script_path.read_bytes()).hexdigest(),
                  mame_sha256=hashlib.sha256(Path(command[0]).read_bytes()).hexdigest())
    for name, helper in (('bank', 'bank_read'), ('copy', 'copy_input')):
        launch[name + '_helper_sha256'] = hashlib.sha256(
            (root / f'tools/noki6210_phone_memory_{helper}.lua').read_bytes()).hexdigest()
    (target / 'launch.json').write_text(json.dumps(launch, indent=2) + '\n')
    environment = os.environ.copy()
    environment.pop('NOKIA_DCT3_6210_PIN_ENTRY', None)
    with (target / 'console.log').open('w') as output:
        subprocess.run(command, cwd=target, env=environment, stdout=output,
                       stderr=subprocess.STDOUT, check=True, timeout=180)
    verify_handset_health((target / 'console.log').read_text(errors='replace'))
    verify((target / 'error.log').read_text(errors='replace'), runtime=True, selftest=True)
    return target


def run(directory, move=False):
    directory.mkdir(parents=True, exist_ok=False)
    baseline = directory / 'baseline'
    prepare_contact(baseline)
    previous = baseline / 'cold'
    phases = (
        ('copy', 'phone_memory_move' if move else 'phone_memory_copy', 34),
        ('phone_cold', 'phone_memory_phone_empty' if move else 'phone_memory_phone_read', 40),
        ('sim_cold', 'phone_memory_sim_read', 40))
    for phase, script, seconds in phases:
        if phase != 'copy':
            previous = directory / 'copy'
        previous = launch_phase(directory, phase, script, seconds, previous)
    assess(directory, move)
    print('OK - physical Phone-to-SIM ' + ('move, erased' if move else 'copy, retained') +
          ' Phone source and cold SIM readback')


def assess(directory, move=False):
    baseline = directory / 'baseline'
    card = lambda phase: (directory / phase / 'nvram/npe3hle/sim_card').read_bytes()
    copy_trace = (directory / 'copy/error.log').read_text(errors='replace')
    verify_copy_trace(copy_trace, move)
    validate_phonebook(copy_trace, card('copy'), b'A')
    verify_copy_storage((baseline / 'cold/nvram/npe3hle/sim_card').read_bytes(),
                        card('copy'), card('phone_cold'), card('sim_cold'))
    selections = {}
    for bank in ('phone', 'sim'):
        with Image.open(directory / (bank + '_cold') / 'snap' /
                        f'6210_phone_memory_{bank}_read_{bank}_selected.png') as frame:
            selections[bank] = hashlib.sha256(frame.convert('L').tobytes()).hexdigest()
    verify_bank_selection_digests(selections)
    for phase, filename, marker, digest in (
            ('copy', '6210_phone_memory_copy_copied',
             '6210_phone_memory_copy: complete=1', MOVED_SHA256 if move else COPIED_SHA256),
            ('phone_cold', '6210_phone_memory_phone_read_' + ('result' if move else 'contact'),
             '6210_phone_memory_phone_read: complete=1', EMPTY_SHA256 if move else CONTACT_SHA256),
            ('sim_cold', '6210_phone_memory_sim_read_contact',
             '6210_phone_memory_sim_read: complete=1', CONTACT_SHA256)):
        trace = (directory / phase / 'error.log').read_text(errors='replace')
        if marker not in trace or (phase != 'copy' and 'ins=dc' in trace):
            raise ValueError('missing physical readback or unexpected cold SIM write: ' + phase)
        if phase == 'sim_cold' and 'header cla=a0 ins=b2 p1=01 p2=04 p3=20 selected=6f3a' not in trace:
            raise ValueError('SIM-bank readback did not read copied ADN record')
        with Image.open(directory / phase / 'snap' / (filename + '.png')) as frame:
            check_frame(frame, digest, phase)
    (directory / 'result.json').write_text(json.dumps(dict(
        product='6210', research_hle=True, phone_to_sim=True,
        source_retained_cold=not move, source_erased_cold=move, destination_read_cold=True,
        unrelated_card_unchanged=True), indent=2) + '\n')


def main():
    parser = argparse.ArgumentParser(description=__doc__)
    parser.add_argument('directory', type=Path)
    parser.add_argument('--move', action='store_true', help='Erase original after copying to SIM')
    args = parser.parse_args()
    try:
        run(args.directory.resolve(), args.move)
    except (OSError, ValueError, RuntimeError, subprocess.SubprocessError) as error:
        parser.exit(1, f'FAIL - {error}\n')


if __name__ == '__main__':
    main()
