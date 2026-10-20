"""Qualify own physical NSK-3 FDN cancellation, erasure and cold empty search."""
import argparse
from pathlib import Path
import sys

if __package__ in (None, ''):
    sys.path.insert(0, str(Path(__file__).resolve().parents[1]))
from tools.nsk3_fdn_record_check import check_trace, RECORD
from tools.nsk3_static_check import inspect
from tools.nsk1_phonebook_roundtrip import frame_check
from tools.sim_fdn_profile import EXTENDED_LENGTH


def erased_card(before):
    record = bytearray(RECORD)
    record[22] = 0xf6
    if len(before) != EXTENDED_LENGTH or before[3578:3610] != record:
        raise ValueError('edited A/12346 FDN entry required')
    result = bytearray(before)
    result[3578:3610] = bytes([255]) * 32
    return bytes(result)


def main():
    parser = argparse.ArgumentParser(description=__doc__)
    for name in ('source', 'seed', 'cancel', 'erase', 'cold'):
        parser.add_argument(name, type=Path)
    args = parser.parse_args()
    try:
        _, flash = inspect(args.source, 'boot4')
        before = (args.seed / 'nvram/nsk3r4t/sim_card').read_bytes()
        erased = erased_card(before)
        for run, mode in ((args.cancel, 'cancel'), (args.erase, 'erase'), (args.cold, 'cold')):
            storage = run / 'nvram/nsk3r4t'
            previous = args.erase if mode == 'cold' else args.seed
            if (run / 'initial_sim_card.bin').read_bytes() != (erased if mode == 'cold' else before):
                raise ValueError('erase card input differs')
            if (run / 'initial_eeprom.bin').read_bytes() != (previous / 'nvram/nsk3r4t/eeprom').read_bytes():
                raise ValueError('erase EEPROM link differs')
            if (storage / 'sim_card').read_bytes() != (before if mode == 'cancel' else erased):
                raise ValueError('whole-card erase/cancel result differs')
            if (storage / 'flash').read_bytes() != flash:
                raise ValueError('own flash changed')
            check_trace((run / 'error.log').read_text(), 'empty' if mode == 'cold' else mode)
        frame_check(args.cold / 'snap/nsk3_fdn_read_26.png', EMPTY_FRAME)
    except (ValueError, OSError) as error:
        print(f'NSK-3 FDN erase failed: {error}', file=sys.stderr)
        return 1
    print('NSK-3 physical FDN cancel/erase and unchanged cold empty search PASS')
    return 0


EMPTY_FRAME = 'ad7a1d617d2b9646c17bdcddec9e3a313dc2acc9932ac6e24c455beb33a8267d'

if __name__ == '__main__':
    raise SystemExit(main())
