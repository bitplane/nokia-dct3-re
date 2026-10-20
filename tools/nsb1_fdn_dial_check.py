"""NSB-1 local FDN permission contrast; not connected-call acceptance."""
import argparse
import json
from pathlib import Path
import re
import sys

if __package__ in (None, ''):
    sys.path.insert(0, str(Path(__file__).resolve().parents[1]))
from tools.nsb1_rom4_compare import check_cold_link, check_card_run, check_frame
from tools.nsb1_pin_roundtrip import check_contacts
from tools.nsb1_fdn_check import check_trace as check_toggle, expected_card
from tools.nsb1_fdn_record_check import RECORD
from tools.sim_fdn_protocol_check import check_phase

PROGRESS = 'ec3a134720f225da5cd966a7f3b7cb349f16f177faa76f06b56cfe740b831331'
DENIED = 'f77c80d0e7545bd4d1c233f7d3564f9bbed1704ed5ee0362f25fb9b724294f3d'
LISTED_DIGITS = 'e54610fe6e16531ffb38c8c4fc71b6ded3d0b068a04eda42b69833ab9255a93c'
UNLISTED_DIGITS = '1eb44681587ee7792fe98249d5a1617b24ed205a4f9d2002358848f6148f9426'


def check_dial(trace, enabled):
    check_contacts(trace, 'pin', 5)
    check_contacts(trace, 'phone', 6)
    contacts = re.findall(r'nsb1_fdn_contact: index=(\d+) down=([01]) ', trace)
    if contacts != [(str(i), down) for i in range(1, 7) for down in ('1', '0')]:
        raise ValueError('physical dial contacts differ')
    if trace.index('SIM status ins=20 sw=9000') > trace.index('nsb1_pin_contact: phase=phone index=1 down=1 '):
        raise ValueError('phone input preceded SIM authentication')
    if trace.index('nsb1_fdn_contact: index=1 down=1 ') < trace.index('nsb1_pin_contact: phase=phone index=6 down=0 '):
        raise ValueError('dial preceded phone submission')
    check_phase(trace, 1 if enabled else 3, 'nsb1_sim_tx')


def main():
    parser = argparse.ArgumentParser(description=__doc__)
    for name in ('seed', 'enabled', 'listed', 'unlisted', 'disabled', 'unlisted_off'):
        parser.add_argument(name, type=Path)
    args = parser.parse_args()
    try:
        before = (args.seed / 'nvram/nsb1r4t/sim_card').read_bytes()
        record = bytearray(RECORD)
        record[22] = 0xf6
        if len(before) != 3898 or before[3575] != 1 or before[3578:3610] != record:
            raise ValueError('saved FDN-Off A/12346 seed required')
        active = expected_card(before, 0)
        for run, source, expected, seconds, phase in (
                (args.enabled, args.seed, active, 56, 0),
                (args.disabled, args.enabled, before, 56, 2),
                (args.listed, args.enabled, active, 46, 1),
                (args.unlisted, args.enabled, active, 46, 1),
                (args.unlisted_off, args.disabled, before, 46, 3)):
            check_cold_link(run, source)
            code = json.loads((run / 'exit_status.json').read_text())['returncode']
            trace, card = check_card_run(run, code, seconds)
            if card != expected:
                raise ValueError('whole-card result differs')
            if phase in (0, 2):
                check_toggle(trace, phase)
            else:
                check_dial(trace, phase == 1)
                check_frame(run / 'snap/nsb1_fdn_35.png', LISTED_DIGITS if run == args.listed else UNLISTED_DIGITS)
                check_frame(run / 'snap/nsb1_fdn_37.png', DENIED if run == args.unlisted else PROGRESS)
    except (ValueError, OSError, KeyError) as error:
        print(f'NSB-1 FDN contrast failed: {error}', file=sys.stderr)
        return 1
    print('NSB-1 local FDN permission contrast PASS; no registration/connected-call claim')
    return 0


if __name__ == '__main__':
    raise SystemExit(main())
