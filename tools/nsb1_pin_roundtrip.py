"""Verify physical NSB-1 wrong-PIN persistence and correct cold PIN entry."""

import argparse
import hashlib
import json
from pathlib import Path
import re
import subprocess
import sys

if __package__ in (None, ''):
    sys.path.insert(0, str(Path(__file__).resolve().parents[1]))
from tools.make_sim_card_profile import CHV1, CHV2, PUK1, PUK2, make_profile
from tools.nsb1_rom4_compare import (
    INPUTS, check_card_run, check_frame, prepare, prepare_cold_run, run_mame,
)
from tools.sim_security_trace_check import AOC_NVRAM_SIZE, CHV_OFFSET, ENABLED_OFFSET, ATTEMPTS_OFFSET, statuses


PIN_PROMPT_RGB_SHA256 = '5fd68f8b16b4650fca10ce9306cea07402bd26b9217b37d5b54fc70f82db7f6f'
WRONG_RGB_SHA256 = 'fd5b8fb1ab0c3397bf958d567a46aee84a72f75123802a00d5bc802a1814543e'
ACCEPTED_RGB_SHA256 = 'dd6eba3e2fe5e7bdfbc0fc187d2dc63475c1227911a0deeaa4be70546075a0c4'
PHONE_PROMPT_RGB_SHA256 = '43d88e4723dbb8c3939169fd10c2e9d6faef1949121371303a1ee55401fd32e3'


def check_card(storage, retries):
    if len(storage) != AOC_NVRAM_SIZE or storage[ENABLED_OFFSET] != 1:
        raise ValueError('PIN card geometry or persistent enabled state changed')
    if storage[CHV_OFFSET:CHV_OFFSET + 32] != CHV1 + CHV2 + PUK1 + PUK2:
        raise ValueError('PIN/PUK contents changed')
    if storage[ATTEMPTS_OFFSET:ATTEMPTS_OFFSET + 4] != bytes((retries, 3, 10, 10)):
        raise ValueError('persistent PIN/PUK retry counters changed')


def check_contacts(trace, phase, count):
    contacts = re.findall(rf'nsb1_pin_contact: phase={phase} index=(\d+) down=([01]) t=[0-9.]+ physical_only=1', trace)
    if contacts != [(str(index), down) for index in range(1, count + 1) for down in ('1', '0')]:
        raise ValueError(f'missing physical {phase} contacts')


def check_wrong(trace, storage, snapshots):
    check_contacts(trace, 'wrong', 5)
    if statuses(trace, 0x20) != [(0x9804, 2, 10)]:
        raise ValueError('wrong physical PIN did not consume exactly one retry')
    check_card(storage, 2)
    check_frame(snapshots / 'nsb1_pin_wrong_1.png', PIN_PROMPT_RGB_SHA256)
    check_frame(snapshots / 'nsb1_pin_wrong_2.png', WRONG_RGB_SHA256)


def check_correct(trace, storage, snapshots):
    check_contacts(trace, 'pin', 5)
    check_contacts(trace, 'phone', 6)
    if statuses(trace, 0x20) != [(0x9000, 3, 10)]:
        raise ValueError('correct cold PIN did not restore the retry count')
    check_card(storage, 3)
    for index, expected in ((1, PIN_PROMPT_RGB_SHA256), (2, ACCEPTED_RGB_SHA256),
                            (3, PHONE_PROMPT_RGB_SHA256), (4, ACCEPTED_RGB_SHA256)):
        check_frame(snapshots / f'nsb1_pin_reload_{index}.png', expected)


def main():
    parser = argparse.ArgumentParser(description=__doc__)
    parser.add_argument('binary', type=Path)
    parser.add_argument('rom_root', type=Path)
    parser.add_argument('run_dir', type=Path)
    parser.add_argument('--checksum-control-from', required=True, type=Path)
    args = parser.parse_args()
    try:
        binary, root, run = args.binary.resolve(), args.rom_root.resolve(), args.run_dir.resolve()
        if run.exists():
            raise ValueError('PIN round trip requires a fresh directory')
        wrong, correct = run / 'wrong', run / 'correct'
        rompath = prepare(root, wrong, args.checksum_control_from.resolve(), True, True)
        card = make_profile(True)
        (wrong / 'nvram/nsb1r4t/sim_card').write_bytes(card)
        manifest_path = wrong / 'input_manifest.json'
        manifest = json.loads(manifest_path.read_text())
        manifest['synthetic_pin_profile_sha256'] = hashlib.sha256(card).hexdigest()
        manifest['synthetic_pin_profile'] = 'shared laboratory PIN-enabled card; no donor storage'
        manifest_path.write_text(json.dumps(manifest, indent=2) + '\n')
        code = run_mame(binary, rompath, wrong, Path(__file__).with_name('nsb1_pin_fixture.lua').resolve(), 24, True)
        trace, storage = check_card_run(wrong, code, 24)
        check_wrong(trace, storage, wrong / 'snap')
        rompath = prepare_cold_run(correct, wrong, lambda data: check_card(data, 2), INPUTS)
        code = run_mame(binary, rompath, correct, Path(__file__).with_name('nsb1_pin_reload.lua').resolve(), 60, True)
        trace, storage = check_card_run(correct, code, 60)
        check_correct(trace, storage, correct / 'snap')
        activity_observed = 'nsb1_activity_check:' in trace
        (run / 'result.json').write_text(json.dumps({'physical_wrong_pin': True, 'cold_retry_persistence': True,
            'physical_correct_pin': True, 'phone_code_distinct': True,
            'mcu_inactivity_branch_observed': activity_observed, 'fitted_mask_proved': False}, indent=2) + '\n')
    except (OSError, ValueError, subprocess.TimeoutExpired) as error:
        parser.exit(1, f'NSB-1 PIN round trip failed: {error}\n')
    coverage = 'MCU inactivity branch observed' if activity_observed else 'MCU inactivity branch not covered'
    print(f'NSB-1 physical wrong PIN, cold retry persistence and correct PIN PASS; {coverage}; compatibility limits retained; evidence={run}')


if __name__ == '__main__':
    main()
