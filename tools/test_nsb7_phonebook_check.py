import unittest
from unittest.mock import patch
from tools.nsb7_phonebook_check import check_pin_card
from tools.nsb7_pin_check import serialized_profile
from tools.nsb7_pin_registration_check import LOCI
from tools.nsb7_phonebook_check import check_trace, SAVE_KEYS, READ_KEYS, HOST_KEYS
from tools.nsk3_phonebook_check import expected_record
from tools.test_nsb7_security_check import trace as security_trace


def trace(readback=False):
    rows = [security_trace()]
    for key in READ_KEYS if readback else SAVE_KEYS:
        rows += [f'nsb7_menu_input: key={HOST_KEYS[key]} state=1 t=26',
                 f'nsb7_key_decode: key={key:02x} observer_writes=0',
                 f'nsb7_menu_input: key={HOST_KEYS[key]} state=0 t=27']
    tx = bytes.fromhex('ff00ffa0a40000026f3a')
    tx += bytes.fromhex('a0b2010420') if readback else bytes.fromhex('a0dc010420') + expected_record()
    rows += [f'nsb7_sim_tx: index={i} value={value:02x}' for i, value in enumerate(tx, 1)]
    return '\n'.join(rows)


class PhonebookCheckTests(unittest.TestCase):
    def test_pin_whole_card_contract(self):
        initial = bytearray(serialized_profile())
        initial[1600:1611] = LOCI
        saved = bytearray(initial)
        saved[:32] = expected_record()
        check_pin_card(bytes(initial), bytes(saved))
        for offset in (32, 1600, 3484, 3516, 3520):
            changed = bytearray(saved)
            changed[offset] ^= 1
            with self.subTest(offset=offset), self.assertRaises(ValueError):
                check_pin_card(bytes(initial), bytes(changed))

    def test_pin_authentication_precedes_phonebook(self):
        text = trace()
        with patch('tools.nsb7_pin_services_check.check_authentication') as authentication:
            check_trace(text, pin=True)
            authentication.assert_called_once_with(text.split('nsb7_menu_input:', 1)[0])
        with self.assertRaises(ValueError):
            check_trace(text, pin=True)

    def test_complete_save_and_read(self):
        check_trace(trace())
        check_trace(trace(True), True)

    def test_missing_contacts_decode_and_wire(self):
        for line in trace().splitlines()[len(security_trace().splitlines()):]:
            with self.subTest(line=line), self.assertRaises(ValueError):
                text = trace()
                index = text.rfind(line)
                check_trace(text[:index] + text[index+len(line):])

    def test_rejects_wrong_phase_and_security_failure(self):
        with self.assertRaises(ValueError):
            check_trace(trace(True))
        with self.assertRaises(ValueError):
            check_trace(trace().replace('match=1', 'match=0'))
        with self.assertRaises(ValueError):
            check_trace(trace(True).replace('value=b2', 'value=dc'), True)


if __name__ == '__main__':
    unittest.main()
