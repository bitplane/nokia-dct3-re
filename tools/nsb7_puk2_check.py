"""PIN2 exhaustion, failed PUK2, cold recovery and recovered PIN2 reuse."""
import argparse
from pathlib import Path
import sys

if __package__ in (None, ''):
    sys.path.insert(0, str(Path(__file__).resolve().parents[1]))
from tools.nsb7_radio_check import contract
from tools.nsb7_pin_change_check import check_change
from tools.nsb7_pin_check import serialized_profile, check_frame
from tools.nsb7_storage_check import inspect
from tools.sim_security_trace_check import CHV_OFFSET, ATTEMPTS_OFFSET, padded_pin


def card_state(retries, puk=10, pin='5678'):
    card = bytearray(serialized_profile())
    card[ATTEMPTS_OFFSET+1] = retries
    card[ATTEMPTS_OFFSET+3] = puk
    card[CHV_OFFSET+8:CHV_OFFSET+16] = padded_pin(pin)
    return bytes(card)


def check_cards(pairs):
    expected = [(card_state(3), card_state(2)), (card_state(2), card_state(1)),
                (card_state(1), card_state(0)), (card_state(0), card_state(0, 9)),
                (card_state(0, 9), card_state(3, pin='8765')),
                (card_state(3, pin='8765'), card_state(3))]
    if pairs != expected:
        raise ValueError('complete PIN2/PUK2 cold card chain differs')


def main():
    parser = argparse.ArgumentParser(description=__doc__)
    for name in ('image', 'first', 'second', 'blocked', 'wrong_puk', 'recovered', 'cold'):
        parser.add_argument(name, type=Path)
    args = parser.parse_args()
    runs = [args.first, args.second, args.blocked, args.wrong_puk, args.recovered, args.cold]
    try:
        contract(args.image.read_bytes())
        texts = [(run / 'error.log').read_text() for run in runs]
        for text, retries in zip(texts[:3], (2, 1, 0)):
            check_change(text, True, pin2=True, remaining=retries)
        check_change(texts[3], True, pin2=True, unblock=True)
        check_change(texts[4], False, pin2=True, unblock=True)
        check_change(texts[5], False, pin2=True, old='8765', new='5678')
        pairs = []
        for run in runs:
            inspect((run / 'nvram/nsb7hle/flash').read_bytes())
            pairs.append(((run / 'initial_sim_card.bin').read_bytes(),
                          (run / 'nvram/nsb7hle/sim_card').read_bytes()))
        check_cards(pairs)
        for run, name, expected in [
                (args.blocked, 'nsb7_change_56.png', '655f0c5941db535e41b3d2330aaef34670731021d3cb07d51cb94ea480db1025'),
                (args.wrong_puk, 'nsb7_change_60.png', '460778ecafd4fb9a23ec8c6632d3b4272b29c63c059b8c2eb764ad810f6c53c8'),
                (args.recovered, 'nsb7_change_60.png', '0aceecb50527060d630eaa09505dc46840ea90ece32feee32cc3fade8b475d92'),
                (args.cold, 'nsb7_change_56.png', '693089e1bfb5848a4d945b894d21369ebe025961fb846d092570126b3cb152c0')]:
            check_frame(run / 'snap' / name, expected)
    except (OSError, ValueError) as error:
        parser.exit(1, f'NSB-7 PUK2: FAIL: {error}\n')
    print('PASS: physical PIN2 exhaustion, isolated failed-PUK2 retry, cold recovery and recovered PIN2 reuse')


if __name__ == '__main__':
    main()
