"""Own NSK-3 local FDN rejection contrast, not connected-call acceptance."""
import argparse
from pathlib import Path
import re
import sys

if __package__ in (None, ''):
    sys.path.insert(0, str(Path(__file__).resolve().parents[1]))
from tools.nsk3_fdn_check import check_trace as check_toggle
from tools.nsk3_security_check import check_trace as check_phone
from tools.nsk3_boot_check import check_upload_handoffs
from tools.nsk3_static_check import inspect
from tools.nsk3_fdn_erase_check import erased_card
from tools.sim_fdn_protocol_check import check_phase
from tools.nsk1_phonebook_roundtrip import frame_check

IDLE = 'fe62208ac0770e78212398ac8c58a352c2955aa877714560005f042cf7056ff3'
DENIED = 'f77c80d0e7545bd4d1c233f7d3564f9bbed1704ed5ee0362f25fb9b724294f3d'


def check_dial(trace, blocked, enabled):
    keys = (5, 4, 3, 2, 1, 0x0e) if blocked else (1, 2, 3, 4, 6, 0x0e)
    check_phone(trace, True, keys)
    contacts = re.findall(r'nsk3_pin_contact: correct=1 key=([0-9a-f]+) down=([01]) ', trace)
    if contacts != [(f'{key:02x}', down) for key in (1, 2, 3, 4, 0x19) for down in ('1', '0')]:
        raise ValueError('physical startup PIN differs')
    if trace.index('SIM status ins=20 sw=9000') > trace.index('nsk3_security_submit:'):
        raise ValueError('SIM/phone authentication order differs')
    application = list(re.finditer(r'nsk3_security_contact: key=[0-9a-f]+ down=1 ', trace))
    if application[6].start() < trace.index('nsk3_security_accepted:'):
        raise ValueError('dial preceded phone acceptance')
    check_phase(trace, 1 if enabled else 3, 'nsk3_sim_tx')
    check_upload_handoffs(trace, 1)


def main():
    parser = argparse.ArgumentParser(description=__doc__)
    for name in ('source', 'seed', 'enabled', 'listed', 'unlisted', 'disabled', 'unlisted_off'):
        parser.add_argument(name, type=Path)
    args = parser.parse_args()
    try:
        _, flash = inspect(args.source, 'boot4')
        original = (args.seed / 'nvram/nsk3r4t/sim_card').read_bytes()
        erased_card(original)  # Require the independently saved A/12346 record.
        if original[3575] != 1:
            raise ValueError('FDN-Off seed required')
        active = bytearray(original)
        active[3575] = 0
        for run, previous, before, after, phase in (
                (args.enabled, args.seed, original, active, 0),
                (args.disabled, args.enabled, active, original, 2)):
            storage = run / 'nvram/nsk3r4t'
            if (run / 'initial_sim_card.bin').read_bytes() != before or (storage / 'sim_card').read_bytes() != after:
                raise ValueError('toggle whole-card result differs')
            if (run / 'initial_eeprom.bin').read_bytes() != (previous / 'nvram/nsk3r4t/eeprom').read_bytes():
                raise ValueError('toggle EEPROM link differs')
            if (storage / 'flash').read_bytes() != flash:
                raise ValueError('toggle flash changed')
            check_toggle((run / 'error.log').read_text(), phase)
        for run, blocked, enabled in ((args.listed, False, True), (args.unlisted, True, True), (args.unlisted_off, True, False)):
            previous = args.enabled if enabled else args.disabled
            storage = run / 'nvram/nsk3r4t'
            expected = active if enabled else original
            if (run / 'initial_sim_card.bin').read_bytes() != expected or (storage / 'sim_card').read_bytes() != expected:
                raise ValueError('dial changed card or used different input')
            if (run / 'initial_eeprom.bin').read_bytes() != (previous / 'nvram/nsk3r4t/eeprom').read_bytes():
                raise ValueError('dial EEPROM link differs')
            if (storage / 'flash').read_bytes() != flash:
                raise ValueError('dial changed flash')
            check_dial((run / 'error.log').read_text(), blocked, enabled)
            frame_check(run / 'snap/nsk3_fdn_dial_26.png', DENIED if blocked and enabled else IDLE)
    except (ValueError, OSError) as error:
        print(f'NSK-3 local FDN contrast failed: {error}', file=sys.stderr)
        return 1
    print('NSK-3 local FDN rejection contrast PASS; no Calling/connected-call acceptance')
    return 0


if __name__ == '__main__':
    raise SystemExit(main())
