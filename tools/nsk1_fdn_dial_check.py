"""Own NSK-1 local FDN permission contrast, not connected-call acceptance."""
import argparse
from pathlib import Path
import sys

if __package__ in (None, ''):
    sys.path.insert(0, str(Path(__file__).resolve().parents[1]))
from tools.nsk1_fdn_check import check_input, check_trace as check_toggle, ON_FRAME, OFF_FRAME
from tools.nsk1_pin2_change_check import check_cold_link
from tools.nsk1_fdn_record_check import erased_card, check_trace as check_record, EDITED_NUMBER
from tools.nsk1_security_compare import check_phase as check_native
from tools.nsk1_phonebook_roundtrip import frame_check
from tools.sim_fdn_profile import expected_validity_card
from tools.sim_fdn_protocol_check import check_phase
from tools.sim_security_trace_check import statuses

LISTED_KEYS = ['one','two','three','four','six','menu']
UNLISTED_KEYS = ['five','four','three','two','one','menu']
LISTED_DIGITS = '0ee0a05c22878d25fffa6ffec966895410b758beb8f289181bc5a1410a5e17b3'
UNLISTED_DIGITS = 'f028869cfe7199fa0880c2fdfdb00852273242b692d03329338a022eb2701b09'
PROGRESS = '2faa6cfc2b466241e37772b46e02786baa8cdb4a38ca9529e14c4a6f2bf23112'
DENIED = 'f77c80d0e7545bd4d1c233f7d3564f9bbed1704ed5ee0362f25fb9b724294f3d'


def check_dial(trace, unlisted, enabled):
    check_input(trace, UNLISTED_KEYS if unlisted else LISTED_KEYS)
    check_phase(trace, 1 if enabled else 3, 'nsk1_sim_tx')
    if any(statuses(trace, ins) for ins in (0xdc, 0xd6)):
        raise ValueError('dial attempt wrote a SIM file')


def main():
    parser = argparse.ArgumentParser(description=__doc__)
    for name in ('seed', 'enabled', 'listed', 'unlisted', 'disabled', 'unlisted_off'):
        parser.add_argument(name, type=Path)
    args = parser.parse_args()
    try:
        _, trace = check_native(args.seed, (args.seed / 'initial_eeprom.bin').read_bytes(), through=59, minimum_records=1)
        check_record(trace, 'cold')
        frame_check(args.seed / 'snap/nsk1_fdn_final.png', EDITED_NUMBER)
        before = (args.seed / 'nvram/nsk1r4t/sim_card').read_bytes()
        erased_card(before)  # Require the qualified A/12346 record, not a supplied list.
        active = expected_validity_card(before, 0)
        for run, source, expected, phase in ((args.enabled, args.seed, active, 0),
                                             (args.disabled, args.enabled, before, 2)):
            initial = check_cold_link(run, source)
            _, trace = check_native(run, (run / 'initial_eeprom.bin').read_bytes(), through=59, minimum_records=1)
            if expected_validity_card(initial, phase) != expected or (run / 'nvram/nsk1r4t/sim_card').read_bytes() != expected:
                raise ValueError('FDN toggle whole-card result differs')
            check_toggle(trace, phase)
            frame_check(run / 'snap/nsk1_fdn_final.png', ON_FRAME if phase == 0 else OFF_FRAME)
        for run, unlisted, enabled in ((args.listed, False, True), (args.unlisted, True, True), (args.unlisted_off, True, False)):
            initial = check_cold_link(run, args.enabled if enabled else args.disabled)
            expected = active if enabled else before
            _, trace = check_native(run, (run / 'initial_eeprom.bin').read_bytes(), through=59, minimum_records=1)
            if initial != expected or (run / 'nvram/nsk1r4t/sim_card').read_bytes() != expected:
                raise ValueError('dial changed or used different card contents')
            check_dial(trace, unlisted, enabled)
            frame_check(run / 'snap/nsk1_fdn_final.png', DENIED if unlisted and enabled else PROGRESS)
            frame_check(run / 'snap/nsk1_fdn_5.png', UNLISTED_DIGITS if unlisted else LISTED_DIGITS)
    except (ValueError, OSError, KeyError) as error:
        print(f'NSK-1 local FDN contrast failed: {error}', file=sys.stderr)
        return 1
    print('NSK-1 local FDN permission contrast PASS; no registration/connected-call claim')
    return 0


if __name__ == '__main__':
    raise SystemExit(main())
