"""Qualify physical NSK-3 FDN changes and full-card cold retention."""
import argparse
from pathlib import Path
import re
import sys

if __package__ in (None, ''):
    sys.path.insert(0, str(Path(__file__).resolve().parents[1]))
from tools.nsk3_security_check import check_trace as check_phone
from tools.nsk3_static_check import inspect
from tools.nsk3_boot_check import check_upload_handoffs
from tools.sim_fdn_protocol_check import check_phase
from tools.sim_fdn_profile import EXTENDED_LENGTH


def check_trace(trace, phase):
    keys = (0x19, 4, 5, 3)
    if phase % 2 == 0:
        keys += (0x19, 0x17 if phase == 0 else 0x18, 0x19, 5, 6, 7, 8, 0x19)
    check_phone(trace, True, keys)
    contacts = re.findall(r'nsk3_pin_contact: correct=1 key=([0-9a-f]+) down=([01]) ', trace)
    expected = [(f'{key:02x}', down) for key in (1, 2, 3, 4, 0x19) for down in ('1', '0')]
    if contacts != expected:
        raise ValueError('physical startup PIN contacts differ')
    if trace.index('SIM status ins=20 sw=9000') > trace.index('nsk3_security_submit:'):
        raise ValueError('phone submission preceded SIM authentication')
    application = list(re.finditer(r'nsk3_security_contact: key=[0-9a-f]+ down=1 ', trace))
    if application[6].start() < trace.index('nsk3_security_accepted:'):
        raise ValueError('application input preceded phone authentication')
    check_phase(trace, phase, 'nsk3_sim_tx')
    check_upload_handoffs(trace, 1)


def check_runs(source, runs):
    if len(runs) != 4:
        raise ValueError('four linked FDN phases required')
    _, flash = inspect(source, 'boot4')
    previous = None
    for phase, run in enumerate(runs):
        storage = run / 'nvram/nsk3r4t'
        before = (run / 'initial_sim_card.bin').read_bytes()
        if len(before) != EXTENDED_LENGTH or before[3570:3575] != b'FDN\x01\x01':
            raise ValueError('extended FDN card required')
        if before[3575] != (1 if phase in (0, 3) else 0):
            raise ValueError('initial ADN validity differs')
        expected = bytearray(before)
        if phase in (0, 2):
            expected[3575] = 0 if phase == 0 else 1
        if (storage / 'sim_card').read_bytes() != expected:
            raise ValueError('whole-card transition differs')
        if previous is not None:
            if before != (previous / 'nvram/nsk3r4t/sim_card').read_bytes():
                raise ValueError('cold card link differs')
            if (run / 'initial_eeprom.bin').read_bytes() != (previous / 'nvram/nsk3r4t/eeprom').read_bytes():
                raise ValueError('cold EEPROM link differs')
        if (storage / 'flash').read_bytes() != flash:
            raise ValueError('own flash changed')
        check_trace((run / 'error.log').read_text(), phase)
        previous = run


def main():
    parser = argparse.ArgumentParser(description=__doc__)
    parser.add_argument('source', type=Path)
    for name in ('on', 'on_cold', 'off', 'off_cold'):
        parser.add_argument(name, type=Path)
    args = parser.parse_args()
    try:
        check_runs(args.source, (args.on, args.on_cold, args.off, args.off_cold))
    except (OSError, ValueError) as error:
        print(f'NSK-3 FDN cycle failed: {error}', file=sys.stderr)
        return 1
    print('NSK-3 physical FDN activation/deactivation and cold card retention PASS; cold menu highlight is not a state oracle')
    return 0


if __name__ == '__main__':
    raise SystemExit(main())
