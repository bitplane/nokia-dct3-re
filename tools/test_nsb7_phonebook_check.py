import unittest
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
