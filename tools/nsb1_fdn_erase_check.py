"""Own physical NSB-1 FDN erase/cancel and unchanged cold empty search."""
import argparse
import json
from pathlib import Path
import sys

if __package__ in (None, ''):
    sys.path.insert(0, str(Path(__file__).resolve().parents[1]))
from tools.nsb1_rom4_compare import check_cold_link, check_card_run, check_frame
from tools.nsb1_fdn_record_check import check_trace, RECORD
from tools.sim_fdn_profile import EXTENDED_LENGTH


def erased_card(before):
    record = bytearray(RECORD)
    record[22] = 0xf6
    if len(before) != EXTENDED_LENGTH or before[3575] != 1 or before[3578:3610] != record:
        raise ValueError('edited FDN-Off A/12346 record required')
    after = bytearray(before)
    after[3578:3610] = bytes([255]) * 32
    return bytes(after)


def main():
    parser = argparse.ArgumentParser(description=__doc__)
    for name in ('seed', 'erase', 'cancel', 'cold'):
        parser.add_argument(name, type=Path)
    args = parser.parse_args()
    try:
        before = (args.seed / 'nvram/nsb1r4t/sim_card').read_bytes()
        erased = erased_card(before)
        for run, mode in ((args.erase, 'erase'), (args.cancel, 'cancel'), (args.cold, 'empty')):
            check_cold_link(run, args.erase if mode == 'empty' else args.seed)
            code = json.loads((run / 'exit_status.json').read_text())['returncode']
            trace, card = check_card_run(run, code, 50 if mode == 'empty' else 70)
            if card != (before if mode == 'cancel' else erased):
                raise ValueError('whole-card erase/cancel result differs')
            check_trace(trace, mode)
        check_frame(args.erase / 'snap/nsb1_fdn_61.png', CONFIRM_FRAME)
        check_frame(args.cancel / 'snap/nsb1_fdn_61.png', CONFIRM_FRAME)
        check_frame(args.erase / 'snap/nsb1_fdn_63.png', ERASED_FRAME)
        check_frame(args.cold / 'snap/nsb1_fdn_45.png', EMPTY_FRAME)
    except (ValueError, OSError, KeyError) as error:
        print(f'NSB-1 FDN erase failed: {error}', file=sys.stderr)
        return 1
    print('NSB-1 physical FDN erase/cancel and unchanged cold empty search PASS')
    return 0


CONFIRM_FRAME = '681a7cd63a5fa836670be4b04fe262a02860b2572cb30330bff5fb5d62dc62d9'
ERASED_FRAME = '922da30495f7290e0ab6f9b128f85f823ee66274495945c30365d8b6497e74d8'
EMPTY_FRAME = 'ad7a1d617d2b9646c17bdcddec9e3a313dc2acc9932ac6e24c455beb33a8267d'

if __name__ == '__main__':
    raise SystemExit(main())
