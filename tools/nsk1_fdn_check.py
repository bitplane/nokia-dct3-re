"""Own NSK-1 physical FDN validity changes and cold SIM identity handshake."""
import argparse
import hashlib
import json
from pathlib import Path
import re
import sys

if __package__ in (None, ''):
    sys.path.insert(0, str(Path(__file__).resolve().parents[1]))
from tools.nsk1_pin_roundtrip import check_run as check_pin, check_contacts
from tools.nsk1_pin2_change_check import check_cold_link
from tools.nsk1_security_compare import check_phase as check_native
from tools.nsk1_phonebook_roundtrip import frame_check
from tools.sim_fdn_profile import extend, expected_validity_card
from tools.sim_fdn_protocol_check import check_phase

MENU = 'menu down down down menu down down menu down menu'.split()
KEY_CODES = {'menu': 0x19, 'clear': 0x1a, 'down': 0x18, 'up': 0x17, 'one': 1, 'two': 2, 'three': 3, 'four': 4, 'five': 5, 'six': 6, 'seven': 7, 'eight': 8, 'hash': 0x0b}
ON_FRAME = '1a85da1529f1469b41d424026cb0b489fc25e5dab64056b729566e0cf64fd53b'
OFF_FRAME = 'aa1638502f5eef8a12434630a230ebd396607341405c96b147a43fecb5be7462'


def phase_keys(phase):
    if phase not in range(4):
        raise ValueError('known FDN phase required')
    if phase % 2:
        return MENU[:-1]
    return MENU + ['up' if phase == 0 else 'down', 'menu', 'five', 'six', 'seven', 'eight', 'menu']


def check_input(trace, keys):
    check_contacts(trace, 'pin', 5)
    check_contacts(trace, 'phone', 6)
    contacts = re.findall(r'nsk1_fdn_contact: index=(\d+) key=(\w+) down=([01]) t=[0-9.]+ physical_only=1', trace)
    if contacts != [(str(i), key, edge) for i, key in enumerate(keys, 1) for edge in ('1', '0')] or len(contacts) != trace.count('nsk1_fdn_contact:'):
        raise ValueError('physical own FDN route differs')
    if (trace.index('SIM status ins=20 sw=9000') > trace.index('nsk1_pin_contact: phase=phone index=1 down=1 ')
            or trace.index('nsk1_fdn_contact: index=1 ') < trace.index('nsk1_pin_contact: phase=phone index=6 down=0 ')):
        raise ValueError('SIM/phone/FDN input ordering differs')
    decoded = re.findall(r'nsk1_fdn_key: value=([0-9a-f]{2}) t=[0-9.]+ observer_writes=0', trace)
    expected = [1, 2, 3, 4, 0x19, 1, 2, 3, 4, 5, 0x19] + [KEY_CODES[key] for key in keys]
    if [int(key, 16) for key in decoded] != expected or len(decoded) != trace.count('nsk1_fdn_key:'):
        raise ValueError('decoded FDN input differs')


def check_trace(trace, phase):
    check_input(trace, phase_keys(phase))
    check_phase(trace, phase, 'nsk1_sim_tx')


def check_runs(source, runs):
    if len(runs) != 4:
        raise ValueError('four linked FDN phases required')
    _, wrong = check_pin(source / 'wrong')
    _, base = check_pin(source / 'correct', True, wrong)
    previous = source / 'correct'
    for phase, run in enumerate(runs):
        before = check_cold_link(run, previous)
        if phase == 0:
            before = extend(base, True)
            fixture = json.loads((run / 'input_manifest.json').read_text()).get('card_fixture')
            expected = {'kind': 'explicit FDN profile with invalid-ADN access',
                        'base_sha256': hashlib.sha256(base).hexdigest(),
                        'input_sha256': hashlib.sha256(before).hexdigest(), 'handset_storage_changed': False}
            if fixture != expected or (run / 'initial_sim_card.bin').read_bytes() != before:
                raise ValueError('explicit external FDN card input differs')
        _, trace = check_native(run, (run / 'initial_eeprom.bin').read_bytes(), through=59, minimum_records=1)
        if (run / 'nvram/nsk1r4t/sim_card').read_bytes() != expected_validity_card(before, phase):
            raise ValueError('FDN changed unrelated card contents')
        check_trace(trace, phase)
        frame_check(run / 'snap/nsk1_fdn_final.png', ON_FRAME if phase in (0, 1) else OFF_FRAME)
        previous = run


def main():
    parser = argparse.ArgumentParser(description=__doc__)
    for name in ('source', 'on', 'on_cold', 'off', 'off_cold'):
        parser.add_argument(name, type=Path)
    args = parser.parse_args()
    try:
        check_runs(args.source, [args.on, args.on_cold, args.off, args.off_cold])
    except (ValueError, OSError, KeyError) as error:
        print(f'NSK-1 FDN validity failed: {error}', file=sys.stderr)
        return 1
    print('NSK-1 physical FDN On/Off and cold identity handshake PASS')
    return 0


if __name__ == '__main__':
    raise SystemExit(main())
