"""PUK physical sequence, APDU and complete cold-card chain tests."""
import unittest

from tools.nsb7_puk_check import check_puk, check_cards, card_state
from tools.make_sim_card_profile import make_profile


def fixture(wrong):
    puk = '12345679' if wrong else '12345678'
    lines = []
    for phase, digits, start in [('puk', puk, 8), ('new', '4321', 19), ('confirm', '4321', 26)]:
        for index, key in enumerate(list(digits) + ['Menu']):
            code = 0x19 if key == 'Menu' else int(key)
            lines += [f'nsb7_puk_input: wrong={int(wrong)} phase={phase} key={key} state=1 t={start+index}.000000000',
                      f'nsb7_key_decode: key={code:02x} observer_writes=0',
                      f'nsb7_puk_input: wrong={int(wrong)} phase={phase} key={key} state=0 t={start+index}.200000000']
    apdu = bytes.fromhex('a02c000010') + puk.encode() + b'4321\xff\xff\xff\xff'
    lines += [f'nsb7_sim_tx: index={i} value={byte:02x}' for i, byte in enumerate(apdu, 1)]
    lines += [f'SIM status ins=2c sw={"9804" if wrong else "9000"} chv={0 if wrong else 3}/3 puk={9 if wrong else 10}/10 enabled=1',
              'nsb7_puk_input: complete']
    return '\n'.join(lines)


def pairs():
    return [(make_profile(True), card_state(2)), (card_state(2), card_state(1)),
            (card_state(1), card_state(0)), (card_state(0), card_state(0, 9)),
            (card_state(0, 9), card_state(3, pin='4321')),
            (card_state(3, pin='4321'), card_state(3, pin='4321'))]


class PukCheckTest(unittest.TestCase):
    def test_failed_and_successful_puk(self):
        check_puk(fixture(True), True)
        check_puk(fixture(False), False)

    def test_every_failed_puk_retry_and_permanent_status(self):
        for remaining in range(10):
            status = '9840' if remaining == 0 else '9804'
            text = fixture(True).replace('puk=9/10', f'puk={remaining}/10').replace('sw=9804', 'sw='+status)
            with self.subTest(remaining=remaining):
                check_puk(text, True, remaining=remaining)
                with self.assertRaises(ValueError):
                    check_puk(text.replace('sw='+status, 'sw=9000'), True, remaining=remaining)

    def test_permanent_requires_blocked_status(self):
        with self.assertRaises(ValueError):
            check_puk(fixture(True).replace('puk=9/10', 'puk=0/10'), True, remaining=0)

    def test_retry_domain(self):
        for remaining in (-1, 10):
            with self.subTest(remaining=remaining), self.assertRaises(ValueError):
                check_puk(fixture(True), True, remaining=remaining)

    def test_corrupt_or_missing_evidence(self):
        for old, new in [('phase=confirm', 'phase=missing'), ('key=19 observer_writes=0', 'key=18 observer_writes=0'),
                         ('sw=9804', 'sw=9000'), ('puk=9/10', 'puk=10/10'),
                         ('value=39', 'value=38'), ('nsb7_puk_input: complete', '')]:
            with self.subTest(old=old), self.assertRaises(ValueError):
                check_puk(fixture(True).replace(old, new), True)

    def test_unrelated_credential_mutation(self):
        with self.assertRaises(ValueError):
            check_puk(fixture(False) + '\nSIM status ins=24 sw=9000 chv=3/3 puk=10/10 enabled=1', False)

    def test_complete_card_chain(self):
        check_cards(pairs())

    def test_lost_cold_puk_retry(self):
        chain = pairs()
        chain[4] = (card_state(0), chain[4][1])
        with self.assertRaises(ValueError):
            check_cards(chain)

    def test_unrelated_card_change(self):
        chain = pairs()
        initial, final = chain[4]
        chain[4] = (initial, b'\x00' + final[1:])
        with self.assertRaises(ValueError):
            check_cards(chain)

    def test_replacement_pin_not_retained(self):
        chain = pairs()
        chain[5] = (card_state(3), card_state(3))
        with self.assertRaises(ValueError):
            check_cards(chain)


if __name__ == '__main__':
    unittest.main()
