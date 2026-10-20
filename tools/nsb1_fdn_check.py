"""Own physical NSB-1 FDN On/Off, identity handshake and linked cold storage."""
import argparse
import hashlib
import json
from pathlib import Path
import re
import sys

if __package__ in (None, ''):
    sys.path.insert(0, str(Path(__file__).resolve().parents[1]))
from tools.nsb1_rom4_compare import check_cold_link, check_card_run, check_frame
from tools.nsb1_pin_roundtrip import check_contacts, ACCEPTED_RGB_SHA256
from tools.sim_fdn_profile import extend, EXTENDED_LENGTH, expected_validity_card as expected_card
from tools.sim_fdn_protocol_check import check_phase


def check_trace(trace, phase):
    check_contacts(trace, 'pin', 5)
    check_contacts(trace, 'phone', 6)
    contacts = re.findall(r'nsb1_fdn_contact: index=(\d+) down=([01]) ', trace)
    if contacts != [(str(i), down) for i in range(1, (5 if phase % 2 else 14)) for down in ('1', '0')]:
        raise ValueError('physical FDN contacts differ')
    if trace.index('SIM status ins=20 sw=9000') > trace.index('nsb1_pin_contact: phase=phone index=1 down=1 '):
        raise ValueError('phone input preceded SIM authentication')
    if trace.index('nsb1_fdn_contact: index=1 down=1 ') < trace.index('nsb1_pin_contact: phase=phone index=6 down=0 '):
        raise ValueError('FDN input preceded phone submission')
    check_phase(trace, phase, 'nsb1_sim_tx')


def main():
    parser = argparse.ArgumentParser(description=__doc__)
    for name in ('seed', 'on', 'on_cold', 'off', 'off_cold'):
        parser.add_argument(name, type=Path)
    args = parser.parse_args()
    try:
        base = (args.seed / 'nvram/nsb1r4t/sim_card').read_bytes()
        previous = args.seed
        for phase, run in enumerate((args.on, args.on_cold, args.off, args.off_cold)):
            check_cold_link(run, previous)
            before = extend(base, True) if phase == 0 else (previous / 'nvram/nsb1r4t/sim_card').read_bytes()
            if phase == 0:
                if (run / 'initial_sim_card.bin').read_bytes() != before:
                    raise ValueError('explicit FDN card input differs')
                fixture = json.loads((run / 'input_manifest.json').read_text()).get('card_fixture')
                if fixture != {'kind': 'explicit FDN profile with invalid-ADN access',
                               'base_sha256': hashlib.sha256(base).hexdigest(),
                               'input_sha256': hashlib.sha256(before).hexdigest(),
                               'handset_storage_changed': False}:
                    raise ValueError('explicit card-profile provenance differs')
            code = json.loads((run / 'exit_status.json').read_text())['returncode']
            trace, card = check_card_run(run, code, 40 if phase % 2 else 56)
            if card != expected_card(before, phase):
                raise ValueError('whole-card FDN result differs')
            check_trace(trace, phase)
            check_frame(run / 'snap/nsb1_pin_reload_4.png', ACCEPTED_RGB_SHA256)
            check_frame(run / 'snap' / f'nsb1_fdn_{35 if phase % 2 else 53}.png', FRAMES[phase])
            previous = run
    except (ValueError, OSError, KeyError) as error:
        print(f'NSB-1 FDN cycle failed: {error}', file=sys.stderr)
        return 1
    print('NSB-1 physical FDN activation/cold handshake/deactivation PASS; cold option highlight is not a state oracle')
    return 0


FRAMES = ('abd36033adcc553c684ff227797d967d7f5adecee927fe164ee7b2d8f395790e',
          '4e90b93c811a22702952b08daafadc2352bf1ec24afcf41b4467d26af64ec28d',
          '8e227a0ebf24dd9d346c96a11995ecc2b9468e05069090feab1199bebf5f85cb',
          '211448dc6a8c16341112e19084fb1a12db3339974b93419481b4fc2c5f02afed')

if __name__ == '__main__':
    raise SystemExit(main())
