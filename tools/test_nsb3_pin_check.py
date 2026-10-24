import hashlib
import json
from pathlib import Path
import tempfile
import unittest
from unittest.mock import patch

from tools import nsb3_nominal_input
from tools.nsb3_pin_check import check_card, check_trace
from tools.make_sim_card_profile import make_current_profile
from tools.sim_security_trace_check import ATTEMPTS_OFFSET, padded_pin


def trace(correct):
    digits = '1234' if correct else '4321'
    keys = ['Keypad ' + digit for digit in digits] + ['Left Softkey / Menu']
    lines = [f'nsb3_pin_key: correct={int(correct)} key={key} down={state} t=12.0 physical_only=1'
             for key in keys for state in (1, 0)]
    status, attempts = ('9000', 3) if correct else ('9804', 2)
    lines.append(f'SIM status ins=20 sw={status} chv={attempts}/3 puk=10/10 enabled=1')
    tx = bytes.fromhex('ff00ffa020000108') + padded_pin(digits)
    lines += [f'nsb3_sim_wire: direction=TX data={byte:02x} t=14.0 observer_writes=0' for byte in tx]
    if correct:
        lines += ['nsb3_phonecode_matched_branch: retry=00 observer_writes=0',
                  'nsb3_phonecode_menu: down=1 physical_only=1', 'nsb3_phonecode_menu: down=0 physical_only=1']
        lines += [f'nsb3_phonecode_key: name=Keypad {digit} down={state}' for digit in '12345' for state in (1, 0)]
    return '\n'.join(lines)


class Nsb3PinTest(unittest.TestCase):
    def test_exhaustion_uses_persistent_attempts_and_terminal_status(self):
        for before in (2, 1):
            status = '9840' if before == 1 else '9804'
            text = trace(False).replace('sw=9804 chv=2/3', f'sw={status} chv={before - 1}/3')
            check_trace(text, False, before)
            with self.assertRaises(ValueError):
                check_trace(text, False)
        with self.assertRaises(ValueError):
            check_trace(trace(False), False, 0)

    def test_status_apdu_and_physical_contacts_required(self):
        for correct in (False, True):
            text = trace(correct)
            check_trace(text, correct)
            for bad in (text.replace('physical_only=1', 'physical_only=0'),
                        text.replace('ins=20', 'ins=24'), text.rsplit('\n', 1)[0],
                        text.replace('data=31 t=', 'data=35 t=')):
                with self.assertRaises(ValueError):
                    check_trace(bad, correct)

    def test_pin_acceptance_does_not_replace_phone_code_acceptance(self):
        with self.assertRaises(ValueError):
            check_trace(trace(True).replace('matched_branch', 'unmatched'), True)

    def test_whole_card_counter_transition(self):
        source = bytearray(make_current_profile(True))
        check_card(source, 3)
        source[ATTEMPTS_OFFSET] = 2
        check_card(source, 2)
        source[0] ^= 1
        with self.assertRaises(ValueError):
            check_card(source, 2)

    def test_preparer_seeds_generated_card_and_requires_insertion(self):
        image = b'own fixture image'
        with tempfile.TemporaryDirectory() as directory, patch.object(
                nsb3_nominal_input, 'IMAGE_SHA256', hashlib.sha256(image).hexdigest()):
            root = Path(directory)
            with self.assertRaises(ValueError):
                nsb3_nominal_input.prepare(image, root / 'bad', pin_enabled=True)
            self.assertFalse((root / 'bad').exists())
            target = root / 'good'
            nsb3_nominal_input.prepare(image, target, insert_sim=True, pin_enabled=True)
            card = make_current_profile(True)
            self.assertEqual((target / 'initial_sim_card.bin').read_bytes(), card)
            self.assertEqual((target / 'nvram/nsb3r4t/sim_card').read_bytes(), card)
            metadata = json.loads((target / 'input.json').read_text())
            self.assertTrue(metadata['laboratory_pin_enabled'])
            self.assertEqual(metadata['initial_sim_card_sha256'], hashlib.sha256(card).hexdigest())


if __name__ == '__main__':
    unittest.main()
