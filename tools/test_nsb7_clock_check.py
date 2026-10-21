import unittest
from tools.nsb7_clock_check import check_counter, check_setting
from tools.test_nsb7_security_check import trace as security_trace


def trace():
    rows = [security_trace()]
    for key, when, decoded in [('Keypad 1', 53, 1), ('Keypad 3', 56, 3), ('Keypad 4', 59, 4),
                               ('Keypad 7', 62, 7), ('Left Softkey / Menu', 65, 0x19)]:
        rows += [f'nsb7_menu_input: key={key} state=1 t={when:.9f}',
                 f'nsb7_key_decode: key={decoded:02x} observer_writes=0',
                 f'nsb7_menu_input: key={key} state=0 t={when+0.2:.9f}']
    return '\n'.join(rows)


class ClockCheckTests(unittest.TestCase):
    def test_complete_setting(self):
        check_setting(trace())

    def test_missing_setting_evidence(self):
        for line in trace().splitlines()[-15:]:
            with self.subTest(line=line), self.assertRaises(ValueError):
                text = trace()
                index = text.rfind(line)
                check_setting(text[:index] + text[index+len(line):])

    def test_counter_contrast(self):
        backed = bytes((25, 47, 13, 0, 0, 30, 255, 80, 1))
        empty = bytes((25, 0, 12, 1, 0, 0, 0, 80, 0))
        check_counter(backed, True)
        check_counter(empty, False)
        for data, retained in ((backed, False), (empty, True), (backed[:-1], True)):
            with self.assertRaises(ValueError):
                check_counter(data, retained)


if __name__ == '__main__':
    unittest.main()
